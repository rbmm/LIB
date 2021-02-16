#include "StdAfx.h"

_NT_BEGIN

#include <iphlpapi.h>
#include <IPExport.h>
#include <icmpapi.h>
#include "icmp.h"

void ICMP::EchoRequestContext::OnApc(ULONG dwError, ULONG ReplySize)
{
	pObj->OnReply(dwError, (PICMP_ECHO_REPLY)ReplyBuffer, ReplySize, ctx);
	delete this;
}

void WINAPI ICMP::EchoRequestContext::sOnApc(PVOID This, PIO_STATUS_BLOCK piosb, ULONG )
{
	reinterpret_cast<EchoRequestContext*>(This)->OnApc(RtlNtStatusToDosError(piosb->Status), (ULONG)piosb->Information);
}

void ICMP::IOCompletionRoutine(CDataPacket* , ULONG , NTSTATUS , ULONG_PTR , PVOID )
{
	__debugbreak();
}

void ICMP::CloseObjectHandle(HANDLE hFile)
{
	IcmpCloseHandle(hFile);
}

ULONG ICMP::Create()
{
	HANDLE hFile = IcmpCreateFile();

	if (hFile == INVALID_HANDLE_VALUE) 
	{
		return GetLastError();
	}

	Assign(hFile);

	return NOERROR;
}

void ICMP::SendEcho(
						 IPAddr DestinationAddress, 
						 const void* RequestData, 
						 WORD RequestSize, 
						 ULONG ReplySize, 
						 ULONG_PTR ctx, 
						 DWORD Timeout/* = 4000*/, 
						 UCHAR Flags/* = IP_FLAG_DF*/, 
						 UCHAR Ttl/* = 255*/)
{
	if (EchoRequestContext* pCtx = new(ReplySize) EchoRequestContext(this, ctx))
	{
		HANDLE hFile;

		ULONG dwError = ERROR_INVALID_HANDLE;

		if (LockHandle(hFile))
		{
			IP_OPTION_INFORMATION opt = { Ttl, 0, Flags };

			dwError = IcmpSendEcho2Ex(hFile, 0, EchoRequestContext::sOnApc, 
				pCtx, 0, DestinationAddress, 
				const_cast<void*>(RequestData), RequestSize, &opt, 
				pCtx->ReplyBuffer, ReplySize, Timeout) ? NOERROR : GetLastError();

			UnlockHandle();
		}

		switch (dwError)
		{
		case NOERROR:
		case ERROR_IO_PENDING:
			break;
		default:
			pCtx->OnApc(dwError, 0);
		}

		return ;
	}

	OnReply(ERROR_OUTOFMEMORY, 0, 0, ctx);
}

_NT_END