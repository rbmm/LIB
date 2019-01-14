#include "StdAfx.h"

_NT_BEGIN

#include "subclass.h"

BOOL GUIAcquireRundownProtection();

void GUIReleaseRundownProtection();

//////////////////////////////////////////////////////////////////////////
// ZSubClass

#pragma pack(push, 1)

struct STUB 
{
#ifdef _WIN64
	DWORD _align;
	WORD _code;// b848 mov rax,_func
	PVOID _func;
	WORD _ffd0;//call rax
#else
	union
	{
		DWORD _dw;
		struct  
		{
			BYTE _align[3];
			BYTE _code;// e8
		};
	};
	DWORD _delta;
#endif
	UCHAR _body[];
};

#pragma pack(pop)

namespace {

	HANDLE _hHeap;

	void __cdecl _DestroyBlockHeap()
	{
		HeapDestroy(_hHeap);
	}

	HANDLE _GetBlockHeap()
	{
		if (!_hHeap)
		{
			if (HANDLE hHeap = HeapCreate(HEAP_CREATE_ENABLE_EXECUTE, 0x10000, 0x100000))
			{
				if (InterlockedCompareExchangePointer((void**)&_hHeap, hHeap, 0) == 0)
				{
					atexit(_DestroyBlockHeap);
					return hHeap;
				}

				HeapDestroy(hHeap);
			}
		}

		return _hHeap;
	}
}

void* ZSubClass::operator new(size_t cb)
{
	if (GUIAcquireRundownProtection())
	{
		if (HANDLE hHeap = _GetBlockHeap())
		{
			if (STUB* p = (STUB*)HeapAlloc(hHeap, 0, FIELD_OFFSET(STUB, _body[cb])))
			{
#ifdef _WIN64
				p->_code = 0xb848;
				p->_ffd0 = 0xd0ff;
				p->_func = __WindowProc;
#else
				p->_dw = 0xe8cccccc;
				p->_delta = RtlPointerToOffset(p->_body, __WindowProc);
#endif

				return p->_body;
			}
		}

		GUIReleaseRundownProtection();
	}

	return nullptr;
}

void ZSubClass::operator delete(void* pv)
{
	HeapFree(_hHeap, 0, CONTAINING_RECORD(pv, STUB, _body));
	GUIReleaseRundownProtection();
}

LRESULT ZSubClass::_WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	AddRef();
	LRESULT r = WindowProc(hWnd, uMsg, wParam, lParam);
	Release();
	return r;
}

LRESULT ZSubClass::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_NCDESTROY)
	{
		Unsubclass(hWnd);
	}

	return CallWindowProc(_prevWndProc, hWnd, uMsg, wParam, lParam);
}

void ZSubClass::Subclass(HWND hWnd)
{
	AddRef();
	_hWnd = hWnd;
	_prevWndProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)&CONTAINING_RECORD(this, STUB, _body)->_code);
}

void ZSubClass::Unsubclass(HWND hWnd)
{
	SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)_prevWndProc);
	_hWnd = 0;
	Release();
}

HRESULT ZSubClass::QI(REFIID /*riid*/, void **ppvObject)
{
	*ppvObject = 0;
	return E_NOTIMPL;
}

_NT_END