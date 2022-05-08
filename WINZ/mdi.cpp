#include "stdafx.h"

_NT_BEGIN
#include "../inc/idcres.h"
#include "mdi.h"
#include "view.h"

//////////////////////////////////////////////////////////////////////////
// ZMDIFrameWnd

void ZMDIFrameWnd::OnIdle()
{
	if (_bChildChanged)
	{
		_bChildChanged = FALSE;

		BOOL bZoomed;

		HWND hwndActive = GetActive(&bZoomed);

		if (hwndActive != _hwndView)
		{
			_hwndView = hwndActive;

			if (hwndActive)
			{
				TabCtrl_SetCurSel(ZTabBar::getHWND(), ZTabBar::findItem((LPARAM)hwndActive));

				BOOL fDoc = FALSE;

				if (ZWnd* p = ZWnd::FromHWND(hwndActive))
				{
					if (ZView* pView = p->getView())
					{
						if (ZDocument* pDocument = pView->getDocument())
						{
							fDoc = TRUE;
							SetActiveDoc(pDocument);
						}
					}

					p->Release();
				}

				if (!fDoc)
				{
					SetActiveDoc(0);
				}
			}
		}
		
		BOOLEAN bClientEdge = !hwndActive || !bZoomed;

		if (bClientEdge != _bClientEdge)
		{
			_bClientEdge = bClientEdge;
			SetWindowLongW(_hwndMDI, GWL_EXSTYLE, bClientEdge ? WS_EX_CLIENTEDGE : 0);
			SetWindowPos(_hwndMDI, 0, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
		}
	}

	ZSDIFrameWnd::OnIdle();
}

LRESULT ZMDIFrameWnd::OnNotify( LPNMHDR lpnm )
{
	switch(lpnm->idFrom)
	{
	case AFX_IDW_REBAR:
		if (lpnm->code == TCN_SELCHANGE)
		{
			if (LPARAM lParam = ZTabBar::getCurParam())
			{
				SendMessage(_hwndMDI, WM_MDIACTIVATE, lParam, 0);
				return 0;
			}
		}
		break;
	}

	return ZSDIFrameWnd::OnNotify(lpnm);
}

void ZMDIFrameWnd::SetTitleText(HWND hwnd, LPCWSTR pszText)
{
	HWND hwndTB = ZTabBar::getHWND();

	if (int n = TabCtrl_GetItemCount(hwndTB))
	{
		TCITEM item = { TCIF_PARAM };

		do 
		{
			if (TabCtrl_GetItem(hwndTB, --n, &item) && item.lParam == (LPARAM)hwnd)
			{
				item.pszText = (LPWSTR)pszText;
				item.mask = TCIF_TEXT;
				TabCtrl_SetItem(hwndTB, n, &item);
				break;
			}
		} while (n);
	}
}

LRESULT ZMDIFrameWnd::DefWinProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HWND hwndTB;

	switch (uMsg)
	{
	case WM_SIZE:
		return 0;

	case WM_CLOSE:
		
		if (int n = TabCtrl_GetItemCount(hwndTB = ZTabBar::getHWND()))
		{
			TCITEM item = { TCIF_PARAM };

			do 
			{
				if (TabCtrl_GetItem(hwndTB, --n, &item))
				{
					SendMessage((HWND)item.lParam, WM_CLOSE, (WPARAM)hwndTB, -1);
				}
			} while (n);

			if (TabCtrl_GetItemCount(hwndTB))
			{
				return 0;
			}
		}
		break;
	}
	return DefFrameProc(hwnd, _hwndMDI, uMsg, wParam, lParam);
}

ZMDIFrameWnd::ZMDIFrameWnd()
{
	_hwndMDI = 0;
	_bChildChanged = FALSE;
	_bClientEdge = TRUE;
}

