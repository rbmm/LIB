#include "stdafx.h"
#include "resource.h"
_NT_BEGIN

#include "../inc/initterm.h"
#include "../winZ/window.h"
#include "../winz/wic.h"

enum : ULONG {
	CONTROL_WPARAM_START = 0xB426E606,
	CONTROL_LPARAM_START = 0x4AC27B7B,
};

class StartButton : public ZWnd
{
	HDC _hMemDC = 0;
	HBITMAP _hbmp = 0;
	HGDIOBJ _ho = 0;
	HMENU _hmenu = 0, _hPopupMenu = 0;
	SIZE _size;
	POINT _pt;
	LONG _Flags = 0;
	enum { 
		bMouseIn, 
		bMousePressed, 
		bCaptureSet,
	};

	BOOL CreateImage(INT cx, INT cy)
	{
		LIC l;
		l._cx = cx, l._cy = 3*cy;
		if (0 <= l.CreateBMPFromPNG(MAKEINTRESOURCEW(1)))
		{
			l._cy /= 3;
			_pt.x = (cx - l._cx) >> 1, _pt.y = (cy - l._cy) >> 1;
			_size.cx = l._cx, _size.cy = l._cy;
			_hbmp = l._hbmp;
			_ho = SelectObject(_hMemDC, l._hbmp);
			return TRUE;
		}

		return FALSE;
	}

	BOOL CreateDCandMenu()
	{
		if (HDC hMemDC = CreateCompatibleDC(0))
		{
			if (HMENU hmenu = LoadMenuW((HINSTANCE)&__ImageBase, MAKEINTRESOURCEW(IDR_MENU1)))
			{
				_hMemDC = hMemDC;
				_hmenu = hmenu;
				_hPopupMenu = GetSubMenu(hmenu, 0);
				return TRUE;
			}
			DeleteDC(hMemDC);
		}

		return FALSE;
	}

	virtual LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		union {
			PAINTSTRUCT ps;
			POINT pt;
			RECT rc;
		};
		switch (uMsg)
		{
		case WM_NCCREATE:
			if (!CreateDCandMenu() || 
				!CreateImage(reinterpret_cast<CREATESTRUCT*>(lParam)->cx, reinterpret_cast<CREATESTRUCT*>(lParam)->cy))
			{
				return FALSE;
			}
			break;
		case WM_NCDESTROY:
			if (_hmenu)
			{
				DestroyMenu(_hmenu);
				_hmenu = 0;
			}

			if (_hMemDC)
			{
				if (_hbmp)
				{
					SelectObject(_hMemDC, _ho);
					DeleteObject(_hbmp);
				}
				DeleteDC(_hMemDC);
			}
			break;

		case WM_ERASEBKGND:
			return TRUE;

		case WM_PAINT:
			if (BeginPaint(hwnd, &ps))
			{
				DrawThemeParentBackground(hwnd, ps.hdc, &ps.rcPaint);
				static const BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

				GdiAlphaBlend(ps.hdc, _pt.x, _pt.y, _size.cx, _size.cy, _hMemDC, 0, 
					_bittest(&_Flags, bCaptureSet) ? 2 * _size.cy : (_bittest(&_Flags, bMouseIn) ? _size.cy : 0), _size.cx, _size.cy, bf);

				EndPaint(hwnd, &ps);
			}
			return 0;

		case WM_COMMAND:
			switch (wParam)
			{
			case ID_0_EXIT:
				DestroyWindow(GetAncestor(hwnd, GA_PARENT));
				break;
			}
			break;

		case WM_MOUSEMOVE:
			if (!_bittestandset(&_Flags, bMouseIn))
			{
				TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd };
				if (TrackMouseEvent(&tme))
				{
					InvalidateRect(hwnd, 0, TRUE);
				}
			}
			return 0;

