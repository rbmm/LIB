#include "stdafx.h"

_NT_BEGIN
//#define _PRINT_CPP_NAMES_
#include "../inc/initterm.h"
#include "../winZ/subclass.h"
#include "../winZ/wic.h"

enum : ULONG {
	CONTROL_WPARAM_START = 0xB426E606,
	CONTROL_LPARAM_START = 0x4AC27B7B,
	CONTROL_WPARAM_STOP = CONTROL_LPARAM_START,
	CONTROL_LPARAM_STOP = CONTROL_WPARAM_START,
	CONTROL_WPARAM_TEST  = 0xE60EF176,
	CONTROL_LPARAM_TEST  = 0x6D0A351D,
	CONTROL_TEST_RETURN  = 0x041FDF4A,
};

class DemoSubClass2 : public ZSubClass
{
	HICON _hi[4]{};

	virtual void AfterLastMessage()
	{
		union {
			HWND hwnd;
			HICON hi;
		};

		SendMessageW(hwnd = getHWND(), WM_SETICON, ICON_SMALL, (LPARAM)_hi[1]);
		SendMessageW(hwnd, WM_SETICON, ICON_SMALL2, (LPARAM)_hi[1]);
		SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)_hi[0]);

		if (hi = _hi[3])
		{
			DestroyIcon(hi);
		}

		if (hi = _hi[2])
		{
			DestroyIcon(hi);
		}
	}

	LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg)
		{
		case WM_NULL:
			if (wParam == CONTROL_WPARAM_TEST && lParam == CONTROL_LPARAM_TEST)
			{
				return CONTROL_TEST_RETURN;
			}

			if (wParam == CONTROL_WPARAM_STOP && lParam == CONTROL_LPARAM_STOP)
			{
				Unsubclass(hwnd);
				return 0;
			}

			break;
		case WM_GETICON:
			switch (wParam)
			{
			case ICON_BIG:
				lParam = (LRESULT)_hi[2];
				break;
			case ICON_SMALL:
			case ICON_SMALL2:
				lParam = (LRESULT)_hi[3];
				break;
			default:lParam = 0;
			}
			if (lParam)
			{
				return lParam;
			}
			break;
		}

		return __super::WindowProc(hwnd, uMsg, wParam, lParam);
	}
public:
	void SetIcons(HWND hwnd)
	{
		_hi[0] = (HICON)SendMessageW(hwnd, WM_GETICON, ICON_BIG, 0);
		_hi[1] = (HICON)SendMessageW(hwnd, WM_GETICON, ICON_SMALL2, 0);

		LIC l;

		l._hi = 0;
		l._cx = GetSystemMetrics(SM_CXICON);
		l._cy = GetSystemMetrics(SM_CYICON);
		if (0 <= l.CreateIconFromPNG(MAKEINTRESOURCEW(1)))
		{
			_hi[2] = l._hi;
			SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)l._hi);
		}

		l._hi = 0;
		l._cx = GetSystemMetrics(SM_CXSMICON);
		l._cy = GetSystemMetrics(SM_CYSMICON);
		if (0 <= l.CreateIconFromPNG(MAKEINTRESOURCEW(1)))
		{
			_hi[3] = l._hi;
			SendMessageW(hwnd, WM_SETICON, ICON_SMALL2, (LPARAM)l._hi);
		}
	}
};

class DemoSubClass : public ZSubClass
{
	virtual void AfterLastMessage()
	{
		HWND hwnd = getHWND();
		__super::WindowProc(hwnd, EM_SETSEL, MAXLONG, MAXLONG);
		__super::WindowProc(hwnd, EM_REPLACESEL, 0, (LPARAM)L"\r\n**** [ --- ] ****\r\n");

		if (hwnd = GetAncestor(hwnd, GA_PARENT))
		{
			SendMessageW(hwnd, WM_NULL, CONTROL_WPARAM_STOP, CONTROL_LPARAM_STOP);
		}
	}

	LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg)
		{
		case WM_NULL:
			if (wParam == CONTROL_WPARAM_TEST && lParam == CONTROL_LPARAM_TEST)
			{
				return CONTROL_TEST_RETURN;
			}

			if (wParam == CONTROL_WPARAM_START && lParam == CONTROL_LPARAM_START)
			{
				__super::WindowProc(hwnd, EM_SETSEL, MAXLONG, MAXLONG);
				__super::WindowProc(hwnd, EM_REPLACESEL, 0, (LPARAM)L"\r\n**** [ +++ ] ****\r\n");

				if (HWND hwndParent = GetAncestor(hwnd, GA_PARENT))
				{
					if (CallWindowProcW((WNDPROC)GetWindowLongPtrW(hwndParent, GWLP_WNDPROC), hwndParent, 
						WM_NULL, CONTROL_WPARAM_TEST, CONTROL_LPARAM_TEST) != CONTROL_TEST_RETURN)
					{
						if (DemoSubClass2* p = new DemoSubClass2)
						{
							if (p->Subclass(hwndParent))
							{
								p->SetIcons(hwndParent);
							}
							p->Release();
						}
					}
				}
				return 0;
			}

			break;
		case WM_CHAR:
			if (wParam - '0' <= '9' - '0')
			{
				if (wParam++ == '9')
				{
					wParam = '0';
				}
				break;
			}
			switch (wParam)
			{
			case VK_RETURN:
				MessageBoxW(hwnd, L"Inject Active", L"[v1.0]", MB_ICONINFORMATION);
				break;
			case VK_ESCAPE:
				Unsubclass(hwnd);
				break;
			}
			break;
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
			HWND hwnd = pcs->hwnd;
			if (CallWindowProcW((WNDPROC)GetWindowLongPtrW(hwnd, GWLP_WNDPROC), hwnd, 
				WM_NULL, CONTROL_WPARAM_TEST, CONTROL_LPARAM_TEST) != CONTROL_TEST_RETURN)
			{
				if (DemoSubClass* p = new DemoSubClass)
				{
					p->Subclass(hwnd);
					p->Release();
				}
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
						 LPVOID /*lpReserved*/)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hModule);
		initterm();
		return TRUE;

	case DLL_PROCESS_DETACH:
		destroyterm();
		break;
	}
	return FALSE;
}

_NT_END