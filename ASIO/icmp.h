#pragma once

#include "io.h"

class __declspec(novtable) ICMP : public IO_OBJECT
{
	virtual void IOCompletionRoutine(CDataPacket* , ULONG , NTSTATUS , ULONG_PTR , PVOID );

	virtual void CloseObjectHandle(HANDLE hFile);

	struct EchoRequestContext 
	{
		ICMP* pObj;
		ULONG_PTR ctx;
		UCHAR ReplyBuffer[];

		EchoRequestContext(ICMP* pObj, ULONG_PTR ctx) : pObj(pObj), ctx(ctx)
		{
			pObj->AddRef();
		}

		~EchoRequestContext()
		{
			pObj->Release();
		}

		void* operator new(size_t s, ULONG ReplySize)
		{
			return IO_OBJECT::operator new(s + ReplySize);
		}

		void operator delete(void* pv)
		{
			IO_OBJECT::operator delete(pv);
		}

		void OnApc(ULONG dwError, ULONG ReplySize);

		static void WINAPI sOnApc(PVOID This, PIO_STATUS_BLOCK piosb, ULONG );
	};

	friend EchoRequestContext;

protected:

	virtual void OnReply(ULONG dwError, PICMP_ECHO_REPLY ReplyBuffer, ULONG ReplySize, ULONG_PTR ctx) = 0;

public:

	ULONG Create();

	void SendEcho(
		IPAddr DestinationAddress, 
		const void* RequestData, 
		WORD RequestSize, 
		ULONG ReplySize, 
		ULONG_PTR ctx, 
		DWORD Timeout = 4000, 
		UCHAR Flags = IP_FLAG_DF, 
		UCHAR Ttl = 255);
};
