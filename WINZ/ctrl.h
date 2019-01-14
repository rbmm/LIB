#pragma once

#include "winZ.h"

class WINZ_API ZToolBar
{
	HWND _hwnd;
	HIMAGELIST _himl;
	HBRUSH _hbr;
public:

	HWND Create(HWND hwnd, HINSTANCE hInstance, int x, int y, int cx, int cy, PTBBUTTON lpButtons, int NumButtons, BOOL bNoDivider, UINT ToolbarID = AFX_IDW_TOOLBAR);
	void EnableCmd(UINT cmd, BOOL bEnable);
	void CheckCmd(UINT cmd, BOOL bCheck);
	void IndeterminateCmd(UINT cmd, BOOL bIndeterminate);
	BOOL EraseBackground(HDC hdc, HWND hwnd);
	void SetBrush(HBRUSH hbr);
	ZToolBar();
	~ZToolBar();
	HWND getHWND(){ return _hwnd; }
};

class WINZ_API ZStatusBar
{
	HWND _hwnd;
public:
	HWND Create(HWND hwnd);

	void SetParts(const int* parts, int n)
	{
		SendMessage(_hwnd, SB_SETPARTS, n, (LPARAM)parts);
	}

	ZStatusBar()
	{
		_hwnd = 0;
	}

	HWND getHWND(){ return _hwnd; }
};

class WINZ_API CIcons
{
	HICON _hicon[2];
public:
	CIcons();
	~CIcons();
	void SetIcons(HWND hwnd, HINSTANCE hInstance, LPCWSTR id);
};

class WINZ_API CMenu
{
	HMENU _hmenu;
public:
	CMenu();
	~CMenu();
	HMENU Load(HINSTANCE hInstance, LPCWSTR id);
	HMENU getMenu(){ return _hmenu; }
};

class WINZ_API ZTabBar
{
	HWND _hwnd;
public:
	HWND Create(HWND hwnd, int x, int y, int cx);
	HWND getHWND(){ return _hwnd; }
	int addItem(PWSTR pszText, LPARAM lParam);
	int delItem(LPARAM lParam);
	int findItem(LPARAM lParam, DWORD* pTabCount = 0);
	LPARAM getCurParam();
};

class WINZ_API ZImageList
{
	HDC _hdc;
	PVOID _pvBits;
	LONG _cx, _cy;
public:

	ZImageList();

	~ZImageList();

	BOOL CreateIList(DWORD cx, DWORD cy, DWORD n);

	BOOL SetBitmap(PVOID ImageBase, PCWSTR pri[], int level, DWORD iImage);

	BOOL Draw(HDC hdcDest, int xoriginDest, int yoriginDest, DWORD iImage);
};
