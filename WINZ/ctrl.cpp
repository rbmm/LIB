#include "StdAfx.h"

_NT_BEGIN

#include "../inc/idcres.h"
#include "ctrl.h"
#include "wic.h"

void ZToolBar::SetBrush(HBRUSH hbr)
{
	_hbr = hbr;
}

BOOL ZToolBar::EraseBackground(HDC hdc, HWND hwnd)
{
	if (_hwnd)
	{
		RECT rc;
		GetClientRect(_hwnd, &rc);
		MapWindowRect(_hwnd, hwnd, &rc);
		FillRect(hdc, &rc, _hbr);
	}

	return TRUE;
}

ZToolBar::ZToolBar()
{
	_himl = 0;
	_hwnd = 0;
	_hbr = (HBRUSH)(1 + COLOR_MENUBAR);
}

ZToolBar::~ZToolBar()
{
	if (_himl) ImageList_Destroy(_himl);
}

void ZToolBar::EnableCmd(UINT cmd, BOOL bEnable)
{
	SendMessage(_hwnd, TB_ENABLEBUTTON, cmd, bEnable);  
}

void ZToolBar::CheckCmd(UINT cmd, BOOL bCheck)
{
	SendMessage(_hwnd, TB_CHECKBUTTON, cmd, bCheck);  
}

void ZToolBar::IndeterminateCmd(UINT cmd, BOOL bIndeterminate)
{
	SendMessage(_hwnd, TB_INDETERMINATE, cmd, bIndeterminate);  
}

HWND ZToolBar::Create(HWND hwnd, HINSTANCE hInstance, int x, int y, int cx, int cy, LPCTBBUTTON lpcButtons, ULONG NumButtons, BOOL bNoDivider, UINT ToolbarID)
{
	ULONG k = NumButtons, NumIcons = sizeof(TBBUTTON)*NumButtons;
	LPTBBUTTON lpButtons = (LPTBBUTTON)alloca(NumIcons);
	memcpy(lpButtons, lpcButtons, NumIcons);
	NumIcons = 0;

	int i;
	do 
	{
		if (lpButtons[--k].fsStyle != BTNS_SEP && lpButtons[k].iBitmap != I_IMAGENONE)
		{
			NumIcons++;
		}
	} while (k);

	HIMAGELIST himl = ImageList_Create(cx, cy, ILC_COLOR32, NumIcons, 0);
	
	if (!himl) return 0;

	_himl = himl;
	LIC c {0, cx, cy };

	do 
	{
		if (lpButtons[k].fsStyle != BTNS_SEP)
		{
			i = -1;
		
			switch (lpButtons[k].iBitmap)
			{
			default:
				return 0;
			case I_IMAGENONE:
				continue;
			case IMAGE_BITMAP:
				if (HBITMAP hbmp = (HBITMAP)LoadImageW(hInstance, MAKEINTRESOURCE(lpButtons[k].idCommand), IMAGE_BITMAP, cx, cy, LR_CREATEDIBSECTION))
				{
					i = ImageList_Add(himl, hbmp, 0);
					DeleteObject(hbmp);
				}
				break;
			case IMAGE_ICON:
				if (HICON hicon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCE(lpButtons[k].idCommand), IMAGE_ICON, cx, cy, LR_CREATEDIBSECTION))
				{
					i = ImageList_AddIcon(himl, hicon);
					DestroyIcon(hicon);
				}
				break;
			case IMAGE_ENHMETAFILE:
				c._pvBits = 0;
				if (0 <= c.CreateBMPFromPNG(MAKEINTRESOURCE(lpButtons[k].idCommand), hInstance))
				{
					i = ImageList_Add(himl, c._hbmp, 0);
					DeleteObject(c._hbmp);
				}
				break;
			}

			if (i < 0) return 0;

			lpButtons[k].iBitmap = i;
		}

	} while (++k < NumButtons);


