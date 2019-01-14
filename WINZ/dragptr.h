#pragma once

#include "winZ.h"

class WINZ_API ZDragPtr
{
	HIMAGELIST _himl;
	PWSTR _txt;

public:

	enum {
		WM_DROP = WM_USER + WM_DROPFILES
	};

	INT_PTR _ptr;

	ZDragPtr();

	~ZDragPtr();

	void SetText(PCWSTR txt);

	void EndDrag();

	void DragMove(HWND hwnd, int x, int y);

	BOOL BeginDrag(HWND hwnd, int x, int y, int dxHotspot, int dyHotspot, DWORD l = MAXDWORD);
};
