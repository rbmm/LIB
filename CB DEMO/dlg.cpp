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

	BOOL CreateDC()
	{
		if (HDC hMemDC = CreateCompatibleDC(0))
		{
			_hMemDC = hMemDC;
			return TRUE;
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
			if (!CreateDC() || 
				!CreateImage(reinterpret_cast<CREATESTRUCT*>(lParam)->cx, reinterpret_cast<CREATESTRUCT*>(lParam)->cy))
			{
				return FALSE;
			}
			break;
		case WM_NCDESTROY:

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
			_bittestandreset(&_Flags, bMousePressed);

			return 0;
		}
		return __super::WindowProc(hwnd, uMsg, wParam, lParam);
	}

	static LRESULT CALLBACK CustomStartWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		if (uMsg == WM_NCCREATE)
		{
			if (StartButton* p = new StartButton)
			{
				lParam = p->MStartWindowProc(hwnd, uMsg, wParam, lParam);
				p->Release();
				return lParam;
			}

			return FALSE;
		}

		return DefWindowProcW(hwnd, uMsg, wParam, lParam);
	}

	struct CustomWnd 
	{
		CustomWnd()
		{
			WNDCLASS cls = {
				0, CustomStartWindowProc, 0, 0, (HINSTANCE)&__ImageBase, 0, 
				LoadCursorW(0, IDC_ARROW), 
				0, 0, L"D27CDB6EAE6D11CF96B8444553540000"
			};

			RegisterClassW(&cls);
		}

		~CustomWnd()
		{
			if (!UnregisterClassW(L"D27CDB6EAE6D11CF96B8444553540000", (HINSTANCE)&__ImageBase))
			{
				if (GetLastError() != ERROR_CLASS_DOES_NOT_EXIST)
				{
					__debugbreak();
				}
			}
		}
	};

	inline static CustomWnd s;
};

class DemoDlg : public ZDlg
{
public:
protected:
private:
	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg)
		{
		case WM_COMMAND:
			switch (wParam)
			{
			case IDCANCEL:
			case IDOK:
				DestroyWindow(hwndDlg);
				break;
			}
			break;
		}
		return __super::DialogProc(hwndDlg, uMsg, wParam, lParam);
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
			if (DemoDlg* p = new DemoDlg)
			{
				p->Create((HINSTANCE)&__ImageBase, MAKEINTRESOURCEW(IDD_DIALOG1), pcs->hwnd, 0);
				p->Release();
			}
		}
	}

	return CallNextHookEx(0, nCode, wParam, lParam);
}

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