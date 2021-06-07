#pragma once

#include "winZ.h"

#ifndef AFX_IDW_TOOLBAR
#define AFX_IDW_TOOLBAR  0xE800
#endif

#define ICON_PNG(name) (PWSTR)((ULONG_PTR)(name) | IMAGE_ORDINAL_FLAG)

class WINZ_API ZToolBar
{
	HWND _hwnd;
	HIMAGELIST _himl;
	HBRUSH _hbr;
public:

	HWND Create(HWND hwnd, HINSTANCE hInstance, int x, int y, int cx, int cy, LPCTBBUTTON lpcButtons, ULONG NumButtons, BOOL bNoDivider, UINT ToolbarID = AFX_IDW_TOOLBAR);
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
	HWND _hwnd = 0;
public:
	HWND Create(HWND hwnd);

	void SetParts(const int* parts, int n)
	{
		SendMessage(_hwnd, SB_SETPARTS, n, (LPARAM)parts);
	}

	HWND getHWND(){ return _hwnd; }
};

class WINZ_API CIcons
{
	HICON _hicon[2]{};
public:
	~CIcons();
	void SetIcons(HWND hwnd, HINSTANCE hInstance, PCWSTR id);
	void SetIcons(HWND hwnd);
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
	HDC _hdc = 0;
	LONG _cx, _cy;
public:

	ZImageList(LONG cx, LONG cy) : _cx(cx), _cy(cy)
	{
	}

	~ZImageList()
	{
		if (_hdc) DeleteDC(_hdc);
	}

	HRESULT LoadFromPNG(_In_ ULONG n, _In_ PCWSTR pszName, _In_ PVOID hmod = &__ImageBase, _In_ PCWSTR pszType = RT_RCDATA);

	BOOL CreateIList(DWORD cx, DWORD cy, DWORD n);

	BOOL SetBitmap(PVOID ImageBase, PCWSTR pri[], int level, DWORD iImage);

	BOOL Draw(HDC hdcDest, int xoriginDest, int yoriginDest, DWORD iImage);
};