#define WS_TB WS_VISIBLE|WS_CHILD|CCS_NOPARENTALIGN|CCS_NORESIZE|TBSTYLE_FLAT|TBSTYLE_LIST|TBSTYLE_TOOLTIPS|TBSTYLE_TRANSPARENT 

	HWND hwndTB = CreateWindowExW(0, TOOLBARCLASSNAME, 0, 
		bNoDivider ? WS_TB|CCS_NODIVIDER : WS_TB, 0, 0, 0, 0, hwnd, (HMENU)ToolbarID, 0, 0);

	if (!hwndTB) return 0;

	_hwnd = hwndTB;

	SendMessage(hwndTB, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0); 
	SendMessage(hwndTB, TB_SETINDENT, cx >> 2, 0); 
	SendMessage(hwndTB, TB_SETMAXTEXTROWS, 1, 0);
	SendMessage(hwndTB, TB_SETIMAGELIST, 0, (LPARAM)himl);
	SendMessage(hwndTB, TB_ADDBUTTONS, NumButtons, (LPARAM)lpButtons);
	SendMessage(hwndTB, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DOUBLEBUFFER|TBSTYLE_EX_DRAWDDARROWS|TBSTYLE_EX_MIXEDBUTTONS );

	RECT rc;
	SIZE size;
	GetClientRect(hwnd, &rc);
	SendMessage(hwndTB, TB_GETMAXSIZE, 0, (LPARAM)&size);
	MoveWindow(hwndTB, x, y, rc.right - x, size.cy+3, TRUE);

	return hwndTB;
}

HWND ZStatusBar::Create(HWND hwnd)
{
	return _hwnd = CreateWindowExW(0, STATUSCLASSNAME, 0, 
		CCS_BOTTOM|SBARS_SIZEGRIP|WS_CHILD|WS_VISIBLE, 
		0, 0, 0, 0, hwnd, (HMENU)AFX_IDW_STATUS_BAR, 0, 0);
}

HRESULT LoadIconEx(HINSTANCE hinst,
				   PCWSTR pszName,
				   int cx,
				   int cy,
				   HICON *phico)
{
	if (IMAGE_SNAP_BY_ORDINAL((ULONG_PTR)pszName))
	{
		LIC c { 0, cx, cy };
		HRESULT hr = c.LoadIconWithPNG((PCWSTR)IMAGE_ORDINAL((ULONG_PTR)pszName), hinst);
		*phico = c._hi;
		return hr;
	}

	return LoadIconWithScaleDown(hinst, pszName, cx, cy, phico);
}

void CIcons::SetIcons(HWND hwnd, HINSTANCE hInstance, PCWSTR id)
{
	LoadIconEx(hInstance, id, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), &_hicon[0]);
	LoadIconEx(hInstance, id, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), &_hicon[1]);

	SetIcons(hwnd);
}

void CIcons::SetIcons(HWND hwnd)
{
	SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)_hicon[0]);
	SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)_hicon[1]);
}

CIcons::~CIcons()
{
	if (_hicon[1]) DestroyIcon(_hicon[1]);
	if (_hicon[0]) DestroyIcon(_hicon[0]);
}

HWND ZTabBar::Create(HWND hwnd, int x, int y, int cx)
{
	if (_hwnd = hwnd = CreateWindowExW(0, WC_TABCONTROL, L"", 
		WS_CHILD|WS_CLIPSIBLINGS|WS_VISIBLE|TCS_FOCUSNEVER|TCS_HOTTRACK|TCS_TOOLTIPS ,//|TCS_FLATBUTTONS|TCS_BUTTONS, 
		x, y, cx, 0, hwnd, (HMENU)AFX_IDW_REBAR, 0, 0))
	{
		SendMessage(hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);
	}
	return hwnd;
}

LPARAM ZTabBar::getCurParam()
{
	int i = TabCtrl_GetCurSel(_hwnd);
	if (0 <= i)
	{
		TCITEM item = { TCIF_PARAM };
		if (TabCtrl_GetItem(_hwnd, i, &item))
		{
			return item.lParam;
		}
	}
	return 0;
}

int ZTabBar::addItem(PWSTR pszText, LPARAM lParam)
{
	TCITEM item = { TCIF_PARAM|TCIF_TEXT, 0, 0, pszText, 0, 0, lParam };

	if (0 <= TabCtrl_InsertItem(_hwnd, MAXLONG, &item) && 1 == TabCtrl_GetItemCount(_hwnd))
	{
		RECT rc = {};
		TabCtrl_AdjustRect(_hwnd, FALSE, &rc);
		return rc.top;
	}
	return 0;
}

