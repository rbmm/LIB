#pragma once

#include "object.h"

class WINZ_API __declspec(novtable) ZSubClass : public ZObject 
{
	HWND _hwnd = 0;
	LONG _dwCallCount;

	static LRESULT CALLBACK SubClassProc(HWND hwnd, UINT uMsg, WPARAM wParam,
		LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData); 

	LRESULT WrapperWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	void OnCallCountZero();

protected:

	virtual HRESULT QI(REFIID riid, void **ppvObject);

	virtual ~ZSubClass() = default;

	virtual LRESULT WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	virtual void AfterLastMessage()
	{
	}

public:

	BOOL Subclass(HWND hWnd);

	void Unsubclass(HWND hWnd);

	HWND getHWND() { return _hwnd; }
};