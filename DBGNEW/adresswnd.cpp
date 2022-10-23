#include "StdAfx.h"

_NT_BEGIN

#include "adresswnd.h"
#include "../inc/idcres.h"

ZAddressWnd::ZAddressWnd(HWND hwndTo)
{
	_hwndTo = hwndTo;
	DbgPrint("++ZAddressWnd(%p)\n", this);
}

ZAddressWnd::~ZAddressWnd()
{
	DbgPrint("--ZAddressWnd(%p)\n", this);
}

BOOL ZAddressWnd::PreTranslateMessage(PMSG lpMsg)
{
	if (lpMsg->message == WM_CHAR && lpMsg->wParam == VK_RETURN)
	{
		if (_hwndTo)
		{
			SendMessage(_hwndTo, WM_COMMAND, IDOK, 0);
		}
		return TRUE;
	}
	return FALSE;
}

LRESULT ZAddressWnd::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	STATIC_WSTRING(szAddress, " Address ");
	static SIZE s;
	RECT rc;
	static const TBBUTTON g_btns[] = {
		{ IMAGE_ENHMETAFILE, IDB_BITMAP26, TBSTATE_ENABLED, BTNS_AUTOSIZE|BTNS_SHOWTEXT, {}, (DWORD_PTR)L"Go", (INT_PTR)L"Go"},
	};
	HWND hwndCtl;
	HFONT hFont;
	switch (uMsg)
	{
	//case WM_CHAR:
	//	DbgBreak();
	//	break;
	case WM_COMMAND:
		switch (wParam)
		{
		case MAKEWPARAM(AFX_IDW_PANE_FIRST, CBN_SETFOCUS):
			Insert();
			return 0;
		case MAKEWPARAM(AFX_IDW_PANE_FIRST, CBN_KILLFOCUS):
			Remove();
			return 0;
		case MAKEWPARAM(AFX_IDW_PANE_FIRST, CBN_SELCHANGE):
			if (_hwndTo)
			{
				return SendMessage(_hwndTo, uMsg, wParam, lParam);
			}
		case IDB_BITMAP26:
			if (_hwndTo)
			{
				SendMessage(_hwndTo, WM_COMMAND, IDOK, 0);
			}
			break;
		}
		break;
	case WM_NOTIFY:
		switch (LOWORD(wParam))
		{
		case AFX_IDW_TOOLBAR:
			if (uMsg == WM_NOTIFY && ((LPNMHDR)lParam)->code == TBN_GETINFOTIPW)
			{
				LPNMTBGETINFOTIP lpttt = (LPNMTBGETINFOTIP)lParam;
				HWND hw = ZToolBar::getHWND();
				TBBUTTON tb;
				if (SendMessage(hw, TB_GETBUTTON, 
					SendMessage(hw, TB_COMMANDTOINDEX, lpttt->iItem, 0), (LPARAM)&tb))
				{
					wcscpy(lpttt->pszText, (PCWSTR)tb.dwData);
				}
				return 0;
			}
		case AFX_IDW_PANE_FIRST:
			if (_hwndTo)
			{
				return SendMessage(_hwndTo, uMsg, wParam, lParam);
			}
		}
		break;

	case WM_SIZE:
		Resize(wParam, lParam);
		break;

	case WM_CREATE:
		GetClientRect(hwnd, &rc);

		hFont = ZGLOBALS::get()->Font->getStatusFont();

		if (!s.cx)
		{
			if (HDC hdc = GetDC(0))
			{
				HGDIOBJ o = SelectObject(hdc, hFont);
				GetTextExtentPoint32(hdc, szAddress, RTL_NUMBER_OF(szAddress) - 1, &s);
				SelectObject(hdc, o);
				ReleaseDC(0, hdc);
			}

			if (!s.cx)
			{
				return -1;
			}
		}

		if (hwndCtl = CreateWindowEx(0, WC_STATIC, szAddress, WS_CHILD|WS_VISIBLE|SS_CENTER|SS_CENTERIMAGE, 
			0, 0, s.cx, rc.bottom, hwnd, 0, 0, 0))
		{
			if (hFont) SendMessage(hwndCtl, WM_SETFONT, (WPARAM)hFont, 0);
		}

		if (hwndCtl = CreateWindowEx(0, WC_COMBOBOX, 0, WS_CHILD|WS_VISIBLE|CBS_DROPDOWN, 
			s.cx, 3, rc.right - (22+4*s.cx/9) - s.cx, 0, hwnd, (HMENU)AFX_IDW_PANE_FIRST, 0, 0))
		{
			COMBOBOXINFO cbi = { sizeof(cbi) };
			if (hFont) SendMessage(hwndCtl, WM_SETFONT, (WPARAM)hFont, 0);
			GetComboBoxInfo(hwndCtl, &cbi);
			SendMessage(cbi.hwndItem, EM_SETMARGINS, EC_LEFTMARGIN|EC_RIGHTMARGIN , MAKELONG(4,4) );
			ComboBox_SetMinVisible(hwndCtl, 10);
			SendMessage(_hwndTo, WM_USER, AFX_IDW_PANE_FIRST, (LPARAM)&cbi);
		}

		ZToolBar::Create(hwnd, (HINSTANCE)&__ImageBase, rc.right - (22+4*s.cx/9), 1, 20, 20, g_btns, RTL_NUMBER_OF(g_btns), TRUE);
		CreateLayout(hwnd);
		break;
	case WM_ERASEBKGND:
		return ZToolBar::EraseBackground((HDC)wParam, hwnd);
	case WM_PAINT:
		PAINTSTRUCT ps;
		if (BeginPaint(hwnd, &ps))
		{
			FillRect(ps.hdc, &ps.rcPaint, (HBRUSH)(1 + COLOR_MENUBAR));
			EndPaint(hwnd, &ps);
		}
		break;
	}
	return ZWnd::WindowProc(hwnd, uMsg, wParam, lParam);
}

HWND ZAddressWnd::Create(int x, int y, int nWidth, int nHeight, HWND hWndParent)
{
	return ZWnd::Create(0, 0, WS_CHILD|WS_VISIBLE, x, y, nWidth, nHeight, hWndParent, 0, 0);
}

_NT_END