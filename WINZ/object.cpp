#include "StdAfx.h"

_NT_BEGIN

#include "object.h"

#ifdef _DBG

static LONG s_n;

void ZObject::operator delete(void* p)
{
	::operator delete (p);
	if (!InterlockedDecrement(&s_n))
	{
		__nop();
	}
}

void* ZObject::operator new(size_t size)
{
	InterlockedIncrement(&s_n);
	return ::operator new(size);
}

#endif

_NT_END