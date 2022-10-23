#pragma once

#include "../winz/window.h"
#include "../winz/ctrl.h"

class ZAddressWnd : public ZWnd, ZTranslateMsg, ZToolBar, CUILayot
{
	HWND _hwndTo;
	~ZAddressWnd();

	virtual BOOL PreTranslateMessage(PMSG lpMsg);
	virtual LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
public:
	ZAddressWnd(HWND hwndTo);
	HWND Create(int x, int y, int nWidth, int nHeight, HWND hWndParent);
};
