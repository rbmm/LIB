#include "StdAfx.h"

_NT_BEGIN
#include "cursors.h"
#include "window.h"
#include "../inc/rundown.h"

struct GUI_RUNDOWN_REF : public RUNDOWN_REF
{
	ULONG dwThreadId;

	GUI_RUNDOWN_REF() : dwThreadId(GetCurrentThreadId()) {}

	virtual void RundownCompleted()
	{
		PostThreadMessage(dwThreadId, WM_QUIT, 0, 0);
	}
} gGUIrp;

void WINAPI gui_delete(void* p)
{
	::operator delete(p);
	gGUIrp.Release();
}

void* WINAPI gui_new(size_t cb)
{
	if (gGUIrp.Acquire())
	{
		if (PVOID p = ::operator new(cb)) return p;
		gGUIrp.Release();
	}
	return nullptr;
}

void WINAPI RundownGUI()
{
	gGUIrp.BeginRundown();
}

BOOL GUIAcquireRundownProtection()
{
	return gGUIrp.Acquire();
}

void GUIReleaseRundownProtection()
{
	gGUIrp.Release();
}

STATIC_WSTRING(szwndcls, "{1A7CB018-E81A-49e3-AD97-5296E36BC716}");

int EmptyPaint(HWND hwnd)
{
	PAINTSTRUCT ps;
	if (BeginPaint(hwnd, &ps))
	{
		EndPaint(hwnd, &ps);
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
// ZWnd

namespace {

	ATOM ga;

	void __cdecl UnregWndClass()
	{
		if (!UnregisterClassW(szwndcls, (HINSTANCE)&__ImageBase))
		{
			__debugbreak();
		}
	}

	LONG gdwTlsIndex = TLS_OUT_OF_INDEXES;

	void __cdecl freeTlsIndex()
	{
		TlsFree(gdwTlsIndex);
	}

	ULONG getTlsIndex()
	{
		if (gdwTlsIndex == TLS_OUT_OF_INDEXES)
		{
			ULONG dwTlsIndex = TlsAlloc();

			if (dwTlsIndex != TLS_OUT_OF_INDEXES)
			{
				if (InterlockedCompareExchangeNoFence(&gdwTlsIndex, dwTlsIndex, TLS_OUT_OF_INDEXES) == (LONG)TLS_OUT_OF_INDEXES)
				{
					atexit(freeTlsIndex);

					return dwTlsIndex;
				}

				TlsFree(dwTlsIndex);
			}
		}

		return gdwTlsIndex;
	}

	BOOL SetThis(PVOID pv)
	{
		ULONG dwTlsIndex = getTlsIndex();

		if (dwTlsIndex == TLS_OUT_OF_INDEXES)
		{
			return FALSE;
		}

		return TlsSetValue(dwTlsIndex, pv);
	}
};

LRESULT CALLBACK ZWnd::StartWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	PVOID pv = TlsGetValue(getTlsIndex());
	SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pv);
	SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)_WindowProc);
	reinterpret_cast<ZWnd*>(pv)->AddRef();
	reinterpret_cast<ZWnd*>(pv)->_hWnd = hwnd;
	return reinterpret_cast<ZWnd*>(pv)->WindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK ZWnd::_WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	ZWnd* p = (ZWnd*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
	p->AddRef();
	LRESULT r = p->WindowProc(hwnd, uMsg, wParam, lParam);
	p->Release();
	return r;
}

LRESULT ZWnd::DefWinProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT ZWnd::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_NCCREATE:
		if (((LPCREATESTRUCT)lParam)->style & WS_MAXIMIZE)
		{
			ShowWindow(hwnd, SW_HIDE);
		}
		break;
	case WM_NCDESTROY:
		_hWnd = 0;
		Release();
		break;
	}

	return DefWinProc(hwnd, uMsg, wParam, lParam);
}

