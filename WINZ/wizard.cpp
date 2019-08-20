#include "stdafx.h"

_NT_BEGIN

#include "../inc/idcres.h"
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

BOOL CWizFrame::OnInitDialog(HWND /*hwndDlg*/)
{
	return TRUE;
}

INT_PTR CWizFrame::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CTLCOLORSTATIC:
		switch (GetDlgCtrlID((HWND)lParam))
		{
		case IDC_STATIC-1:
		case IDC_STATIC-2:
			return FALSE;
		}
		SetTextColor((HDC)wParam, (INT_PTR)GetSysColor(COLOR_WINDOWTEXT));
		SetBkMode((HDC)wParam, TRANSPARENT);
		[[fallthrough]];

	case WM_CTLCOLORDLG:
		return (INT_PTR)GetSysColorBrush(COLOR_WINDOW);

	case WM_PAINT:
		PAINTSTRUCT ps;
		if (BeginPaint(hwndDlg, &ps))
		{
			RECT rc;
			GetWindowRect(GetDlgItem(hwndDlg, IDC_STATIC-1), &rc);
			ScreenToClient(hwndDlg, 1+(PPOINT)&rc);
			GetClientRect(hwndDlg, &ps.rcPaint);
			ps.rcPaint.top = rc.bottom;
			FillRect(ps.hdc, &ps.rcPaint, GetSysColorBrush(COLOR_MENU));
			EndPaint(hwndDlg, &ps);
		}
		return TRUE;

	case WM_DESTROY:
		if (_pActivePage)
		{
			_pActivePage->Release();
			_pActivePage = 0;
		}
		break;

	case WM_INITDIALOG:
		_pActivePage = 0;
		return OnInitDialog(hwndDlg);

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

BOOL CWizPage::OnInitDialog(HWND /*hwndDlg*/)
{
	return TRUE;
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
		return OnInitDialog(hwndDlg);

	case WM_SETFOCUS:
		return TRUE;

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