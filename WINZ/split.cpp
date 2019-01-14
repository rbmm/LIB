#include "StdAfx.h"

_NT_BEGIN

#include "split.h"

//////////////////////////////////////////////////////////////////////////
// ZScplitWnd

ZSplitWnd::ZSplitWnd(int t)
{
	_hdc = 0, _t = t;
}

ZSplitWnd::~ZSplitWnd()
{
}

BOOL ZSplitWnd::OnCreate(HWND hwnd)
{
	RECT rc;
	GetClientRect(hwnd, &rc);
	cx = rc.right, cy = rc.bottom;
	return CreateChilds(hwnd);
}

BOOL ZSplitWnd::MouseOnSplit(HWND hwnd, LPARAM lParam)
{
	POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
	ScreenToClient(hwnd, &pt);
	return PointInSplit(pt);
}

LRESULT ZSplitWnd::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_SIZE:
		switch (wParam)
		{
		case SIZE_RESTORED:
		case SIZE_MINIMIZED:
		case SIZE_MAXIMIZED:
			cx = GET_X_LPARAM(lParam), cy = GET_Y_LPARAM(lParam);
			MoveChilds();
			break;
		}
		break;
	case WM_NCLBUTTONDOWN:
		if (!_hdc && MouseOnSplit(hwnd, lParam) && (_hdc = GetDC(hwnd)))
		{
			RECT rc = { 0, 0, cx, cy};
			MapWindowRect(hwnd, HWND_DESKTOP, &rc);
			SetCapture(hwnd);
			ClipCursor(&rc);
			POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			ScreenToClient(hwnd, &pt);
			_tPrev = GetT(pt);
			DrawMovingSplit();
		}
		break;
	case WM_MOUSEMOVE:
		if (_hdc)
		{
			int t = GetT(lParam);
			if (_tPrev != t)
			{
				DrawMovingSplit();
				_tPrev = t;
				DrawMovingSplit();
			}
		}
		break;
	case WM_LBUTTONUP:
		if (_hdc)
		{
			DrawMovingSplit();
			ClipCursor(0); 
			ReleaseCapture();
			ReleaseDC(hwnd, _hdc);
			_hdc = 0;
			_t = _tPrev;
			MoveChilds();
			InvalidateRect(hwnd, 0, FALSE);
		}
		break;
	case WM_NCHITTEST:
		if (MouseOnSplit(hwnd, lParam)) return GetHitCode();
		break;
	case WM_DESTROY:
		if (_hdc) ReleaseDC(hwnd, _hdc);
		break;
	case WM_CREATE:
		if (!OnCreate(hwnd)) return -1;
		break;
	case WM_ERASEBKGND:
		return TRUE;
	case WM_PAINT:
		PAINTSTRUCT ps;
		if (BeginPaint(hwnd, &ps))
		{
			DrawSplit(ps.hdc);
			EndPaint(hwnd, &ps);
		}
		break;
	}
	return ZWnd::WindowProc(hwnd, uMsg, wParam, lParam);
}

HRESULT ZSplitWnd::QI(REFIID riid, void **ppvObject)
{
	if (riid == __uuidof(ZSplitWnd))
	{
		*ppvObject = static_cast<ZObject*>(this);
		AddRef();
		return S_OK;
	}

	return ZWnd::QI(riid, ppvObject);
}

//////////////////////////////////////////////////////////////////////////
// ZSplitWndV

int ZSplitWndV::GetHitCode()
{
	return HTRIGHT;
}

BOOL ZSplitWndV::CreateChilds(HWND hwnd)
{
	return (_hwndCld[0] = CreateChild(TRUE, hwnd, 0, 0, _t - 2, cy)) && 
		(_hwndCld[1] = CreateChild(FALSE, hwnd, _t - 2, 0, cx - _t - 2, cy));
}

void ZSplitWndV::MoveChilds()
{
	if (_t > cx - 16) _t = cx - 16;
	if (_t < 0) _t = 0;

	MoveWindow(_hwndCld[0], 0, 0, _t - 2, cy, TRUE);
	MoveWindow(_hwndCld[1], _t + 2, 0, cx - _t - 2, cy, TRUE);
}

BOOL ZSplitWndV::PointInSplit(POINT pt)
{
	return (DWORD)pt.y < (DWORD)cy && (DWORD)(pt.x - _t + 2) < 5;
}

int ZSplitWndV::GetT(POINT pt)
{
	return pt.x;
}

int ZSplitWndV::GetT(LPARAM lParam)
{
	return GET_X_LPARAM(lParam);
}

void ZSplitWndV::DrawSplit(HDC hdc)
{
	RECT rc = { _t - 2, -1, _t + 2, cy + 2 };
	DrawEdge(hdc, &rc, EDGE_RAISED, BF_RECT);
}

void ZSplitWndV::DrawMovingSplit()
{
	PatBlt(_hdc, _tPrev - 1, 0, 3, cy, DSTINVERT);
}

HRESULT ZSplitWndV::QI(REFIID riid, void **ppvObject)
{
	if (riid == __uuidof(ZSplitWndV))
	{
		*ppvObject = static_cast<ZObject*>(this);
		AddRef();
		return S_OK;
	}

	return ZSplitWnd::QI(riid, ppvObject);
}

//////////////////////////////////////////////////////////////////////////
// ZSplitWndH

int ZSplitWndH::GetHitCode()
{
	return HTBOTTOM;
}

BOOL ZSplitWndH::CreateChilds(HWND hwnd)
{
	return (_hwndCld[0] = CreateChild(TRUE, hwnd, 0, 0, _t - 2, cy)) && 
		(_hwndCld[1] = CreateChild(FALSE, hwnd, _t - 2, 0, cx - _t - 2, cy));
}

void ZSplitWndH::MoveChilds()
{
	if (_t > cy - 16) _t = cy - 16;
	if (_t < 0) _t = 0;

	MoveWindow(_hwndCld[0], 0, 0, cx, _t - 2, TRUE);
	MoveWindow(_hwndCld[1], 0, _t + 2, cx, cy - _t - 2, TRUE);
}

BOOL ZSplitWndH::PointInSplit(POINT pt)
{
	return (DWORD)pt.x < (DWORD)cx && (DWORD)(pt.y - _t + 2) < 5;
}

int ZSplitWndH::GetT(POINT pt)
{
	return pt.y;
}

int ZSplitWndH::GetT(LPARAM lParam)
{
	return GET_Y_LPARAM(lParam);
}

void ZSplitWndH::DrawSplit(HDC hdc)
{
	RECT rc = { -1, _t - 2, cx + 2, _t + 2};
	DrawEdge(hdc, &rc, EDGE_RAISED, BF_RECT);
}

void ZSplitWndH::DrawMovingSplit()
{
	PatBlt(_hdc, 0, _tPrev - 1, cx, 3, DSTINVERT);
}

HRESULT ZSplitWndH::QI(REFIID riid, void **ppvObject)
{
	if (riid == __uuidof(ZSplitWndH))
	{
		*ppvObject = static_cast<ZObject*>(this);
		AddRef();
		return S_OK;
	}

	return ZSplitWnd::QI(riid, ppvObject);
}

_NT_END