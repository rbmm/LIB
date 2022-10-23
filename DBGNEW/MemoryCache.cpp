#include "StdAfx.h"

_NT_BEGIN

#include "MemoryCache.h"
#include "common.h"

void ZMemoryCache::Cleanup()
{
	if (_Buffer)
	{
		SIZE_T RegionSize = 0;
		if (0 <= ZwFreeVirtualMemory(NtCurrentProcess(), &_Buffer, &RegionSize, MEM_RELEASE))
		{
			_Buffer = 0;
		}
	}
}

ZMemoryCache::~ZMemoryCache()
{
	Cleanup();
}

ZMemoryCache::ZMemoryCache()
{
	_Buffer = 0, _index = 0;
	__stosp((PULONG_PTR)_RemoteAddresses, (ULONG_PTR)INVALID_HANDLE_VALUE, chunk_count);
}

void ZMemoryCache::Invalidate()
{
	if (_hProcess != NtCurrentProcess())
	{
		__stosp((PULONG_PTR)_RemoteAddresses, (ULONG_PTR)INVALID_HANDLE_VALUE, chunk_count);
	}
}

BOOL ZMemoryCache::Create()
{
	if (_hProcess == NtCurrentProcess())
	{
		return TRUE;
	}

	SIZE_T RegionSize = chunk_count * chunk_size;
	return 0 <= ZwAllocateVirtualMemory(NtCurrentProcess(), &_Buffer, 0, &RegionSize, MEM_COMMIT, PAGE_READWRITE);
}

void ZMemoryCache::WriteToCache(PVOID RemoteAddress, UCHAR c)
{
	PVOID pv = (PVOID)((ULONG_PTR)RemoteAddress & ~(chunk_size-1));

	if (void** ppv = findPVOID(chunk_count, _RemoteAddresses, pv))
	{
		CHUNK LocalAddress =  _Chunks + (DWORD)(ppv - _RemoteAddresses);
		DWORD delta = RtlPointerToOffset(pv, RemoteAddress);
		(*LocalAddress)[delta] = c;
	}
}

NTSTATUS ZMemoryCache::Write(PVOID RemoteAddress, UCHAR c)
{
	if ((INT_PTR)RemoteAddress < 0)
	{
		return STATUS_ACCESS_VIOLATION;
	}

	PVOID BaseRemoteAddress = RemoteAddress;
	SIZE_T ProtectSize = sizeof(c);

	MEMORY_BASIC_INFORMATION mbi;
	NTSTATUS status = ZwQueryVirtualMemory(_hProcess, RemoteAddress, MemoryBasicInformation, &mbi, sizeof(mbi), 0);

	if (0 > status)
	{
		return status;
	}

	if (mbi.State != MEM_COMMIT)
	{
		return STATUS_INVALID_PARAMETER;
	}

	ULONG Protect = PAGE_EXECUTE_READWRITE;

	switch (mbi.Protect & 0xff)
	{
	case PAGE_READONLY:
		Protect = PAGE_READWRITE;
	case PAGE_EXECUTE:
	case PAGE_EXECUTE_READ:
		BaseRemoteAddress = RemoteAddress;
		status = ZwProtectVirtualMemory(_hProcess, &BaseRemoteAddress, &ProtectSize, Protect, &mbi.Protect);
		break;
	case PAGE_READWRITE:
	case PAGE_WRITECOPY:
	case PAGE_EXECUTE_READWRITE:
	case PAGE_EXECUTE_WRITECOPY:
		Protect = mbi.Protect;
		break;
	default:
		return STATUS_INVALID_PARAMETER;
	}

	if (0 <= status)
	{
		if (_hProcess == NtCurrentProcess())
		{
			__try
			{
				*(PUCHAR)RemoteAddress = c;
				status = 0;
			}
			__except(EXCEPTION_EXECUTE_HANDLER)
			{
				status = GetExceptionCode();
			}
		}
		else if (0 <= (status = ZwWriteVirtualMemory(_hProcess, RemoteAddress, &c, sizeof(c), 0)))
		{
			WriteToCache(RemoteAddress, c);
		}
		if (Protect != mbi.Protect) ZwProtectVirtualMemory(_hProcess, &BaseRemoteAddress, &ProtectSize, mbi.Protect, &Protect);
	}

	return status;
}

NTSTATUS DbgReadMemory(HANDLE ProcessHandle, PVOID BaseAddres, PVOID Buffer, SIZE_T BufferLength, PSIZE_T ReturnLength = 0);

NTSTATUS ZMemoryCache::Read(PVOID RemoteAddress, PVOID buf, DWORD cb, PSIZE_T pcb)
{
	if (!cb) return STATUS_INVALID_PARAMETER;

	if (_hProcess == NtCurrentProcess() && (INT_PTR)RemoteAddress >= 0)
	{
		return DbgReadMemory(NtCurrentProcess(), RemoteAddress, buf, cb, pcb);
	}
	
	DWORD dw;
	SIZE_T cbReaded = 0;

	PBYTE BaseRemoteAddress = (PBYTE)((ULONG_PTR)RemoteAddress & ~(chunk_size-1));
	PVOID LocalAddress;
	DWORD delta = RtlPointerToOffset(BaseRemoteAddress, RemoteAddress);
	
	do 
	{
		dw = min(cb, chunk_size - delta);

		if (void** ppv = findPVOID(chunk_count, _RemoteAddresses, BaseRemoteAddress))
		{
			LocalAddress =  _Chunks + (DWORD)(ppv - _RemoteAddresses);
		}
		else
		{
			LONG index = _index++ & (chunk_count - 1);
			
			LocalAddress = _Chunks + index;

			if (0 > DbgReadMemory(_hProcess, BaseRemoteAddress, LocalAddress, chunk_size, 0))
			{
				_RemoteAddresses[index] = INVALID_HANDLE_VALUE;
				if (pcb)
				{
					*pcb = cbReaded;
				}
				return cbReaded ? STATUS_PARTIAL_COPY : STATUS_UNSUCCESSFUL;
			}

			_RemoteAddresses[index] = BaseRemoteAddress;
		}

		memcpy(buf, RtlOffsetToPointer(LocalAddress, delta), dw);

		delta = 0;
		cbReaded += dw;
		buf = RtlOffsetToPointer(buf, dw);
		BaseRemoteAddress += chunk_size;

	} while (cb -= dw);

	if (pcb)
	{
		*pcb = cbReaded;
	}

	return STATUS_SUCCESS;
}

_NT_END