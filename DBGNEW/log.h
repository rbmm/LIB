#pragma once

#include "dbgdoc.h"
#include "../winZ/mdi.h"

class ZLogFrame : public ZMDIChildFrame, ZFontNotify, ZView
{
	virtual ZView* getView();

	virtual ZWnd* getWnd();

	virtual BOOL CanCloseFrame();

	virtual BOOL CanClose();

	virtual void DestroyView();

	virtual HWND CreateView(HWND hWndParent, int nWidth, int nHeight, PVOID /*lpCreateParams*/);

	virtual void OnNewFont(HFONT hFont);

public:

	ZLogFrame(ZDocument* pDocument);
};

HWND CreateLogView(ZDocument* pDocument, DWORD dwProcessId);
