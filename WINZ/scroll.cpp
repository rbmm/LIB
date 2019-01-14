#include "stdafx.h"

_NT_BEGIN

#include "scroll.h"

ZScrollWnd::ZScrollWnd()
{
	_nPos.y = 0, _nPos.x = 0, _TrackEnabled = TRUE;
}

int ZScrollWnd::GetIndent()
{
	return 0;
}

void ZScrollWnd::GetPos(POINT& nPos)
{
	nPos = _nPos;
}

void ZScrollWnd::SetPos(POINT& nPos)
{
	_nPos = nPos;
}

LRESULT ZScrollWnd::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;

	switch (message)
	{
	case WM_VSCROLL:
		OnScroll(hwnd, SB_VERT, LOWORD(wParam));
		return 0;
	case WM_HSCROLL:
		OnScroll(hwnd, SB_HORZ, LOWORD(wParam));
		return 0;
	case WM_SIZE:
		switch(wParam)
		{
		case SIZE_MAXIMIZED:
		case SIZE_RESTORED:
			OnSize(hwnd, LOWORD(lParam), HIWORD(lParam));
		}
		break;
	case WM_PAINT:
		if (BeginPaint(hwnd, &ps))
		{
			SIZE v;
			GetUnitSize(v);

			int vy = ps.rcPaint.top / v.cy;
			int ny = (ps.rcPaint.bottom + v.cy - 1) / v.cy;
			int y = vy * v.cy;

			ps.rcPaint.top = y;
			ps.rcPaint.bottom = ny * v.cy;
			ny -= vy;

			LONG ix = GetIndent();

			if (ix && ps.rcPaint.left < ix)
			{
				LONG right = ps.rcPaint.right;
				
				if (right > ix)
				{
					ps.rcPaint.right = ix;
				}

				OnDrawIndent(ps.hdc, y, _nPos.y + vy, ny, &ps.rcPaint);
				
				ps.rcPaint.right = right, ps.rcPaint.left = ix;
			}

			if (ps.rcPaint.left < ps.rcPaint.right)
			{
				int vx = (ps.rcPaint.left - ix) / v.cx;
				int x = ix + vx * v.cx;
				int nx = (ps.rcPaint.right - ix + v.cx - 1) / v.cx;

				ps.rcPaint.left = x;
				ps.rcPaint.right = ix + nx * v.cx;
				
				OnDraw(ps.hdc, x, y, _nPos.x + vx, _nPos.y + vy, nx - vx, ny, &ps.rcPaint);
			}

			EndPaint(hwnd, &ps);
		}
		return 0;
	case WM_ERASEBKGND:
		return TRUE;
	case WM_LBUTTONDOWN:
		SetFocus(hwnd);
		break;
	case WM_MOUSEWHEEL:
		OnScroll(hwnd, SB_VERT, (short)HIWORD(wParam) < 0 ? SB_LINEDOWN : SB_LINEUP);
		break;
	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_DOWN:
			OnScroll(hwnd, SB_VERT, SB_LINEDOWN);
			break;
		case VK_UP:
			OnScroll(hwnd, SB_VERT, SB_LINEUP);
			break;
		case VK_PRIOR:
			OnScroll(hwnd, SB_VERT, SB_PAGEUP);
			break;
		case VK_NEXT:
			OnScroll(hwnd, SB_VERT, SB_PAGEDOWN);
			break;
		case VK_LEFT:
			OnScroll(hwnd, SB_HORZ, SB_LINEUP);
			break;
		case VK_RIGHT:
			OnScroll(hwnd, SB_HORZ, SB_LINEDOWN);
			break;
		}
		break;
	}

	return ZWnd::WindowProc(hwnd, message, wParam, lParam);
}

int ZScrollWnd::ScrollLines(int /*nBar*/, int nLines, int& nPos)
{
	nPos += nLines;
	return nLines;
}

int ZScrollWnd::NewPos(int /*nBar*/, int nPos, BOOL /*bTrack*/)
{
	return nPos;
}

