#pragma once

#include "../inc/asmfunc.h"
#include "winZ.h"

void ReferenceDll()ASM_FUNCTION;
void DereferenceDll()ASM_FUNCTION;

#define Z_INTERFACE(x) __declspec(uuid(x)) __declspec(novtable)

#ifdef _X86_
#define VSIFN "YGXXZ"
#else
#define VSIFN "YAXXZ"
#endif

__pragma(comment(linker, "/alternatename:@?FastReferenceDll=@?FastReferenceDllNop" ))
__pragma(comment(linker, "/alternatename:?ReferenceDll@NT@@" VSIFN "=@?FastReferenceDllNop" ))
__pragma(comment(linker, "/alternatename:?DereferenceDll@NT@@" VSIFN "=@?FastReferenceDllNop" ))

//#define _DBG

class WINZ_API __declspec(novtable) ZObject
{
private:
	
	LONG _dwRef;

protected:

	virtual ~ZObject()
	{
		DereferenceDll();
	}

public:

#ifdef _DBG
	void operator delete(void* p);
	void* operator new(size_t size);
#endif

	ZObject()
	{
		_dwRef = 1;
		ReferenceDll();
	}

	virtual HRESULT QI(REFIID /*riid*/, void **ppvObject)
	{
		*ppvObject = 0;
		return E_NOINTERFACE;
	}

	ULONG AddRef()
	{
		return InterlockedIncrementNoFence(&_dwRef);
	}

	ULONG Release()
	{
		ULONG dwRef = InterlockedDecrement(&_dwRef);
		if (!dwRef) delete this;
		return dwRef;
	}
};