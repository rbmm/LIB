#include "StdAfx.h"

_NT_BEGIN

#include "subclass.h"

//////////////////////////////////////////////////////////////////////////
// ZSubClass
void ZSubClass::OnCallCountZero()
{
	AfterLastMessage();
	Release();
}

BOOL ZSubClass::Subclass(HWND hWnd)
{
	_dwCallCount = 1 << 31;
	AddRef();

	if (SetWindowSubclass(hWnd, SubClassProc, (UINT_PTR)this, (ULONG_PTR)this))
	{
		_hwnd = hWnd;
		return TRUE;
	}

	_dwCallCount = 0;
	OnCallCountZero();
	return FALSE;
}

void ZSubClass::Unsubclass(HWND hWnd)
{
	if (_bittestandreset(&_dwCallCount, 31))
	{
		if (!RemoveWindowSubclass(hWnd, SubClassProc, (UINT_PTR)this)) __debugbreak();
		if (!_dwCallCount)
		{
			OnCallCountZero();
		}
	}
}

LRESULT ZSubClass::WrapperWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	_dwCallCount++;
	lParam = WindowProc(hWnd, uMsg, wParam, lParam);
	if (!--_dwCallCount)
	{
		OnCallCountZero();
	}
	return lParam;
}

LRESULT CALLBACK ZSubClass::SubClassProc(HWND hwnd, UINT uMsg, WPARAM wParam,
							  LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	if (uIdSubclass != dwRefData) __debugbreak();

	return reinterpret_cast<ZSubClass*>(dwRefData)->WrapperWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT ZSubClass::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_NCDESTROY:
		Unsubclass(hWnd);
		break;
	}

	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

HRESULT ZSubClass::QI(REFIID /*riid*/, void **ppvObject)
{
	*ppvObject = 0;
	return E_NOTIMPL;
}

_NT_END