void ZScrollWnd::OnScroll(HWND hwnd, int nBar, int code)
{
	SCROLLINFO si = { sizeof(si), SIF_ALL };

	if (GetScrollInfo(hwnd, nBar, &si))
	{
		int nPage = si.nPage, nPos = si.nPos;

		switch(nPage)
		{
		case 0:
			nPage = 1;
		case 1:
			break;
		default:
			nPage--;
		}

		int nLines = 0, maxLines = 1 + si.nMax - si.nPage - si.nPos;

		switch (code)
		{
		case SB_PAGEDOWN:
			nLines = ScrollLines(nBar, min(nPage, maxLines), si.nPos);
			break;
		case SB_PAGEUP:
			nLines = ScrollLines(nBar, -min(nPage, si.nPos), si.nPos);
			break;
		case SB_LINEDOWN:
			nLines = ScrollLines(nBar, min(1, maxLines), si.nPos);
			break;
		case SB_LINEUP:
			nLines = ScrollLines(nBar, -min(1, si.nPos), si.nPos);
			break;
		case SB_TOP:
			si.nPos = NewPos(nBar, 0, FALSE);
			break;
		case SB_BOTTOM:
			si.nPos = NewPos(nBar, 1 + si.nMax - si.nPage, FALSE);
			break;
		case SB_THUMBTRACK:
			if (_TrackEnabled)
			{
		case SB_THUMBPOSITION:
				si.nPos = NewPos(nBar, si.nTrackPos, SB_THUMBTRACK == code);
			}
			break;
		case SB_ENDSCROLL:
			break;
		default: return;
		}

		if (nLines || si.nPos != nPos)
		{
			//DbgPrint("| %u -> %u\n", nPos, si.nPos);
			si.fMask = SIF_POS;

			nPos = SetScrollInfo(hwnd, nBar, &si, TRUE);

			int dx, dy;

			SIZE u;
			GetUnitSize(u);

			if (nBar == SB_VERT)
			{
				dy = nLines * u.cy, dx = 0, _nPos.y = nPos;
			}
			else
			{
				dx = nLines * u.cx, dy = 0, _nPos.x = nPos;
			}

			RECT rc;
			GetClientRect(hwnd, &rc);

			if (nBar == SB_HORZ)
			{
				rc.left = GetIndent();
			}

			if (nLines)
			{
				ScrollWindowEx(hwnd, -dx, -dy, &rc, &rc, 0, 0, SW_ERASE|SW_INVALIDATE);
			}
			else
			{
				InvalidateRect(hwnd, 0, TRUE);
			}
		}
	}
}

void ZScrollWnd::OnSize(HWND hwnd, int cx, int cy)
{
	Resize(hwnd, cx, cy, FALSE);
}

void ZScrollWnd::GoTo(HWND hwnd, int x, int y)
{
	_nPos.x = x, _nPos.y = y;
	RECT rc;
	GetClientRect(hwnd, &rc);
	Resize(hwnd, rc.right, rc.bottom, TRUE);
}

void ZScrollWnd::Resize(HWND hwnd, int cx, int cy, BOOL bNeedRepaint)
{
	if (0 > (cx -= GetIndent())) cx = 0;

	SIZE v, N;

	GetUnitSize(v);
	GetVirtualSize(N);

	SCROLLINFO si;
	si.cbSize = sizeof(SCROLLINFO);
	si.fMask = SIF_PAGE|SIF_RANGE|SIF_POS;
	si.nMin = 0;

	si.nPos = _nPos.y;
	si.nMax = N.cy - 1;
	si.nPage = (cy + v.cy - 1) / v.cy;

	if (si.nPos + (int)si.nPage > 1 + si.nMax)
	{		
		if ((si.nPos = 1 + si.nMax - si.nPage) < 0)
		{
			si.nPos = 0;
		}

		if (_nPos.y != si.nPos)
		{
			_nPos.y = si.nPos;
			bNeedRepaint = TRUE;
		}
	}

	SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

	si.nPos = _nPos.x;
	si.nMax = N.cx - 1;
	si.nPage = (cx + v.cx - 1) / v.cx;

	if (si.nPos + (int)si.nPage > 1 + si.nMax)
	{		
		if ((si.nPos = 1 + si.nMax - si.nPage) < 0)
		{
			si.nPos = 0;
		}

		if (_nPos.x != si.nPos)
		{
			_nPos.x = si.nPos;
			bNeedRepaint = TRUE;
		}
	}

	SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);

	if (bNeedRepaint)
	{
		InvalidateRect(hwnd, 0, TRUE);
	}
}

HRESULT ZScrollWnd::QI(REFIID riid, void **ppvObject)
{
	if (riid == __uuidof(ZScrollWnd))
	{
		*ppvObject = static_cast<ZObject*>(this);
		AddRef();
		return S_OK;
	}

	return ZWnd::QI(riid, ppvObject);
}

_NT_END