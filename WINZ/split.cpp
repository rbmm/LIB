#include "StdAfx.h"

_NT_BEGIN

#include "split.h"

void GetChildPosV(_In_ PRECT rc, _In_ ULONG dx, _In_ BOOL lt, _Out_ PRECT rcChild)
{
	rcChild->top = 0;
	rcChild->bottom = rc->bottom;
	if (lt)
	{
		rcChild->left = 0;
		rcChild->right = dx;
	}
	else
	{
		rcChild->left = dx;
		rcChild->right = rc->right;
	}
}

void GetChildPosH(_In_ PRECT rc, _In_ ULONG dy, _In_ BOOL lt, _Out_ PRECT rcChild)
{
	rcChild->left = 0;
	rcChild->right = rc->right;
	if (lt)
	{
		rcChild->top = 0;
		rcChild->bottom = dy;
	}
	else
	{
		rcChild->top = dy;
		rcChild->bottom = rc->bottom;
	}
}

void GetChildsPosV(_In_ HWND hwnd, _In_ ULONG xy, _Out_ PRECT prc1, _Out_ PRECT prc2)
{
	RECT RC;
	GetClientRect(hwnd, &RC);
	GetChildPosV(&RC, xy,     TRUE,  prc1);
	GetChildPosV(&RC, xy + GetSystemMetrics(SM_CXDLGFRAME), FALSE, prc2);
}

void GetChildsPosH(_In_ HWND hwnd, _In_ ULONG xy, _Out_ PRECT prc1, _Out_ PRECT prc2)
{
	RECT RC;
	GetClientRect(hwnd, &RC);
	GetChildPosH(&RC, xy,     TRUE,  prc1);
	GetChildPosH(&RC, xy + GetSystemMetrics(SM_CYDLGFRAME), FALSE, prc2);
}

BOOL ZSplitWnd::OnCreate(HWND hwnd)
{
	RECT rc1, rc2;
	(_bVert ? GetChildsPosV : GetChildsPosH)(hwnd, _xy, &rc1, &rc2);
	_hwndChild[0] = CreateChild(TRUE, hwnd, rc1.left, rc1.top, rc1.right - rc1.left, rc1.bottom - rc1.top);
	_hwndChild[1] = CreateChild(FALSE, hwnd, rc2.left, rc2.top, rc2.right - rc2.left, rc2.bottom - rc2.top);

	return TRUE;
}

void ZSplitWnd::MoveChilds(HWND hwnd, ULONG xy)
{
	if (HDWP hWinPosInfo = BeginDeferWindowPos(2))
	{
		RECT rc1, rc2;
		(_bVert ? GetChildsPosV : GetChildsPosH)(hwnd, xy, &rc1, &rc2);
		DeferWindowPos(hWinPosInfo, _hwndChild[0], 0, rc1.left, rc1.top, rc1.right - rc1.left, rc1.bottom - rc1.top, SWP_NOZORDER );
		DeferWindowPos(hWinPosInfo, _hwndChild[1], 0, rc2.left, rc2.top, rc2.right - rc2.left, rc2.bottom - rc2.top, SWP_NOZORDER );
		EndDeferWindowPos(hWinPosInfo);
	}
}

void ZSplitWnd::OnPaint(HWND hwnd)
{
	PAINTSTRUCT ps;
	if (BeginPaint(hwnd, &ps))
	{
		FillRect(ps.hdc, &ps.rcPaint, _hbr);
		EndPaint(hwnd, &ps);
	}
}

void ZSplitWnd::OnEnterSizeMove(HWND hwnd)
{
	RECT rc;
	GetWindowRect(hwnd, &rc);
	_rc = rc;
	_xyNew = _xy;
	ULONG xy;
	if (_bVert)
	{
		xy = GetSystemMetrics( SM_CXMINTRACK );
		rc.left += xy;
		rc.right -= xy;
	}
	else
	{
		xy = GetSystemMetrics( SM_CYMINTRACK );
		rc.top += xy;
		rc.bottom -= xy;
	}
	ClipCursor(&rc);
}

void ZSplitWnd::OnSizing(HWND hwnd)
{
	POINT pt;
	if (GetCursorPos(&pt) && ScreenToClient(hwnd, &pt))
	{
		ULONG xy = _bVert ? pt.x : pt.y;

		if (xy != _xyNew)
		{
			_xyNew = xy;
			MoveChilds(hwnd, xy);
		}
	}
}

LRESULT ZSplitWnd::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
		if (!OnCreate(hwnd))
		{
			return -1;
		}
		break;

	case WM_ERASEBKGND:
		return TRUE;

	case WM_PAINT:
		OnPaint(hwnd);
		return 0;

	case WM_ENTERSIZEMOVE:
		OnEnterSizeMove(hwnd);
		return 0;

	case WM_EXITSIZEMOVE:
		_xy = _xyNew, _bSplit = FALSE;
		ClipCursor(0);
		return 0;

	case WM_NCHITTEST:
		wParam = DefWinProc(hwnd, uMsg, wParam, lParam);
		return wParam == HTCLIENT ? (_bSplit = TRUE, _bVert ? HTRIGHT : HTBOTTOM) : (_bSplit = FALSE, wParam);

	case WM_SIZING:
		if (_bSplit)
		{
			*(PRECT)lParam = _rc;
			OnSizing(hwnd);
			return TRUE;
		}
		break;

	case WM_WINDOWPOSCHANGED:
		if (!(reinterpret_cast<WINDOWPOS*>(lParam)->flags & SWP_NOSIZE))
		{
			MoveChilds(hwnd, _xyNew);
		}
		return 0;
	}

	return __super::WindowProc(hwnd, uMsg, wParam, lParam);
}

_NT_END