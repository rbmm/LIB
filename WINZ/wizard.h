#pragma once

#include "../winz/window.h"

class CWizPage;

class __declspec(novtable) CWizFrame : public ZDlg 
{
	friend CWizPage;

	CWizPage* _pActivePage;

	virtual void OnInitDialog(HWND hwndDlg);

	virtual void DoInstall();

	virtual void SetStatusText(PCWSTR lpCaption, PCWSTR lpText);

protected:

	CWizFrame() : _pActivePage(0) {}

	virtual void OnCancel(HWND hwndDlg);

	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

	void SetActivePage(CWizPage* pActivePage);
public:
	virtual void* GetPageData(int iPage);
};

class __declspec(novtable) CWizPage : public ZDlg 
{
	friend CWizFrame;

	CWizFrame* _pFrame;

	virtual void OnInitDialog(HWND hwndDlg);

	virtual void OnActivate(HWND hwndDlg, WPARAM bActivate);

protected:

	CWizPage() : _pFrame(0) { }

	void SwitchToWindow(HWND hwnd);

	void SwitchToWindow(ULONG uCmd)
	{
		SwitchToWindow(GetWindow(getHWND(), uCmd));
	}

	void DoInstall()
	{
		_pFrame->DoInstall();
	}

	void* GetPageData(int iPage)
	{
		return _pFrame->GetPageData(iPage);
	}

	void SetStatusText(PCWSTR lpCaption, PCWSTR lpText)
	{
		_pFrame->SetStatusText(lpCaption, lpText);
	}

	virtual void GoBack();

	virtual void GoNext();

	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

	virtual void OnFrameBtnClicked(WPARAM cmd, HWND hwndCtrl);
};