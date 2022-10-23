#include "stdafx.h"

_NT_BEGIN

#include "../winz/window.h"
#include "common.h"
#include "fontDlg.h"
#include "resource.h"
#include "../inc/idcres.h"

struct ENUM_DATA 
{
	PCWSTR lfFaceName;
	HWND hwnd;
};

int CALLBACK EnumFontFamExProc(
							   LOGFONT *lpelfe,
							   NEWTEXTMETRIC * /*lpntme*/,
							   DWORD /*FontType*/,
							   ENUM_DATA* p
							   )
{
	if (
		(lpelfe->lfPitchAndFamily & (FIXED_PITCH|FF_MODERN)) == (FIXED_PITCH|FF_MODERN) &&
		lpelfe->lfFaceName[0] != '@'
		)
	{
		int i = ComboBox_AddString(p->hwnd, lpelfe->lfFaceName);
		if (!_wcsicmp(lpelfe->lfFaceName, p->lfFaceName))
		{
			ComboBox_SetCurSel(p->hwnd, i);
		}
	}
	return TRUE;
}

ZFontDlg::ZFontDlg() : ZFont(FALSE)
{
	if (ZToolBar* tb = ZGLOBALS::getMainFrame())
	{
		tb->EnableCmd(IDB_BITMAP17, FALSE);
	}
}

ZFontDlg::~ZFontDlg()
{
	if (ZToolBar* tb = ZGLOBALS::getMainFrame())
	{
		tb->EnableCmd(IDB_BITMAP17, TRUE);
	}
}

void ZFontDlg::SetFont()
{
	if (SetNewFont(this, FALSE))
	{
		SendMessage(_hwndSample, WM_SETFONT, (WPARAM)getFont(), TRUE);
	}
}

int FillSizes(HWND hwndctl, int lfHeight)
{
	int s = 0, a, b, j;
	if (hwndctl)
	{
		NONCLIENTMETRICS ncm = { GetNONCLIENTMETRICSWSize() };
		if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
		{
			s = ncm.lfMessageFont.lfHeight < 0 ? -1 : +1;
			a = abs(ncm.lfMessageFont.lfHeight), b = abs(ncm.lfCaptionFont.lfHeight);
			if (a <= b)
			{
				b <<= 1;
				do 
				{
					WCHAR sz[32];
					swprintf(sz, L"%2u", a);
					j = ComboBox_AddString(hwndctl, sz);
					if (a == lfHeight)
					{
						ComboBox_SetCurSel(hwndctl, j);
					}
				} while (++a < b);
			}
		}
	}

	return s;
}

INT_PTR ZFontDlg::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	WCHAR sz[32];
	switch (uMsg)
	{
	case WM_DESTROY:
		_hwndSample = 0;
		break;

	case WM_INITDIALOG:
		SendMessage(_hwndSample = GetDlgItem(hwndDlg, IDC_STATIC1), WM_SETFONT, (WPARAM)getFont(), 0);

		if (HWND hwndctl = GetDlgItem(hwndDlg, IDC_COMBO1))
		{
			if (HDC hdc = GetDC(0))
			{
				LOGFONT lf = {};
				ENUM_DATA o = { lfFaceName, hwndctl };
				EnumFontFamiliesEx(hdc, &lf, (FONTENUMPROCW)EnumFontFamExProc, (LPARAM)&o, 0);
				ReleaseDC(0, hdc);
				_s = FillSizes(GetDlgItem(hwndDlg, IDC_COMBO2), abs(lfHeight));
			}
		}

		if (!_s)
		{
			DestroyWindow(hwndDlg);
			return 0;
		}

		SetDlgItemText(hwndDlg, IDC_STATIC1, 
			L"01234567890123456789012345\r\n"
			L"ABCDEFGHIJKLMNOPQRSTUVWXYZ\r\n"
			L"abcdefghijklmnopqrstuvwxwz\r\n"
			L",./|\\(){}[]<>!@#$%^_*`~-+=");
		break;

	case WM_COMMAND:
		switch (wParam)
		{
		case IDOK:
			_font->SetNewFont(this, TRUE);
		case IDCANCEL:
			DestroyWindow(hwndDlg);
			break;

		case MAKEWPARAM(IDC_COMBO2, CBN_SELCHANGE):
			if (ComboBox_GetText((HWND)lParam, sz, RTL_NUMBER_OF(sz)))
			{
				PWSTR c;
				int i = wcstoul(sz, &c, 10);
				if (!*c)
				{
					lfHeight = i * _s;
					SetFont();
				}
			}
			break;

		case MAKEWPARAM(IDC_COMBO1, CBN_SELCHANGE):
			if (ComboBox_GetText((HWND)lParam, lfFaceName, LF_FACESIZE))
			{
				SetFont();
			}
			break;
		}
		break;
	}

	return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
}

HWND ZFontDlg::Create(HWND hWndParent)
{
	if (_font = ZGLOBALS::getFont())
	{
		if (CopyFont(_font))
		{
			return  ZDlg::Create((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDD_DIALOG2), hWndParent, 0);
		}
	}

	return 0;
}

void ChoseFont(HWND hWndParent)
{
	if (ZFontDlg* p = new ZFontDlg)
	{
		p->Create(hWndParent);
		p->Release();
	}
}

_NT_END