int ZTabBar::delItem(LPARAM lParam)
{
	HWND hwnd = _hwnd;
	DWORD TabCount;
	int i = findItem(lParam, &TabCount);
	RECT rc;
	if (0 <= i && TabCtrl_DeleteItem(hwnd, i) && TabCount == 1 && GetWindowRect(hwnd, &rc))
	{
		return rc.bottom - rc.top;
	}
	return 0;
}

int ZTabBar::findItem(LPARAM lParam, DWORD* pTabCount)
{
	HWND hwnd = _hwnd;
	int n = TabCtrl_GetItemCount(hwnd);
	if (pTabCount) *pTabCount = n;

	TCITEM item = { TCIF_PARAM };

	while (n--)
	{
		if (TabCtrl_GetItem(hwnd, n, &item) && item.lParam == lParam)
		{
			break;
		}
	}

	return n;
}

CMenu::CMenu()
{
	_hmenu = 0;
}

CMenu::~CMenu()
{
	if (_hmenu) DestroyMenu(_hmenu);
}

HMENU CMenu::Load(HINSTANCE hInstance, LPCWSTR id)
{
	return _hmenu = LoadMenu(hInstance, id);
}

//////////////////////////////////////////////////////////////////////////
// ZImageList

BOOL ZImageList::CreateIList(DWORD cx, DWORD cy, DWORD n)
{
	if (_hdc = CreateCompatibleDC(0))
	{
		BITMAPINFO bi = { {sizeof(bi.bmiHeader), cx, cy * n, 1, 32 } };

		if (HBITMAP hbmp = CreateDIBSection(0, &bi, DIB_RGB_COLORS, 0, 0, 0))
		{
			SelectObject(_hdc, hbmp);
			DeleteObject(hbmp);
			_cx = cx, _cy = cy;
			return TRUE;
		}
	}

	return FALSE;
}

BOOL ZImageList::SetBitmap(PVOID ImageBase, PCWSTR pri[], int level, DWORD i)
{
	BITMAP bi;
	if (GetObject(GetCurrentObject(_hdc, OBJ_BITMAP), sizeof(bi), &bi) != sizeof(bi) || !bi.bmBits)
	{
		return FALSE;
	}

	PIMAGE_RESOURCE_DATA_ENTRY pirde;
	PBITMAPINFOHEADER pbih;
	DWORD cb;
	if (
		0 <= LdrFindResource_U(ImageBase, pri, level, &pirde) && 
		0 <= LdrAccessResource(ImageBase, pirde, (void**)&pbih, &cb) &&
		cb > sizeof(BITMAPINFOHEADER) &&
		pbih->biSize >= sizeof(BITMAPINFOHEADER) &&
		pbih->biWidth == _cx &&
		!(pbih->biHeight % _cy) &&
		pbih->biSize + 4 * pbih->biWidth * pbih->biHeight == cb
		)
	{
		memcpy(RtlOffsetToPointer(bi.bmBits, i * _cx * _cy * 4), RtlOffsetToPointer(pbih, pbih->biSize), cb - pbih->biSize);
		return TRUE;
	}

	return FALSE;
}

HRESULT ZImageList::LoadFromPNG(_In_ ULONG n, _In_ PCWSTR pszName, _In_ PVOID hmod /*= &__ImageBase*/, _In_ PCWSTR pszType /*= RT_RCDATA*/)
{
	LIC l { 0, _cx, n*_cy };

	HRESULT hr = l.CreateBMPFromPNG(pszName, hmod, pszType);

	if (0 <= hr)
	{
		if (_hdc = CreateCompatibleDC(0))
		{
			SelectObject(_hdc, l._hbmp);
		}
		else
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
		}

		DeleteObject(l._hbmp);
	}

	return hr;
}

BOOL ZImageList::Draw(HDC hdcDest, int xoriginDest, int yoriginDest, DWORD iImage)
{
	static BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

	return GdiAlphaBlend(hdcDest, xoriginDest, yoriginDest, _cx, _cy, _hdc, 0, _cy * iImage, _cx, _cy, bf);
}

_NT_END
