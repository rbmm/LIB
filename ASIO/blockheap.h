#pragma once

class BLOCK_HEAP
{
	SLIST_HEADER _head;
	PVOID _buf;
	SIZE_T _cb;
	ULONG _count;

public:

	BLOCK_HEAP();

	~BLOCK_HEAP();

	BOOL IsBlock(PVOID p);

	BOOL Create(DWORD cbBlock, DWORD count, ULONG protect = PAGE_READWRITE);

	PVOID alloc();

	void free(PVOID p);
};

