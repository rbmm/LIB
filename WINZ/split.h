#pragma once

#include "window.h"

class WINZ_API Z_INTERFACE("A1C070DA-EEAD-4304-9528-C08DD3488A44") ZSplitWnd : public ZWnd
{
	HWND _hwndChild[2];
	HBRUSH _hbr;
	RECT _rc;
	ULONG _xy, _xyNew;
	BOOLEAN _bVert, _bSplit = FALSE;

	void MoveChilds(HWND hwnd, ULONG xy);
	BOOL OnCreate(HWND hwnd);
	void OnPaint(HWND hwnd);
	void OnEnterSizeMove(HWND hwnd);
	void OnSizing(HWND hwnd);

protected:
	virtual LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual HWND CreateChild(BOOL lt, HWND hwndParent, int x, int y, int nWidth, int nHeight) = 0;

public:

	ZSplitWnd(BOOLEAN bV, ULONG xy, HBRUSH hbr) : _bVert(bV), _xy(xy), _xyNew(xy), _hbr(hbr) {}
};