HWND ZWnd::Create( DWORD dwExStyle, LPCTSTR lpWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, PVOID lpParam)
{
	if (!ga)
	{
		WNDCLASS wndcls = { 0, StartWindowProc, 0, 0, (HINSTANCE)&__ImageBase, 0, CCursorCashe::GetCursor(CCursorCashe::ARROW), 0, 0, szwndcls };
		if (ATOM a = RegisterClass(&wndcls))
		{
			ga = a;
			atexit(UnregWndClass);
		}
		else if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
		{
			return NULL;
		}
	}

	return SetThis(this) ? CreateWindowExW(dwExStyle, szwndcls, lpWindowName, dwStyle, x, y, nWidth, nHeight, 
		hWndParent, hMenu, (HINSTANCE)&__ImageBase, lpParam) : NULL;
}

ZWnd* ZWnd::FromHWND(HWND hwnd)
{
	if (_WindowProc == (PVOID)GetWindowLongPtrW(hwnd, GWLP_WNDPROC))
	{
		ZWnd* p = (ZWnd*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
		p->AddRef();
		return p;
	}

	return 0;
}

ZView* ZWnd::getView()
{
	return 0;
}

BOOL ZWnd::CanCloseFrame()
{
	return TRUE;
}

HRESULT ZWnd::QI(REFIID riid, void **ppvObject)
{
	if (riid == __uuidof(ZWnd))
	{
		*ppvObject = static_cast<ZObject*>(this);
		AddRef();
		return S_OK;
	}

	*ppvObject = 0;

	return E_NOINTERFACE;
}

//////////////////////////////////////////////////////////////////////////
// ZDlg

INT_PTR CALLBACK ZDlg::StartDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	PVOID pv = TlsGetValue(getTlsIndex());
	SetWindowLongPtr(hwndDlg, DWLP_USER, (LONG_PTR)pv);
	SetWindowLongPtr(hwndDlg, DWLP_DLGPROC, (LONG_PTR)_DialogProc);
	reinterpret_cast<ZDlg*>(pv)->AddRef();
	reinterpret_cast<ZDlg*>(pv)->_hWnd = hwndDlg;

	return reinterpret_cast<ZDlg*>(pv)->DialogProc(hwndDlg, uMsg, wParam, lParam);
}

INT_PTR CALLBACK ZDlg::_DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	ZDlg* p = (ZDlg*)GetWindowLongPtrW(hwndDlg, DWLP_USER);
	p->AddRef();
	INT_PTR r = p->DialogProc(hwndDlg, uMsg, wParam, lParam);
	p->Release();
	return r;
}

INT_PTR ZDlg::DialogProc(HWND /*hwndDlg*/, UINT uMsg, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	switch (uMsg)
	{
	case WM_NCDESTROY:
		_hWnd = 0;
		Release();
		break;
	}

	return 0;
}

HWND ZDlg::Create(HINSTANCE hInstance, LPCWSTR lpTemplateName, HWND hWndParent, LPARAM dwInitParam)
{
	return SetThis(this) ? CreateDialogParam(hInstance, lpTemplateName, hWndParent, StartDialogProc, dwInitParam) : NULL;
}

INT_PTR ZDlg::DoModal(HINSTANCE hInstance, LPCWSTR lpTemplateName, HWND hWndParent, LPARAM dwInitParam)
{
	return SetThis(this) ? DialogBoxParam(hInstance, lpTemplateName, hWndParent, StartDialogProc, dwInitParam) : -1;
}

BOOL ZDlg::IsDialog(HWND hwnd)
{
	return _DialogProc == (PVOID)GetWindowLongPtrW(hwnd, DWLP_DLGPROC);
}

ZDlg* ZDlg::FromHWND(HWND hwnd)
{
	if (IsDialog(hwnd))
	{
		ZDlg* p = (ZDlg*)GetWindowLongPtrW(hwnd, DWLP_USER);
		p->AddRef();
		return p;
	}

	return 0;
}

HRESULT ZDlg::QI(REFIID riid, void **ppvObject)
{
	if (riid == __uuidof(ZDlg))
	{
		*ppvObject = static_cast<ZObject*>(this);
		AddRef();
		return S_OK;
	}

	*ppvObject = 0;

	return E_NOINTERFACE;
}

BOOL IsDialogMessageEx(PMSG lpMsg)
{
	HWND hwnd = lpMsg->hwnd;

	while (hwnd)
	{
		if (ZDlg::IsDialog(hwnd))
		{
			return IsDialogMessage(hwnd, lpMsg);
		}
		hwnd = GetParent(hwnd);
	}

	return FALSE;
}

_NT_END
