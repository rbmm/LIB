// stdafx.cpp : source file that includes just the standard includes
// winZ.pch will be the pre-compiled header
// stdafx.obj will contain the pre-compiled type information

#include "stdafx.h"

// TODO: reference any additional headers you need in STDAFX.H
// and not in this file
void* __cdecl operator new[](size_t ByteSize)
{
	return HeapAlloc(GetProcessHeap(), 0, ByteSize);
}

void* __cdecl operator new(size_t ByteSize)
{
	return HeapAlloc(GetProcessHeap(), 0, ByteSize);
}

void __cdecl operator delete(void* Buffer)
{
	HeapFree(GetProcessHeap(), 0, Buffer);
}

void __cdecl operator delete(void* Buffer, size_t)
{
	HeapFree(GetProcessHeap(), 0, Buffer);
}

void __cdecl operator delete[](void* Buffer)
{
	HeapFree(GetProcessHeap(), 0, Buffer);
}

void __cdecl operator delete[](void* Buffer, size_t)
{
	HeapFree(GetProcessHeap(), 0, Buffer);
}
