#include "stdafx.h"

_NT_BEGIN

#include "dragptr.h"
#include "app.h"

ZDragPtr::ZDragPtr()
{
	_himl = 0, _txt = 0;
}

ZDragPtr::~ZDragPtr()
{
	if (_himl)
	{
		ImageList_Destroy(_himl);
	}

	if (_txt)
	{
		delete _txt;
	}
}

void ZDragPtr::SetText(PCWSTR txt)
{
	if (_txt)
	{
		delete _txt;
		_txt = 0;
	}

	if (txt)
	{
		SIZE_T len = wcslen(txt);

		if (_txt = new WCHAR[len + 1])
		{
			wcscpy(_txt, txt);
		}
	}
}

void ZDragPtr::EndDrag()
{
	if (_himl)
	{
		ReleaseCapture();
		ShowCursor(TRUE);
		ImageList_DragLeave(0);
		ImageList_EndDrag();
		ImageList_Destroy(_himl);
		_himl = 0;

		POINT pt;
		if (GetCursorPos(&pt))
		{
			if (HWND hwnd = WindowFromPoint(pt))
			{
				SendMessage(hwnd, WM_DROP, (WPARAM)_txt, _ptr);
			}
		}
	}

	if (_txt)
	{
		delete _txt;
		_txt = 0;
	}
}

void ZDragPtr::DragMove(HWND hwnd, int x, int y)
{
	POINT pt = { x, y };
	ClientToScreen(hwnd, &pt);
	ImageList_DragMove(pt.x, pt.y);
}

BOOL ZDragPtr::BeginDrag(HWND hwnd, int x, int y, int dxHotspot, int dyHotspot, DWORD l)
{
	if (_himl)
	{
		return FALSE;
	}

	HBITMAP hbmp = 0;

	WCHAR buf[64], *sz = buf;

	DWORD len;

	if (_txt)
	{
		len = (DWORD)wcslen(sz = _txt);
	}
	else
	{
		len = swprintf(sz, L"%p", (void*)_ptr);

		if (len > l)
		{
			sz += len - l;
			len = l;
		}
	}

	SIZE u;
	ZFont* font = ZGLOBALS::getFont();
	font->getSIZE(&u);

	int i = -1;
	HIMAGELIST himl = 0;

	RECT rc = { 0, 0, len * u.cx + 4, u.cy + 2 };

	if (HDC hdc = GetDC(0))
	{
		BITMAPINFO bi = {{ sizeof(BITMAPINFOHEADER), rc.right, rc.bottom, 1, 32}};

		PDWORD pv;

		if (hbmp = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, (void**)&pv, 0, 0))
		{
			if (HDC hmemDC = CreateCompatibleDC(hdc))
			{
				HGDIOBJ hgo = SelectObject(hmemDC, hbmp);

				HGDIOBJ hf = SelectObject(hmemDC, font->getFont());

				SetBkColor(hmemDC, RGB(255, 0, 0));
				SetTextColor(hmemDC, RGB(255, 255, 255));

				ExtTextOut(hmemDC, 2, 1, ETO_CLIPPED|ETO_OPAQUE, &rc, sz, len, 0);
				FrameRect(hmemDC, &rc, GetSysColorBrush(COLOR_MENUTEXT));

				SelectObject(hmemDC, hf);
				SelectObject(hmemDC, hgo);

				DeleteDC(hmemDC);
			}

			DWORD n = rc.right * rc.bottom;
			do 
			{
				*pv++ |= 0xff000000;
			} while (--n);

			if (himl = ImageList_Create(rc.right, rc.bottom, ILC_COLOR32, 1, 0))
			{
				i = ImageList_Add(himl, hbmp, 0);
			}

			DeleteObject(hbmp);
		}

		ReleaseDC(0, hdc);
	}

	if (0 <= i)
	{
		if (ImageList_BeginDrag(himl, i, dxHotspot + 2, dyHotspot + 1))
		{
			POINT pt = { x, y };
			ClientToScreen(hwnd, &pt);

			if (ImageList_DragEnter(0, pt.x, pt.y))
			{
				_himl = himl;
				ShowCursor(FALSE);
				SetCapture(hwnd);
				return TRUE;
			}

			ImageList_EndDrag();
		}
	}

	if (himl)
	{
		ImageList_Destroy(himl);
	}

	return FALSE;
}

_NT_END