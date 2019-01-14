#pragma once 

#include "winZ.h"

struct WINZ_API CUILayot : LIST_ENTRY
{
	int nNumWindows;

	enum 
	{
		movAx = 0x01,
		movBx = 0x02,
		movAy = 0x10,
		movBy = 0x20
	};

	struct ENTRY : LIST_ENTRY, RECT
	{
		HWND hwnd;
		DWORD f;
	};

	ENTRY* get(HWND hwnd);

protected:

	CUILayot();

	~CUILayot();

	BOOL CreateLayout(HWND hwndParent, int xCenter = 0, int yCenter = 0);

	BOOL AddChild(HWND hwndParent, HWND hwnd, int xCenter = 0, int yCenter = 0);

	BOOL RemoveChild(HWND hwnd);

	void Resize(int cx, int cy);
	
	void Resize(WPARAM wParam, LPARAM lParam);

	void OnParentNotify(HWND hwndParent, WPARAM wParam, LPARAM lParam);

	void Modify2Childs(HWND hwnd1, HWND hwnd2, int d);
};