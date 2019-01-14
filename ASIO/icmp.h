#pragma once

#include "io.h"

class __declspec(novtable) ICMP_DATA : public IO_OBJECT
{
	virtual void IOCompletionRoutine(CDataPacket* , DWORD , NTSTATUS , ULONG_PTR , PVOID );

	virtual void CloseObjectHandle(HANDLE hFile);

	struct REQUEST_DATA 
	{
		ICMP_DATA* m_pObj;
		CDataPacket* m_packet;
		PVOID m_pv;
		DWORD m_dw;

		REQUEST_DATA(ICMP_DATA* pObj, CDataPacket* packet, PVOID pv = 0, DWORD dw = 0);

		~REQUEST_DATA();
	};

	static void WINAPI OnApc(REQUEST_DATA* pData, PIO_STATUS_BLOCK piosb, ULONG );

protected:

	virtual void OnReply(PICMP_ECHO_REPLY reply, NTSTATUS status, DWORD ReplySize, CDataPacket* packet, PVOID pv, DWORD dw) = 0;

public:

	ULONG Create();

	ULONG SendEcho(IPAddr DestinationAddress, PVOID pvData, WORD cbData, CDataPacket* packet, PVOID pv, DWORD dw, DWORD Timeout = 4000, UCHAR Flags = IP_FLAG_DF, UCHAR Ttl = 255);
};
