#include "stdafx.h"

_NT_BEGIN

#include "blockheap.h"

BLOCK_HEAP::BLOCK_HEAP()
{
	_buf = 0, _cb = 0;
	RtlInitializeSListHead(&_head);
}

BLOCK_HEAP::~BLOCK_HEAP()
{
	if (_buf) 
	{
		if (RtlQueryDepthSList(&_head) != (WORD)_count)
		{
			__debugbreak();
		}
		SIZE_T RegionSize = 0;
		NtFreeVirtualMemory(NtCurrentProcess(), &_buf, &RegionSize, MEM_RELEASE);
	}
}

BOOL BLOCK_HEAP::IsBlock(PVOID p)
{
	return RtlPointerToOffset(_buf, p) < _cb;
}

BOOL BLOCK_HEAP::Create(DWORD cbBlock, DWORD count, ULONG protect)
{
	if (!count) return FALSE;

	if (cbBlock < sizeof(SLIST_ENTRY)) cbBlock = sizeof(SLIST_ENTRY);

	cbBlock = (cbBlock + __alignof(SLIST_ENTRY) - 1) & ~(__alignof(SLIST_ENTRY) - 1);

	SIZE_T RegionSize = cbBlock * count;

	union {
		PVOID pv;
		PBYTE pb;
		PSLIST_ENTRY entry;
	};

	pv = 0;

	if (0 <= NtAllocateVirtualMemory(NtCurrentProcess(), &pv, 0, &RegionSize, MEM_COMMIT, protect))
	{
		_buf = pv, _cb = RegionSize, count = (DWORD)RegionSize / cbBlock, _count = count;
		
		PSLIST_HEADER head = &_head;

		do 
		{
			RtlInterlockedPushEntrySList(head, entry);
		} while (pb += cbBlock, --count);

		return TRUE;
	}

	return FALSE;
}

PVOID BLOCK_HEAP::alloc()
{
	return RtlInterlockedPopEntrySList(&_head);
}

void BLOCK_HEAP::free(PVOID p)
{
	RtlInterlockedPushEntrySList(&_head, reinterpret_cast<PSLIST_ENTRY>(p));
}

_NT_END