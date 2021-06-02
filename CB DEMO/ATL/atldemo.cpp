#include "pch.h"

#pragma warning(disable : 4706)

void ReferenceDll();
void DereferenceDll();

enum : ULONG {
	CONTROL_WPARAM_START = 0xB426E606,
	CONTROL_LPARAM_START = 0x4AC27B7B,
	CONTROL_WPARAM_STOP = CONTROL_LPARAM_START,
	CONTROL_LPARAM_STOP = CONTROL_WPARAM_START,
	CONTROL_WPARAM_TEST  = 0xE60EF176,
	CONTROL_LPARAM_TEST  = 0x6D0A351D,
	CONTROL_TEST_RETURN  = 0x041FDF4A,
};

class MySubClassBaseT : public CWindowImplBaseT<>
{
	ULONG dwRefCount = 1;

	static LRESULT CALLBACK StubWindowProc(
		_In_ HWND hWnd,
		_In_ UINT uMsg,
		_In_ WPARAM wParam,
		_In_ LPARAM lParam)ASM_FUNCTION;

	virtual WNDPROC GetWindowProc()
	{
		// force reference/include WindowProc
		return _ReturnAddress() ? StubWindowProc : WindowProc;
		//return StubWindowProc;
	}

protected:

	virtual void OnFinalMessage(_In_ HWND /*hwnd*/)
	{
		Release();
	}

	virtual ~MySubClassBaseT()
	{
		DereferenceDll();
	}

public:

	MySubClassBaseT()
	{
		ReferenceDll();
	}

	BOOL SubclassWindow(_In_ HWND hWnd)
	{
		if (__super::SubclassWindow(hWnd))
		{
			AddRef();
			return TRUE;
		}

		return FALSE;
	}

	HWND UnsubclassWindow(_In_ BOOL bForce /*= FALSE*/)
	{
		if (HWND hwnd = __super::UnsubclassWindow(bForce))
		{
			m_dwState |= WINSTATE_DESTROYED;
			m_hWnd = hwnd;
			return hwnd;
		}

		return 0;
	}

	void AddRef()
	{
		dwRefCount++;
	}

	void Release()
	{
		if (!--dwRefCount)
		{
			delete this;
		}
	}
};

class DemoSubClass2 : public MySubClassBaseT
{
	HICON _hi[4]{};

	virtual BOOL ProcessWindowMessage(
		_In_ HWND /*hWnd*/,
		_In_ UINT uMsg,
		_In_ WPARAM wParam,
		_In_ LPARAM lParam,
		_Inout_ LRESULT& lResult,
		_In_ DWORD /*dwMsgMapID*/)
	{
		switch (uMsg)
		{
		case WM_NULL:
			if (wParam == CONTROL_WPARAM_TEST && lParam == CONTROL_LPARAM_TEST)
			{
				lResult = CONTROL_TEST_RETURN;
				return TRUE;
			}

			if (wParam == CONTROL_WPARAM_STOP && lParam == CONTROL_LPARAM_STOP)
			{
				UnsubclassWindow(FALSE);
				lResult = 0;
				return TRUE;
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
				lResult = lParam;
				return TRUE;
			}
			break;
		}

		return FALSE;
	}

	virtual void OnFinalMessage(_In_ HWND hwnd)
	{
		HICON hi;

		SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)_hi[1]);
		SendMessageW(hwnd, WM_SETICON, ICON_SMALL2, (LPARAM)_hi[1]);
		SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)_hi[0]);

		if (hi = _hi[3]) DestroyIcon(hi);
		if (hi = _hi[2]) DestroyIcon(hi);

		__super::OnFinalMessage(hwnd);
	}

public:

	void SetIcons(HWND hwnd)
	{
		_hi[0] = (HICON)SendMessageW(hwnd, WM_GETICON, ICON_BIG, 0);
		_hi[1] = (HICON)SendMessageW(hwnd, WM_GETICON, ICON_SMALL2, 0);

		HICON hi;
		if (0 <= LoadIconWithScaleDown(0, IDI_SHIELD, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), &hi))
		{
			_hi[2] = hi;
			SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hi);
		}

		if (0 <= LoadIconWithScaleDown(0, IDI_SHIELD, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), &hi))
		{
			_hi[3] = hi;
			SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hi);
			SendMessageW(hwnd, WM_SETICON, ICON_SMALL2, (LPARAM)hi);
		}
	}
};

class DemoSubClass : public MySubClassBaseT
{
	virtual void OnFinalMessage(_In_ HWND hwnd)
	{
		SendMessageW(hwnd, EM_SETSEL, MAXLONG, MAXLONG);
		SendMessageW(hwnd, EM_REPLACESEL, 0, (LPARAM)L"\r\n**** [ --- ] ****\r\n");

		if (HWND hwndParent = GetAncestor(hwnd, GA_PARENT))
		{
			SendMessageW(hwndParent, WM_NULL, CONTROL_WPARAM_STOP, CONTROL_LPARAM_STOP);
		}

		__super::OnFinalMessage(hwnd);
	}

	virtual BOOL ProcessWindowMessage(
		_In_ HWND hwnd,
		_In_ UINT uMsg,
		_In_ WPARAM wParam,
		_In_ LPARAM lParam,
		_Inout_ LRESULT& lResult,
		_In_ DWORD /*dwMsgMapID*/)
	{
		switch (uMsg)
		{
		case WM_NULL:
			if (wParam == CONTROL_WPARAM_TEST && lParam == CONTROL_LPARAM_TEST)
			{
				lResult = CONTROL_TEST_RETURN;
				return TRUE;
			}

			if (wParam == CONTROL_WPARAM_START && lParam == CONTROL_LPARAM_START)
			{
				DefWindowProc(EM_SETSEL, MAXLONG, MAXLONG);
				DefWindowProc(EM_REPLACESEL, 0, (LPARAM)L"\r\n**** [ +++ ] ****\r\n");

				if (HWND hwndParent = GetAncestor(hwnd, GA_PARENT))
				{
					if (CallWindowProcW((WNDPROC)::GetWindowLongPtrW(hwndParent, GWLP_WNDPROC), hwndParent, 
						WM_NULL, CONTROL_WPARAM_TEST, CONTROL_LPARAM_TEST) != CONTROL_TEST_RETURN)
					{
						if (DemoSubClass2* p = new DemoSubClass2)
						{
							if (p->SubclassWindow(hwndParent))
							{
								p->SetIcons(hwndParent);
							}
							p->Release();
						}
					}
				}
				lResult = 0;
				return TRUE;
			}
			break;

		case WM_CHAR:
			if (wParam - '0' <= '9' - '0')
			{
				if (wParam++ == '9')
				{
					wParam = '0';
				}
				lResult = DefWindowProc(uMsg, wParam, lParam);
				return TRUE;
			}

			switch (wParam)
			{
			case VK_RETURN:
				::MessageBoxW(hwnd, L"Inject Active", L"[v1.0]", MB_ICONINFORMATION);
				break;
			case VK_ESCAPE:
				UnsubclassWindow(FALSE);
				break;
			}
			break;
		}

		return FALSE;
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
					p->SubclassWindow(hwnd);
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