void ZMDIFrameWnd::OnCreateChild(HWND hwnd)
{
	WCHAR sz[64];
	if (!GetWindowText(hwnd, sz, _countof(sz)))
	{
		*sz = 0;
	}
	if (int cy = ZTabBar::addItem(sz, (LPARAM)hwnd))
	{
		if (_bClientEdge)
		{
			_bClientEdge = FALSE;
			SetWindowLongW(_hwndMDI, GWL_EXSTYLE, 0);
		}

		CUILayot::Modify2Childs(ZTabBar::getHWND(), _hwndMDI, cy);
	}
}

void ZMDIFrameWnd::OnDestroyChild(HWND hwnd)
{
	if (int cy = ZTabBar::delItem((LPARAM)hwnd))
	{
		SetView(0);
		SetActiveDoc(0);

		if (!_bClientEdge)
		{
			_bClientEdge = TRUE;
			SetWindowLongW(_hwndMDI, GWL_EXSTYLE, WS_EX_CLIENTEDGE);
		}

		CUILayot::Modify2Childs(ZTabBar::getHWND(), _hwndMDI, -cy);
	}
}

BOOL ZMDIFrameWnd::CreateClient(HWND hwnd, int x, int y, int nWidth, int nHeight)
{
	if (!ZTabBar::Create(hwnd, 0, y, nWidth)) return FALSE;

	CLIENTCREATESTRUCT ccs = { GetSubMenu(getMenu(), 1), AFX_IDM_FIRST_MDICHILD};

	if (_hwndMDI = CreateWindowEx(WS_EX_CLIENTEDGE, L"MDICLIENT", 0, 
		WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS|WS_VISIBLE, 
		x, y, nWidth, nHeight, hwnd, (HMENU)AFX_IDW_RESIZE_BAR, 0, &ccs))
	{
		return TRUE;
	}

	return FALSE;
}

HWND ZMDIFrameWnd::GetActive(PBOOL pbZoomed)
{
	return (HWND)SendMessage(_hwndMDI, WM_MDIGETACTIVE, 0, (LPARAM)pbZoomed);
}

HRESULT ZMDIFrameWnd::QI(REFIID riid, void **ppvObject)
{
	if (riid == __uuidof(ZMDIFrameWnd))
	{
		*ppvObject = static_cast<ZObject*>(this);
		AddRef();
		return S_OK;
	}

	return ZSDIFrameWnd::QI(riid, ppvObject);
}

//////////////////////////////////////////////////////////////////////////
// ZMDIChildFrame

ZMDIChildFrame::ZMDIChildFrame()
{
	_hwndView = 0;
	_pFrame = static_cast<ZMDIFrameWnd*>(ZGLOBALS::getMainFrame());
}

ZView* ZMDIChildFrame::getView()
{
	return ZView::get(_hwndView);
}

void ZMDIChildFrame::Activate()
{
	_Activate(_pFrame->_hwndMDI, getHWND());
}

void ZMDIChildFrame::_Activate(HWND hwndMDIClient, HWND hwnd)
{
	SendMessage(hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwnd, 0);
	PostThreadMessage(GetCurrentThreadId(), WM_NULL, 0, 0);
}

HWND ZMDIChildFrame::Create(PCWSTR lpWindowName, PVOID lpParam)
{
	return ZWnd::Create(WS_EX_MDICHILD|WS_EX_CLIENTEDGE, lpWindowName, 
		WS_VISIBLE|WS_OVERLAPPEDWINDOW|WS_MAXIMIZE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS|MDIS_ALLCHILDSTYLES, 
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, _pFrame->_hwndMDI, 0, lpParam);
}

LRESULT ZMDIChildFrame::DefWinProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return DefMDIChildProc(hwnd, uMsg, wParam, lParam);
}

void ZMDIChildFrame::DoResize(WPARAM wParam, LPARAM lParam)
{
	switch (wParam)
	{
	case SIZE_RESTORED:
	case SIZE_MAXIMIZED:
		if (_hwndView) MoveWindow(_hwndView, 0, 0, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), TRUE);
	}
}

