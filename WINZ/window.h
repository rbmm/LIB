#pragma once

#include "object.h"
#include "app.h"
#include "layout.h"

class ZView;

void WINAPI gui_delete(void* p);

void* WINAPI gui_new(size_t cb);

void WINAPI RundownGUI();

class WINZ_API Z_INTERFACE("8E9D9C1D-763E-4ad0-8C68-C2D6F232BB45") ZWnd : public ZObject
{
	LRESULT WrapperWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	static LRESULT CALLBACK _WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	static LRESULT CALLBACK StartWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	HWND _hWnd;
	LONG _dwCallCount;

protected:

	LRESULT MStartWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	virtual void AfterLastMessage()
	{
	}

	virtual LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	virtual LRESULT DefWinProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

public:

	virtual ZView* getView();
	
	virtual BOOL CanCloseFrame();

	virtual HRESULT QI(REFIID riid, void **ppvObject);

	HWND Create(
		DWORD dwExStyle,
		PCWSTR lpWindowName,
		DWORD dwStyle,
		int x,
		int y,
		int nWidth,
		int nHeight,
		HWND hWndParent,
		HMENU hMenu,
		PVOID lpParam
		);

	HWND getHWND(){ return _hWnd; }

	ZWnd() { _hWnd = 0; }

	static ZWnd* FromHWND(HWND hwnd);

	void operator delete[](void* p)
	{
		gui_delete(p);
	}

	void operator delete(void* p)
	{
		gui_delete(p);
	}

	void* operator new(size_t cb)
	{
		return gui_new(cb);
	}
};

class WINZ_API Z_INTERFACE("03200284-8B53-41b5-9310-BD613171E2F3") ZDlg : public ZObject
{
	HWND _hWnd;
	LONG _dwCallCount;

	INT_PTR WrapperDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK _DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

	INT_PTR MStartDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK StartDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

protected:
	virtual void AfterLastMessage()
	{
	}

	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

public:

	virtual HRESULT QI(REFIID riid, void **ppvObject);

	HWND Create(HINSTANCE hInstance, LPCWSTR lpTemplateName, HWND hWndParent, LPARAM dwInitParam);

	INT_PTR DoModal(HINSTANCE hInstance, LPCWSTR lpTemplateName, HWND hWndParent, LPARAM dwInitParam);

	HWND getHWND(){ return _hWnd; }

	ZDlg() { _hWnd = 0; }

	static ZDlg* FromHWND(HWND hwnd);

	static BOOL IsDialog(HWND hwnd);

	void operator delete[](void* p)
	{
		gui_delete(p);
	}

	void operator delete(void* p)
	{
		gui_delete(p);
	}

	void* operator new(size_t cb)
	{
		return gui_new(cb);
	}
};

int WINZ_API EmptyPaint(HWND hwnd);

BOOL IsDialogMessageEx(PMSG lpMsg);


