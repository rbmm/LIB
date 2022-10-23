#pragma once

#include "memdump.h"

class CDbgPipe;

class ZMemoryCache
{
	enum{
		chunk_size = 0x400,
		chunk_count = 0x40
	};
protected:
	union {
		HANDLE _hProcess;// local debugger
		IMemoryDump* _pDump; // dump
		CDbgPipe* _pipe;// remote  debugger
	};
private:
	typedef BYTE (*CHUNK)[chunk_size];
	union
	{
		PVOID _Buffer;
		CHUNK _Chunks;
	};
	PVOID _RemoteAddresses[chunk_count];
	DWORD _LocalOffsets[chunk_count];
	LONG _index;
protected:
	void Cleanup();
	void WriteToCache(PVOID RemoteAddress, UCHAR c);

public:

	~ZMemoryCache();
	ZMemoryCache();

	HANDLE getProcess() { return _hProcess; }

	BOOL Create();
	NTSTATUS Read(PVOID RemoteAddress, PVOID buf, DWORD cb, PSIZE_T pcb = 0);
	NTSTATUS Write(PVOID RemoteAddress, UCHAR c);
	void Invalidate();
	NTSTATUS ReadNoCache(PVOID RemoteAddress, PVOID buf, DWORD cb, PSIZE_T pcb = 0)
	{
		return ZwReadVirtualMemory(_hProcess, RemoteAddress, buf, cb, pcb);
	}
};
