#include "stdafx.h"

_NT_BEGIN

#include "tp.h"
#include "io.h"

#define _THREAD_POOL_BEGIN namespace CThreadPool {
#define _THREAD_POOL_END }

_THREAD_POOL_BEGIN

HANDLE m_hiocp;
LONG m_NumberOfThreads, m_dwRef = 1;

void AddRef()
{
	InterlockedIncrement(&m_dwRef);
}

void Release()
{
	if (!InterlockedDecrement(&m_dwRef))
	{
		if (m_hiocp)
		{
			NtClose(m_hiocp);
		}
	}
}

static OBJECT_ATTRIBUTES zoa = { sizeof(zoa) };

NTSTATUS AltBindIoCompletionCallback(HANDLE FileHandle, LPOVERLAPPED_COMPLETION_ROUTINE Function, ULONG /*Flags*/ )
{
	if (!Function) return STATUS_INVALID_PARAMETER_2;

	IO_STATUS_BLOCK iosb;

	FILE_COMPLETION_INFORMATION fci = { m_hiocp, Function };

	return NtSetInformationFile(FileHandle, &iosb, &fci, sizeof fci, FileCompletionInformation);
}

void Stop()
{
	fnSetIoCompletionCallback = (NTSTATUS (NTAPI *)(HANDLE , LPOVERLAPPED_COMPLETION_ROUTINE , ULONG ))RtlSetIoCompletionCallback;

	if (LONG n = m_NumberOfThreads)
	{
		HANDLE hiocp = m_hiocp;
		do 
		{
			ZwSetIoCompletion(hiocp, 0, 0, 0, 0);
		} while (--n);
	}

	Release();
}

BOOL Start()
{
	SYSTEM_BASIC_INFORMATION sbi;

	if (0 > NtQuerySystemInformation(SystemBasicInformation, &sbi, sizeof sbi, 0) ||
		0 >= sbi.NumberOfProcessors ||
		0 > ZwCreateIoCompletion(&m_hiocp, IO_COMPLETION_ALL_ACCESS, &zoa, sbi.NumberOfProcessors)) 
		return FALSE;

	do 
	{
		LdrAddRefDll(0, (HMODULE)&__ImageBase);
		AddRef();
		InterlockedIncrement(&m_NumberOfThreads);
		if (HANDLE hThread = CreateThread(0, 0, WorkThread, 0, 0, 0))
		{
			NtClose(hThread);
		}
		else
		{
			InterlockedDecrement(&m_NumberOfThreads);
			Release();
			FreeLibrary((HMODULE)&__ImageBase);
		}
	} while (--sbi.NumberOfProcessors);

	if (m_NumberOfThreads)
	{
		fnSetIoCompletionCallback = AltBindIoCompletionCallback;
	}

	return m_NumberOfThreads;
}

DWORD CALLBACK WorkThread(PVOID )
{
	IO_STATUS_BLOCK iosb;
	VOID (WINAPI *Function)(NTSTATUS Status, ULONG_PTR Information, PVOID Context);
	PVOID Context;
	HANDLE hiocp = m_hiocp;

	for (;;)
	{
		switch (ZwRemoveIoCompletion(hiocp, (void**)&Function, (void**)&Context, &iosb, 0))
		{
		case STATUS_SUCCESS:
			if (Function)
			{
				Function(iosb.Status, iosb.Information, Context);
				if (m_hiocp) continue;
			}
		default:
			InterlockedDecrement(&m_NumberOfThreads);
			Release();
			FreeLibraryAndExitThread((HMODULE)&__ImageBase, 0);
		}
	}
}

NTSTATUS Post(
			  VOID (WINAPI *Function)(NTSTATUS Status, ULONG_PTR Information, PVOID Context),
			  PVOID Context, 
			  NTSTATUS Status, 
			  ULONG_PTR Information)
{
	if (!Function)
	{
		return STATUS_INVALID_PARAMETER_1;
	}
	return ZwSetIoCompletion(m_hiocp, Function, Context, Status, Information);
}

_THREAD_POOL_END

_NT_END