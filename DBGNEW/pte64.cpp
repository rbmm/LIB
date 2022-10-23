#include "StdAfx.h"

_NT_BEGIN

#include "common.h"
#include "memdump.h"

#define EXTEND64(Addr) ((ULONG64)(LONG_PTR)(Addr))
//int OnCopyException(PVOID& p, PEXCEPTION_RECORD per)
//{
//	if (per->ExceptionCode == STATUS_ACCESS_VIOLATION && per->NumberParameters > 1 && !per->ExceptionInformation[0])
//	{
//		p = (PVOID)per->ExceptionInformation[1];
//	}
//
//	return EXCEPTION_EXECUTE_HANDLER;
//}

NTSTATUS DbgReadMemory(HANDLE ProcessHandle, PVOID BaseAddres, PVOID Buffer, SIZE_T BufferLength, PSIZE_T ReturnLength )
{
	SIZE_T cbCopy;

	if (!ReturnLength)
	{
		ReturnLength = &cbCopy;
	}

	*ReturnLength = 0;

	if (ProcessHandle != NtCurrentProcess() && (ULONG_PTR)ProcessHandle > MAXUSHORT)
	{
		return ((IMemoryDump*)ProcessHandle)->ReadVirtual(BaseAddres, Buffer, BufferLength, ReturnLength);
	}

	if ((INT_PTR)BaseAddres < 0)
	{
		if (g_hDrv)
		{
			IO_STATUS_BLOCK iosb;
			NTSTATUS status = ZwDeviceIoControlFile(g_hDrv, 0, 0, 0, &iosb, IOCTL_ReadMemory, &BaseAddres, sizeof(BaseAddres), Buffer, (DWORD)BufferLength);
			*ReturnLength = iosb.Information;
			return status;
		}
		else
		{
			*ReturnLength = 0;
			return STATUS_ACCESS_VIOLATION;
		}
	}
	//else if (ProcessHandle == NtCurrentProcess())
	//{
	//	__try 
	//	{
	//		memcpy(Buffer, BaseAddres, BufferLength);
	//		return 0;
	//	}
	//	__except(OnCopyException(Buffer, (GetExceptionInformation())->ExceptionRecord))
	//	{
	//		if (cbCopy = RtlPointerToOffset(BaseAddres, Buffer))
	//		{
	//			if (cbCopy < BufferLength)
	//			{
	//				*ReturnLength = cbCopy;

	//				return STATUS_PARTIAL_COPY;
	//			}
	//		}

	//		*ReturnLength = 0;
	//		return GetExceptionCode();
	//	}
	//}
	else 
	{
		return ZwReadVirtualMemory(ProcessHandle, BaseAddres, Buffer, BufferLength, ReturnLength);
	}
}

NTSTATUS SymReadMemory(ZDbgDoc* pDoc, PVOID BaseAddres, PVOID Buffer, SIZE_T BufferLength, PSIZE_T ReturnLength)
{
	return pDoc ? pDoc->Read(BaseAddres, Buffer, (DWORD)BufferLength, ReturnLength) : 
		DbgReadMemory(NtCurrentProcess(), BaseAddres, Buffer, BufferLength, ReturnLength);
}

NTSTATUS DoIoControl(ULONG code)
{
	IO_STATUS_BLOCK iosb;
	return g_hDrv ? ZwDeviceIoControlFile(g_hDrv, 0, 0, 0, &iosb, code, 0, 0, 0, 0) : STATUS_INVALID_HANDLE;
}

BOOL LoadDrv()
{
	STATIC_UNICODE_STRING(tkn, "\\Registry\\Machine\\System\\CurrentControlSet\\Services\\{FC81D8A3-6002-44bf-931A-352B95C4522F}");
	NTSTATUS status = ZwLoadDriver((PUNICODE_STRING)&tkn);

	if (0 > status && status != STATUS_IMAGE_ALREADY_LOADED)
	{
		return FALSE;
	}

	IO_STATUS_BLOCK iosb;
	STATIC_OBJECT_ATTRIBUTES(oa, "\\device\\69766781178D422cA183775611A8EE55");

	return 0 <= ZwOpenFile(&g_hDrv, SYNCHRONIZE, &oa, &iosb, FILE_SHARE_VALID_FLAGS, FILE_SYNCHRONOUS_IO_NONALERT);
}

_NT_END