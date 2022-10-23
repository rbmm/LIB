#pragma once

#include "../winz/window.h"
#include "../winz/app.h"

class ZFontDlg : public ZDlg, ZFont
{
	ZFont* _font;
	HWND _hwndSample;
	int _s;

	void SetFont();
	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

public:

	~ZFontDlg();
	ZFontDlg();
	HWND Create(HWND hWndParent);
};

void ChoseFont(HWND hWndParent);
