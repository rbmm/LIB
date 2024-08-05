#include "stdafx.h"

_NT_BEGIN

#include "../inc/rtlframe.h"
#include "resource.h"
#include "../winz/app.h"

struct FICON 
{
	HICON hIcon;
};

LRESULT CALLBACK CBTProc(int nCode, WPARAM wParam, LPARAM lParam )
{
	if (nCode == HCBT_CREATEWND)
	{
		CBT_CREATEWND* pccw = reinterpret_cast<CBT_CREATEWND*>(lParam);

		if (pccw->lpcs->lpszClass == WC_DIALOG)
		{
			if (FICON* p = RTL_FRAME<FICON>::get())
			{
				SendMessageW((HWND)wParam, WM_SETICON, ICON_SMALL, (LPARAM)p->hIcon);		
			}
		}
	}

	return CallNextHookEx(0, nCode, wParam, lParam);
}

int CustomMessageBox(HWND hWnd, PCWSTR lpText, PCWSTR lpszCaption, UINT uType)
{
	PCWSTR pszName = 0;

	switch (uType & MB_ICONMASK)
	{
	case MB_ICONINFORMATION:
		pszName = IDI_INFORMATION;
		break;
	case MB_ICONQUESTION:
		pszName = IDI_QUESTION;
		break;
	case MB_ICONWARNING:
		pszName = IDI_WARNING;
		break;
	case MB_ICONERROR:
		pszName = IDI_ERROR;
		break;
	}

	MSGBOXPARAMS mbp = {
		sizeof(mbp),
		IsWindowVisible(hWnd) ? hWnd : 0,
		(HINSTANCE)&__ImageBase,
		lpText, 
		lpszCaption, 
		(uType & ~MB_ICONMASK)|MB_USERICON,
		MAKEINTRESOURCE(IDR_MENU1)
	};

	HHOOK hhk = 0;
	RTL_FRAME<FICON> frame;
	frame.hIcon = 0;

	if (pszName && 0 <= LoadIconWithScaleDown(0, pszName, 
		GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), &frame.hIcon))
	{
		hhk = SetWindowsHookExW(WH_CBT, CBTProc, 0, GetCurrentThreadId());
	}

	int i = MessageBoxIndirect(&mbp);

	if (hhk) UnhookWindowsHookEx(hhk);
	if (frame.hIcon) DestroyIcon(frame.hIcon);

	return i;
}

HMODULE GetNtMod()
{
	static HMODULE s_hntmod;
	if (!s_hntmod)
	{
		s_hntmod = GetModuleHandle(L"ntdll");
	}

	return s_hntmod;
}

int ShowErrorBox(HWND hwnd, HRESULT dwError, PCWSTR lpCaption, UINT uType)
{
	if (!hwnd)
	{
		hwnd = ZGLOBALS::getMainHWND();
	}

	int r = 0;
	LPCVOID lpSource = 0;
	ULONG dwFlags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS;

	if ((dwError & FACILITY_NT_BIT) || (0 > dwError && HRESULT_FACILITY(dwError) == FACILITY_NULL))
	{
		dwError &= ~FACILITY_NT_BIT;
__nt:
		dwFlags = FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS;

		lpSource = GetNtMod();
	}

	PWSTR lpText;
	if (FormatMessageW(dwFlags, lpSource, dwError, 0, (PWSTR)&lpText, 0, 0))
	{
		if (!uType)
		{
			uType = (lpSource ? 0 > dwError : dwError) ? MB_ICONERROR : MB_ICONINFORMATION;
		}
		r = CustomMessageBox(hwnd, lpText, lpCaption, uType);
		LocalFree(lpText);
	}
	else if (dwFlags & FORMAT_MESSAGE_FROM_SYSTEM)
	{
		goto __nt;
	}

	return r;
}

_NT_END