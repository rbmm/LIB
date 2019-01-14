#pragma once

#include "winZ.h"

#define Z_INTERFACE(x) __declspec(uuid(x)) __declspec(novtable)

//#define _DBG

class WINZ_API __declspec(novtable) ZObject
{
private:
	
	LONG _dwRef;

protected:

	virtual ~ZObject()
	{
	}

public:

#ifdef _DBG
	void operator delete(void* p);
	void* operator new(size_t size);
#endif

	ZObject()
	{
		_dwRef = 1;
	}

	virtual HRESULT QI(REFIID /*riid*/, void **ppvObject)
	{
		*ppvObject = 0;
		return E_NOINTERFACE;
	}

	ULONG AddRef()
	{
		return InterlockedIncrement(&_dwRef);
	}

	ULONG Release()
	{
		ULONG dwRef = InterlockedDecrement(&_dwRef);
		if (!dwRef) delete this;
		return dwRef;
	}
};