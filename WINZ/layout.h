#pragma once 

#include "winZ.h"

class WINZ_API CUILayot : LIST_ENTRY
{
	int nNumWindows;

	struct ENTRY : LIST_ENTRY, RECT
	{
		HWND hwnd;
		DWORD f;
	};

	ENTRY* get(HWND hwnd);

	void remove(ENTRY* entry);

protected:

	enum 
	{
		movAx = 0x01,
		movBx = 0x02,
		movAy = 0x10,
		movBy = 0x20
	};

	CUILayot();

	~CUILayot();

	BOOL CreateLayout(HWND hwndParent, int xCenter = 0, int yCenter = 0);

	BOOL AddChild(HWND hwndParent, HWND hwnd, int xCenter = 0, int yCenter = 0);

	BOOL RemoveChild(HWND hwnd);

	BOOL ModifyChild(HWND hwndParent, UINT nCtrlID, ULONG f);

	void Resize(int cx, int cy);

	void Resize(WPARAM wParam, LPARAM lParam);

	void OnParentNotify(HWND hwndParent, WPARAM wParam, LPARAM lParam, int xCenter = 0, int yCenter = 0);

	void Modify2Childs(HWND hwnd1, HWND hwnd2, int d);
};