#include "StdAfx.h"

_NT_BEGIN

#include "wizard.h"

CWizardChildDlg::CWizardChildDlg(HWND hwndParent)
{
	_hwndParent = hwndParent;
}

void CWizardChildDlg::Navigate(HWND hwndDlg, UINT uCmd)
{
	if (HWND hwnd = GetWindow(hwndDlg, uCmd))
	{
		ShowWindow(hwnd, SW_SHOW);
		ShowWindow(hwndDlg, SW_HIDE);
		SetFocus(hwnd);
	}
}

INT_PTR CWizardChildDlg::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_SHOWWINDOW:
		if (wParam && !lParam)
		{
			SendMessage(_hwndParent, WM_USER + WM_SHOWWINDOW, 0, (LPARAM)hwndDlg);
			OnShow(hwndDlg);
		}
		break;
	case WM_COMMAND:
		switch(wParam)
		{
		case IDCANCEL:
			Navigate(hwndDlg, GW_HWNDPREV);
			break;
		case IDOK:
			Navigate(hwndDlg, GW_HWNDNEXT);
			break;
		}
		break;
	}

	return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
}

void CWizardChildDlg::OnShow(HWND /*hwndDlg*/)
{

}

HWND CWizardChildDlg::_create(CWizardChildDlg* (*createObject)(HWND), HINSTANCE hInstance, LPCWSTR lpTemplateName, HWND hWndParent, LPARAM dwInitParam)
{
	HWND hwnd = 0;
	if (CWizardChildDlg* p = createObject(hWndParent))
	{
		hwnd = p->Create(hInstance, lpTemplateName, hWndParent, dwInitParam);
		p->Release();
	}
	return hwnd;
}

HWND CWizardChildDlg::_create(CWizardChildDlg* (*createObject)(HWND), LPCWSTR lpTemplateName, HWND hWndParent)
{
	return _create(createObject, (HINSTANCE)&__ImageBase, lpTemplateName, hWndParent, 0);
}

_NT_END