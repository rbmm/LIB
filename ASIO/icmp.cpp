#include "StdAfx.h"

_NT_BEGIN

#include <iphlpapi.h>
#include <IPExport.h>
#include <icmpapi.h>
#include "icmp.h"

void ICMP_DATA::IOCompletionRoutine(CDataPacket* , DWORD , NTSTATUS , ULONG_PTR , PVOID )
{
	__debugbreak();
}

void ICMP_DATA::CloseObjectHandle(HANDLE hFile)
{
	IcmpCloseHandle(hFile);
}

void WINAPI ICMP_DATA::OnApc(REQUEST_DATA* pData, PIO_STATUS_BLOCK piosb, ULONG )
{
	pData->m_pObj->OnReply((PICMP_ECHO_REPLY)pData->m_packet->getData(), piosb->Status, 
		(ULONG)piosb->Information, pData->m_packet, pData->m_pv, pData->m_dw);

	delete pData;
}

ULONG ICMP_DATA::Create()
{
	HANDLE hFile = IcmpCreateFile();

	if (hFile == INVALID_HANDLE_VALUE) 
	{
		return GetLastError();
	}

	Assign(hFile);

	return NOERROR;
}

ULONG ICMP_DATA::SendEcho(IPAddr DestinationAddress, PVOID pvData, WORD cbData, CDataPacket* packet, PVOID pv, DWORD dw, DWORD Timeout, UCHAR Flags, UCHAR Ttl)
{
	IP_OPTION_INFORMATION opt = { Ttl, 0, Flags };

	ULONG err = ERROR_NO_SYSTEM_RESOURCES;

	if (REQUEST_DATA* pData = new REQUEST_DATA(this, packet, pv, dw))
	{
		err = ERROR_INVALID_HANDLE;

		HANDLE hFile;
		if (LockHandle(hFile))
		{
			err = IcmpSendEcho2Ex(hFile, 0, (PIO_APC_ROUTINE)OnApc, pData, 
				0, DestinationAddress, 
				pvData, cbData, 
				&opt, 
				packet->getFreeBuffer(), packet->getFreeSize(), 
				Timeout);

			UnlockHandle();
		}

		switch (err)
		{
		case NOERROR:
		case ERROR_IO_PENDING:
			break;
		default:
			delete pData;
		}
	}

	return err;
}

ICMP_DATA::REQUEST_DATA::REQUEST_DATA(ICMP_DATA* pObj, CDataPacket* packet, PVOID pv, DWORD dw)
{
	m_pObj = pObj;
	pObj->AddRef();
	m_packet = packet;
	packet->AddRef();
	m_pv = pv;
	m_dw = dw;
}

ICMP_DATA::REQUEST_DATA::~REQUEST_DATA()
{
	m_packet->Release();
	m_pObj->Release();
}

_NT_END