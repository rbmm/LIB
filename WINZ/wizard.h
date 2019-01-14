#pragma once

#include "../winZ/window.h"

class __declspec(novtable) CWizardChildDlg : public ZDlg
{
protected:
	HWND _hwndParent;

	CWizardChildDlg(HWND hwndParent);

	virtual void Navigate(HWND hwndDlg, UINT uCmd);

	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

	virtual void OnShow(HWND hwndDlg);

public:

	static HWND _create(CWizardChildDlg* (*createObject)(HWND), HINSTANCE hInstance, LPCWSTR lpTemplateName, HWND hWndParent, LPARAM dwInitParam);

	static HWND _create(CWizardChildDlg* (*createObject)(HWND), LPCWSTR lpTemplateName, HWND hWndParent);
};

