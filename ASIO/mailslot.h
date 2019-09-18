#pragma once

#include "io.h"

class MailSlot : public IO_OBJECT
{
	void OnWrite(NTSTATUS status);

protected:
	
	enum { opRead = 'rrrr', opWrite = 'wwww' };

	virtual void IOCompletionRoutine(CDataPacket* packet, DWORD Code, NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PVOID Pointer);

	virtual void OnServerClosed()
	{
	}

	virtual void OnError(NTSTATUS /*status*/)
	{
	}

	virtual void OnRead(PVOID /*buf*/, ULONG /*dwNumberOfBytesTransfered*/, CDataPacket* /*packet*/)
	{
	}

	virtual void FreeAfterWrite(PVOID /*buf*/)
	{
	}

	virtual void OnBufferTooSmall(ULONG NextSize);
	
	virtual bool IsSizeOk(ULONG /*NextSize*/)
	{
		return true;
	}

public:
	NTSTATUS Read(CDataPacket* packet);

	NTSTATUS Write(PVOID Buffer, ULONG Length);

	NTSTATUS Init(HANDLE hFile);

	NTSTATUS Create(
		_In_     PCWSTR lpName,
		_In_     DWORD nMaxMessageSize = 0,
		_In_     DWORD lReadTimeout = MAILSLOT_WAIT_FOREVER,
		_In_opt_ PSECURITY_ATTRIBUTES lpSecurityAttributes = 0);

	NTSTATUS Open(PCWSTR lpName, PSECURITY_ATTRIBUTES lpSecurityAttributes = 0);
};
