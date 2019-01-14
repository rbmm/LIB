// stdafx.cpp : source file that includes just the standard includes
// ktdi.pch will be the pre-compiled header
// stdafx.obj will contain the pre-compiled type information

#include "stdafx.h"

// TODO: reference any additional headers you need in STDAFX.H
// and not in this file
void* __cdecl operator new(size_t size, NT::POOL_TYPE PoolType)
{
	return NT::ExAllocatePool(PoolType, size);
}

void* __cdecl operator new[](size_t size, NT::POOL_TYPE PoolType)
{
	return NT::ExAllocatePool(PoolType, size);
}

void __cdecl operator delete(PVOID pv)
{
	NT::ExFreePool(pv);
}

void __cdecl operator delete(PVOID pv, size_t)
{
	NT::ExFreePool(pv);
}

void __cdecl operator delete[](PVOID pv)
{
	NT::ExFreePool(pv);
}