		case WM_MOUSELEAVE:
			if (_bittestandreset(&_Flags, bMouseIn))
			{
				InvalidateRect(hwnd, 0, TRUE);
			}
			return 0;

		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
			if (!_bittestandset(&_Flags, bMousePressed))
			{
				if (!_bittestandset(&_Flags, bCaptureSet))
				{
					SetCapture(hwnd);
					InvalidateRect(hwnd, 0, TRUE);
				}
			}
			return 0;
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
			if (_bittestandreset(&_Flags, bCaptureSet))
			{
				ReleaseCapture();
				InvalidateRect(hwnd, 0, TRUE);
			}
			if (_bittestandreset(&_Flags, bMousePressed))
			{
				if ((ULONG)GET_X_LPARAM(lParam) < (ULONG)_size.cx && (ULONG)GET_Y_LPARAM(lParam) < (ULONG)_size.cy)
				{
					GetCursorPos(&pt);
					SetForegroundWindow(hwnd);

					TrackPopupMenu(_hPopupMenu, TPM_BOTTOMALIGN|TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, 0);
				}
			}

			return 0;
		}
		return __super::WindowProc(hwnd, uMsg, wParam, lParam);
	}
};

class SecondaryTrayWnd : public ZWnd
{
	HWND _hwndStart;
	HCURSOR _hcur[2]{};
	int _xy;

	BOOL OnCreate(HWND hwnd, CREATESTRUCT* pcs)
	{
		if (StartButton* p = new StartButton)
		{
			int xy = pcs->cy;
			_xy = xy;

			_hwndStart = hwnd = p->Create(0, 0, WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS, 0, 0, xy, xy, hwnd, 0, 0);

			p->Release();

			if (hwnd)
			{
				_hcur[0] = LoadCursor(0, IDC_ARROW);
				_hcur[1] = LoadCursor(0, IDC_SIZEALL);
				return TRUE;
			}
		}

		return FALSE;
	}

	virtual LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		union {
			PAINTSTRUCT ps;
			POINT pt;
			RECT rc;
		};

		switch (uMsg)
		{
		case WM_NCLBUTTONDBLCLK:
			DestroyWindow(hwnd);
			return 0;

		case WM_NCCREATE:
			if (!OnCreate(hwnd, reinterpret_cast<CREATESTRUCT*>(lParam)))
			{
				return FALSE;
			}
			break;

		case WM_ERASEBKGND:
			GetClientRect(hwnd, &rc);
			FillRect((HDC)wParam, &rc, (HBRUSH)(1 + COLOR_DESKTOP));
			return TRUE;

		case WM_SIZE:
			if (_hwndStart)
			{
				switch (wParam)
				{
				case SIZE_RESTORED:
				case SIZE_MAXIMIZED:
					MoveWindow(_hwndStart, 0, 0, GET_X_LPARAM(lParam)>>1, GET_Y_LPARAM(lParam), TRUE);
				}
			}
			break;
		case WM_PAINT:
			if (BeginPaint(hwnd, &ps))
			{
				EndPaint(hwnd, &ps);
			}
			return 0;

		case WM_NCHITTEST:
			pt.x = GET_X_LPARAM(lParam);
			pt.y = GET_Y_LPARAM(lParam);
			ScreenToClient(hwnd, &pt);
			return pt.x < _xy ? HTCLIENT : HTCAPTION;

		case WM_SETCURSOR:
			SetCursor(_hcur[HIWORD(lParam) && LOWORD(lParam) == HTCAPTION ? 1 : 0]) ;
			return TRUE;
		}

		return __super::WindowProc(hwnd, uMsg, wParam, lParam);
	}
};

