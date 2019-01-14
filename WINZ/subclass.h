#pragma once

#include "object.h"

class WINZ_API __declspec(novtable) ZSubClass : public ZObject
{
	WNDPROC _prevWndProc;
	HWND _hWnd;

	LRESULT _WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	static void __WindowProc();

protected:

	virtual HRESULT QI(REFIID riid, void **ppvObject);

	virtual LRESULT WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

public:

	HWND getHWND(){ return _hWnd; }

	void Subclass(HWND hWnd);

	void Unsubclass(HWND hWnd);

	void* operator new(size_t cb);

	void operator delete(void* pv);
};