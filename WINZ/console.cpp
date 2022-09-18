#include "stdafx.h"

_NT_BEGIN

#include "console.h"

void ZConsole::SetFont(HWND hwnd)
{
	NONCLIENTMETRICS ncm = { GetNONCLIENTMETRICSWSize() };
	LOGFONT lf;
	if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
	{
		memcpy(&lf, &ncm.lfMenuFont, sizeof(LOGFONT));
		lf.lfHeight = -ncm.iMenuHeight;
		lf.lfItalic = 0;
		lf.lfUnderline = 0;
		lf.lfStrikeOut = 0;
		lf.lfWeight = FW_NORMAL;
		lf.lfQuality = CLEARTYPE_QUALITY;
		lf.lfPitchAndFamily = FIXED_PITCH|FF_MODERN;
		wcscpy(lf.lfFaceName, L"Lucida Console");

		if (HFONT hfont = CreateFontIndirect(&lf))
		{
			_hFont = hfont;
			SendMessage(hwnd, WM_SETFONT, (WPARAM)hfont, 0);
		}
	}
}

PCUNICODE_STRING ZConsole::getPosName()
{
	STATIC_UNICODE_STRING_(console);
	return &console;
}

HWND ZConsole::CreateView(HWND hWndParent, int nWidth, int nHeight, PVOID lpCreateParams)
{
	HWND hwnd = CreateWindowExW(0, WC_EDIT, 0, WS_VISIBLE|WS_CHILD|ES_AUTOVSCROLL|ES_MULTILINE|WS_VSCROLL|WS_HSCROLL,
		0, 0, nWidth, nHeight, hWndParent, (HMENU)1, 0, 0);

	if (hwnd)
	{
		SetFont(hwnd);

		if (lpCreateParams)
		{
			SetIcons(hWndParent, 
				reinterpret_cast<INID*>(lpCreateParams)->hInstance, 
				reinterpret_cast<INID*>(lpCreateParams)->id);
		}
	}
	return hwnd;
}

LRESULT ZConsole::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_NCCREATE:
		_hFont = 0;
		_hBrush = GetStockObject(BLACK_BRUSH);
		if (InterlockedCompareExchangePointerNoFence((void**)&g_ConHWND, (void*)hwnd, 0)) return FALSE;
		break;

	case WM_DESTROY:
		g_ConHWND = 0;
		if (_hFont)
		{
			DeleteObject(_hFont);
		}
		break;

	case WM_NCDESTROY:
		RundownGUI();
		break;

	case WM_CTLCOLOREDIT:
		SetTextColor((HDC)wParam, RGB(0,255,0));
		SetBkColor((HDC)wParam, RGB(0,0,0));
		return (LPARAM)_hBrush;

	case WM_APP + WM_PRINTCLIENT:
		SendMessage(_hwndView, EM_SETSEL, MAXLONG, MAXLONG);
		SendMessage(_hwndView, EM_REPLACESEL, 0, lParam);	
		break;
	}
	return ZFrameWnd::WindowProc(hwnd, uMsg, wParam, lParam);
}

BOOL ZConsole::CanClose()
{
	if (_ZGLOBALS* p = ZGLOBALS::get())
	{
		if (p->App)
		{
			return p->App->CanClose(getHWND());
		}
	}
	return TRUE;
}

void WINAPI ZConsole::_print(PCWSTR buf)
{
	SendMessage(g_ConHWND, WM_APP + WM_PRINTCLIENT, 0, (LPARAM)buf);
}

void WINAPI ZConsole::vprint(PCWSTR format, va_list args)
{
	int len = _vscwprintf(format, args);
	if (0 < len)
	{
		if (PWSTR buf = (PWSTR)_malloca((len + 1) * sizeof(WCHAR)))
		{
			len = _vsnwprintf(buf, len, format, args);
			if (0 < len)
			{
				buf[len] = 0;
				_print(buf);
			}
			_freea(buf);
		}
	}
}	

void ZConsole::print(PCWSTR format, ...)
{
	vprint(format, va_list(&format + 1));
}

_NT_END