LRESULT CALLBACK CallWndProc(int nCode,
							 WPARAM wParam,
							 LPARAM lParam)
{
	if (nCode == HC_ACTION)
	{
		PCWPSTRUCT pcs = (PCWPSTRUCT)lParam;

		if (pcs->message == WM_NULL && pcs->wParam == CONTROL_WPARAM_START && pcs->lParam == CONTROL_LPARAM_START)
		{
			RECT rc;
			if (SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0))
			{
				int xy = GetSystemMetrics(SM_CYSCREEN) - (rc.bottom -= rc.top);

				ULONG r;
				BCryptGenRandom(0, (PBYTE)&r, sizeof(r), BCRYPT_USE_SYSTEM_PREFERRED_RNG);

				if (SecondaryTrayWnd* pTwnd = new SecondaryTrayWnd)
				{
					pTwnd->Create(WS_EX_TOPMOST|WS_EX_TOOLWINDOW, 0, WS_POPUP|WS_VISIBLE|WS_CLIPCHILDREN, 
						((rc.right - rc.left - 2*xy) * LOWORD(r)) >> 16, ((rc.bottom - xy) * HIWORD(r)) >> 16,
						xy * 2, xy, HWND_DESKTOP, 0, 0);

					pTwnd->Release();
				}
			}
		}
	}

	return CallNextHookEx(0, nCode, wParam, lParam);
}

#ifndef _WIN64
BOOL CALLBACK EnumThreadWndProc( HWND hwnd, LPARAM lParam)
{
	if (hwnd = FindWindowExW(hwnd, 0, WC_EDITW, 0))
	{
		if (HHOOK hhk = SetWindowsHookExW(WH_CALLWNDPROC, CallWndProc, (HMODULE)&__ImageBase, (ULONG)lParam))
		{
			SendMessageW(hwnd, WM_NULL, CONTROL_WPARAM_START, CONTROL_LPARAM_START);

			UnhookWindowsHookEx(hhk);
		}

		return FALSE;
	}

	return TRUE;
}
#endif

BOOL CALLBACK EnumThreadWndProc2( HWND hwnd, LPARAM /*lParam*/)
{
	if (GetClassLongPtrW(hwnd, GCLP_HMODULE) == (ULONG_PTR)&__ImageBase)
	{
		DestroyWindow(hwnd);
	}

	return TRUE;
}

STDAPI DllRegisterServer()
{
#ifndef _WIN64
	BOOL bWow;
	if (IsWow64Process(NtCurrentProcess(), &bWow) && bWow)
	{
		WCHAR appname[MAX_PATH];
		if (SearchPathW(0, L"notepad.exe", 0, _countof(appname), appname, 0))
		{
			STARTUPINFO si = { sizeof(si) };
			PROCESS_INFORMATION pi;
			if (CreateProcessW(appname, 0, 0, 0, 0, 0, 0, 0, &si, &pi))
			{
				CloseHandle(pi.hThread);
				if (WaitForInputIdle(pi.hProcess, INFINITE) == WAIT_OBJECT_0)
				{
					EnumThreadWindows(pi.dwThreadId, EnumThreadWndProc, pi.dwThreadId);
				}
				CloseHandle(pi.hProcess);
			}
		}
	}
	else
#endif
	{
		if (HWND hwnd = GetShellWindow())
		{
			if (ULONG dwThreadId = GetWindowThreadProcessId(hwnd, 0))
			{
				if (HHOOK hhk = SetWindowsHookExW(WH_CALLWNDPROC, CallWndProc, (HMODULE)&__ImageBase, dwThreadId))
				{
					SendMessageW(hwnd, WM_NULL, CONTROL_WPARAM_START, CONTROL_LPARAM_START);

					UnhookWindowsHookEx(hhk);
				}
			}
		}
	}

	return S_OK;
}

BOOLEAN APIENTRY DllMain(HMODULE hModule,
						 DWORD  ul_reason_for_call,
						 LPVOID ProcessTermination)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hModule);
		initterm();
		return TRUE;

	case DLL_PROCESS_DETACH:
		if (ProcessTermination)
		{
			EnumThreadWindows(GetCurrentThreadId(), EnumThreadWndProc2, 0);
		}
		destroyterm();
		break;
	}
	return FALSE;
}

_NT_END