BOOL ZMDIChildFrame::CanClose()
{
	return ZView::CanClose(_hwndView);
}

LRESULT ZMDIChildFrame::OnNotify( LPNMHDR /*lpnm*/ )
{
	return 0;
}

LRESULT ZMDIChildFrame::WindowProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	union {
		HWND hwndView;
		RECT rc;
	};

	switch (uMsg)
	{
	case WM_NOTIFY:
		return OnNotify((LPNMHDR)lParam);

	case WM_CLOSE:
		if (!CanClose()) return 0;
		break;

	case WM_CHILDACTIVATE:
		_pFrame->_bChildChanged = TRUE;
		break;

	case WM_CREATE:
		_pFrame->OnCreateChild(hwnd);

		if (!GetClientRect(hwnd, &rc) || !(_hwndView = CreateView(hwnd, rc.right, rc.bottom, 
			(PVOID)((LPMDICREATESTRUCT)((LPCREATESTRUCT)lParam)->lpCreateParams)->lParam
			))) return -1;

		if (_hwndView == HWND_BROADCAST)
		{
			_hwndView = 0;
		}
		_pFrame->_hwndView = 0;
		break;
	
	case WM_DESTROY:
		_pFrame->OnDestroyChild(hwnd);
		break;
	
	case SB_SETTEXT:
		_pFrame->SetStatusText((int)wParam, (LPCWSTR)lParam);
		return 0;
	
	case WM_SETTEXT:
		_pFrame->SetTitleText(hwnd, (LPCWSTR)lParam);
		return 0;
	
	case WM_ERASEBKGND:
		return TRUE;

	case WM_PAINT:
		EmptyPaint(hwnd);
		break;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	case WM_CHAR:
	case WM_SYSCHAR:
		if (hwndView = _hwndView) 
		{
			return SendMessage(hwndView, uMsg, wParam, lParam);
		}
		break;

	case WM_SETFOCUS:
		if (hwndView = _hwndView) SetFocus(hwndView);
		break;

	case WM_SIZE:
		DoResize(wParam, lParam);
		break;
	}

	return ZWnd::WindowProc(hwnd, uMsg, wParam, lParam);
}

HRESULT ZMDIChildFrame::QI(REFIID riid, void **ppvObject)
{
	if (riid == __uuidof(ZDocument))
	{
		*ppvObject = 0;
		HRESULT hr = E_NOINTERFACE;

		if (_hwndView)
		{
			if (ZWnd* pWnd = ZWnd::FromHWND(_hwndView))
			{
				hr = pWnd->QI(riid, ppvObject);

				pWnd->Release();
			}
		}

		return hr;
	}

	if (riid == __uuidof(ZMDIChildFrame))
	{
		*ppvObject = static_cast<ZObject*>(this);
		AddRef();
		return S_OK;
	}

	return ZWnd::QI(riid, ppvObject);
}

//////////////////////////////////////////////////////////////////////////
// ZMDIChildMultiFrame

HWND ZMDIChildMultiFrame::CreateView(HWND hWndParent, int nWidth, int nHeight, PVOID lpCreateParams)
{
	if (CreateClient(hWndParent, nWidth, nHeight, lpCreateParams))
	{
		CreateLayout(hWndParent);
		return _hwndView ? _hwndView : HWND_BROADCAST;
	}
	return 0;
}

void ZMDIChildMultiFrame::DoResize(WPARAM wParam, LPARAM lParam)
{
	CUILayot::Resize(wParam, lParam);
}

HRESULT ZMDIChildMultiFrame::QI(REFIID riid, void **ppvObject)
{
	if (riid == __uuidof(ZMDIChildMultiFrame))
	{
		*ppvObject = static_cast<ZObject*>(this);
		AddRef();
		return S_OK;
	}

	return ZMDIChildFrame::QI(riid, ppvObject);
}

_NT_END