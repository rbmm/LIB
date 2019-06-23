#include "stdafx.h"

_NT_BEGIN

#include "Wizard.h"

void CWizFrame::OnCancel(HWND hwndDlg)
{
	EndDialog(hwndDlg, IDCANCEL);
}

void CWizFrame::DoInstall()
{
}

void CWizFrame::SetStatusText(PCWSTR /*lpCaption*/, PCWSTR /*lpText*/)
{
}

void* CWizFrame::GetPageData(int /*iPage*/)
{
	return 0;
}

void CWizFrame::SetActivePage(CWizPage* pActivePage)
{
	if (_pActivePage)
	{
		_pActivePage->Release();
	}

	_pActivePage = pActivePage;

	pActivePage->AddRef();
}

void CWizFrame::OnInitDialog(HWND /*hwndDlg*/)
{
}

INT_PTR CWizFrame::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DESTROY:
		if (_pActivePage)
		{
			_pActivePage->Release();
			_pActivePage = 0;
		}
		break;

	case WM_INITDIALOG:
		_pActivePage = 0;
		OnInitDialog(hwndDlg);
		break;

	case WM_COMMAND:
		switch (wParam)
		{
		case IDCANCEL:
			OnCancel(hwndDlg);
			break;
		default:
			if (_pActivePage)
			{
				_pActivePage->OnFrameBtnClicked(wParam, (HWND)lParam);
			}
		}
		break;
	}
	return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
}

void CWizPage::SwitchToWindow(HWND hwnd)
{
	if (hwnd)
	{
		ShowWindow(hwnd, SW_SHOW);
		ShowWindow(getHWND(), SW_HIDE);
		SetFocus(hwnd);
	}
}

void CWizPage::GoBack()
{
	SwitchToWindow(GW_HWNDPREV);
}

void CWizPage::GoNext()
{
	SwitchToWindow(GW_HWNDNEXT);
}

void CWizPage::OnFrameBtnClicked(WPARAM cmd, HWND /*hwndCtrl*/)
{
	switch (cmd)
	{
	case IDABORT:
		GoBack();
		break;
	case IDOK:
		GoNext();
		break;
	}
}

void CWizPage::OnInitDialog(HWND /*hwndDlg*/)
{
}

void CWizPage::OnActivate(HWND /*hwndDlg*/, WPARAM /*bActivate*/)
{
}

INT_PTR CWizPage::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
		_pFrame = (CWizFrame*)lParam;
		_pFrame->AddRef();
		OnInitDialog(hwndDlg);
		break;

	case WM_SHOWWINDOW:
		if (!lParam)
		{
			if (wParam)
			{
				_pFrame->SetActivePage(this);
			}
			OnActivate(hwndDlg, wParam);
		}
		break;

	case WM_DESTROY:
		if (_pFrame)
		{
			_pFrame->Release();
			_pFrame = 0;
		}
		break;
	}

	return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
}

_NT_END