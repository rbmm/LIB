#pragma once

#include "io.h"

class MailSlot : public IO_OBJECT
{
	void OnBufferTooSmall();

	void ReadWrite(PVOID Buffer, ULONG Length, ULONG op, NTSTATUS (NTAPI * fn)(
		HANDLE ,HANDLE ,PIO_APC_ROUTINE ,PVOID ,PIO_STATUS_BLOCK ,PVOID , ULONG ,PLARGE_INTEGER, PULONG));

protected:
	
	enum { opRead, opWrite };

	virtual void IOCompletionRoutine(CDataPacket* packet, DWORD Code, NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PVOID Pointer);

	virtual void OnServerClosed()
	{
	}

	virtual void OnError(NTSTATUS /*status*/)
	{
	}

	virtual PVOID OnWrite(PVOID Buffer, ULONG_PTR /*dwNumberOfBytesTransfered*/)
	{
		return Buffer;
	}

	virtual PVOID OnRead(PVOID Buffer, ULONG_PTR /*dwNumberOfBytesTransfered*/)
	{
		return Buffer;
	}

	virtual void BufferNotInUse(PVOID /*buf*/)
	{
	}

	virtual void OnBufferTooSmall(ULONG /*NextSize*/)
	{
	}
	
public:
	void Read(PVOID Buffer, ULONG Length);

	void Write(PVOID Buffer, ULONG Length);

	NTSTATUS Init(HANDLE hFile);

	NTSTATUS Create(
		_In_     PCWSTR lpName,
		_In_     DWORD nMaxMessageSize = 0,
		_In_     DWORD lReadTimeout = MAILSLOT_WAIT_FOREVER,
		_In_opt_ PSECURITY_ATTRIBUTES lpSecurityAttributes = 0);

	NTSTATUS Open(PCWSTR lpName, PSECURITY_ATTRIBUTES lpSecurityAttributes = 0);
};
