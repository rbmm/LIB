#include "StdAfx.h"

_NT_BEGIN

#include "frame.h"
#include "view.h"

//////////////////////////////////////////////////////////////////////////
// ZFrameWnd

ZView* ZFrameWnd::getView()
{
	return ZView::get(_hwndView);
}

HWND ZFrameWnd::Create(
			DWORD dwExStyle,
			PCWSTR lpWindowName,
			DWORD dwStyle,
			int x,
			int y,
			int nWidth,
			int nHeight,
			HWND hWndParent,
			HMENU hMenu,
			PVOID lpParam
			)
{
	if (PCUNICODE_STRING Name = getPosName())
	{
		if (ZRegistry* p = ZGLOBALS::getRegistry())
		{
			p->LoadWinPos(Name, x, y, nWidth, nHeight, dwStyle);
		}
	}

	return ZWnd::Create(dwExStyle, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, lpParam);
}

PCUNICODE_STRING ZFrameWnd::getPosName()
{
	return 0;
}

void ZFrameWnd::DoResize(WPARAM wParam, LPARAM lParam)
{
	switch (wParam)
	{
	case SIZE_RESTORED:
	case SIZE_MAXIMIZED:
		if (_hwndView) MoveWindow(_hwndView, 0, 0, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), TRUE);
	}
}

BOOL ZFrameWnd::CanClose()
{
	return ZView::CanClose(_hwndView);
}

LRESULT ZFrameWnd::OnCreate(HWND hwnd, CREATESTRUCT* lpcs)
{
	RECT rc;
	if (!GetClientRect(hwnd, &rc) || 
		!(_hwndView = CreateView(hwnd, rc.right, rc.bottom, lpcs->lpCreateParams))) return -1;

	if (_hwndView == HWND_BROADCAST)
	{
		_hwndView = 0;
	}

	return 0;
}

LRESULT ZFrameWnd::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CLOSE:
		if (!CanClose()) return 0;
		break;

	case WM_SETFOCUS:
		if (_hwndView) SetFocus(_hwndView);
		break;

	case WM_SIZE:
		DoResize(wParam, lParam);
		break;

	case WM_DESTROY:
		if (PCUNICODE_STRING Name = getPosName())
		{
			if (ZRegistry* p = ZGLOBALS::getRegistry())
			{
				p->SaveWinPos(Name, hwnd);
			}
		}
		break;

	case WM_ERASEBKGND:
		return TRUE;

	case WM_PAINT:
		return EmptyPaint(hwnd);

	case WM_CREATE:
		return OnCreate(hwnd, (LPCREATESTRUCT)lParam);

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	case WM_CHAR:
	case WM_SYSCHAR:
	case WM_COMMAND:
		if (_hwndView)
		{
			if (ZWnd* p = ZWnd::FromHWND(_hwndView))
			{
				p->WindowProc(_hwndView, uMsg, wParam, lParam);
				p->Release();
			}
		}
		break;
	}

	return ZWnd::WindowProc(hwnd, uMsg, wParam, lParam);
}

HRESULT ZFrameWnd::QI(REFIID riid, void **ppvObject)
{
	if (riid == __uuidof(ZFrameWnd))
	{
		*ppvObject = static_cast<ZObject*>(this);
		AddRef();
		return S_OK;
	}

	return ZWnd::QI(riid, ppvObject);
}

//////////////////////////////////////////////////////////////////////////
// ZFrameMultiWnd

HWND ZFrameMultiWnd::CreateView(HWND hWndParent, int nWidth, int nHeight, PVOID lpCreateParams)
{
	if (CreateClient(hWndParent, nWidth, nHeight, lpCreateParams))
	{
		CreateLayout(hWndParent);
		return _hwndView ? _hwndView : HWND_BROADCAST;
	}
	return 0;
}

void ZFrameMultiWnd::DoResize(WPARAM wParam, LPARAM lParam)
{
	CUILayot::Resize(wParam, lParam);
}

HRESULT ZFrameMultiWnd::QI(REFIID riid, void **ppvObject)
{
	if (riid == __uuidof(ZFrameMultiWnd))
	{
		*ppvObject = static_cast<ZObject*>(this);
		AddRef();
		return S_OK;
	}

	return ZWnd::QI(riid, ppvObject);
}

//////////////////////////////////////////////////////////////////////////
// ZSDIFrameWnd

ZSDIFrameWnd::ZSDIFrameWnd()
{
	_pActiveDoc = 0;
	_pCmdId = 0;
	_nCmdId = 0;

	if (_ZGLOBALS* globals = ZGLOBALS::get())
	{
		globals->MainFrame = this;
	}
}

ZSDIFrameWnd::~ZSDIFrameWnd()
{
	if (_pActiveDoc)
	{
		_pActiveDoc->Release();
	}

	if (_pCmdId)
	{
		delete [] _pCmdId;
	}

	if (_ZGLOBALS* globals = ZGLOBALS::get())
	{
		globals->MainFrame = 0;
		globals->hwndMain = 0;
	}
}

void ZSDIFrameWnd::SetActiveDoc(ZDocument* pDoc)
{
	if (_pActiveDoc != pDoc)
	{
		if (_pActiveDoc)
		{
			_pActiveDoc->OnActivate(FALSE);
			_pActiveDoc->Release();
		}

		if (pDoc)
		{
			pDoc->AddRef();
			pDoc->OnActivate(TRUE);
		}

		_pActiveDoc = pDoc;
	}
}

DWORD ZSDIFrameWnd::getDocumentCmdId(WORD const** ppCmdId)
{
	*ppCmdId = 0;
	return 0;
}

void ZSDIFrameWnd::OnIdle()
{
	PWORD pCmdId, pcCmdId;

	if (DWORD nCmdId = _nCmdId)
	{
		pCmdId = (PWORD)alloca(nCmdId << 1);

		if (_pActiveDoc)
		{
			__movsw(pCmdId, _pcCmdId, nCmdId);
			_pActiveDoc->EnableCommands(nCmdId, pCmdId);
		}
		else
		{
			__stosw(pCmdId, 0, nCmdId);
		}

		pcCmdId = pCmdId, pCmdId = _pCmdId;

		do 
		{
			WORD cmd = *pcCmdId++, _cmd = *pCmdId;

			if (cmd != _cmd)
			{
				*pCmdId = cmd;
				EnableCmd(cmd|_cmd, cmd);
			}

		} while (pCmdId++, --nCmdId);
	}
}

LRESULT ZSDIFrameWnd::OnNotify(LPNMHDR lpnm)
{
	switch(lpnm->idFrom)
	{
	case AFX_IDW_TOOLBAR:
		switch (lpnm->code)
		{
		case TBN_DROPDOWN:
			{
				LPNMTOOLBAR lpnmtb = (LPNMTOOLBAR)lpnm;
				WORD cmd = (WORD)lpnmtb->iItem;
				if (_pActiveDoc && _nCmdId && findWORD(_nCmdId, _pCmdId, cmd))
				{
					return _pActiveDoc->OnCmdMsg(MAKEWPARAM(cmd, TBN_DROPDOWN), (LPARAM)lpnmtb);
				}
			}
			return TBDDRET_NODEFAULT;

		case TBN_GETINFOTIPW:
			LPNMTBGETINFOTIP lpttt = (LPNMTBGETINFOTIP)lpnm;
			HWND hwnd = ZToolBar::getHWND();
			TBBUTTON tb;
			if (SendMessage(hwnd, TB_GETBUTTON, 
				SendMessage(hwnd, TB_COMMANDTOINDEX, lpttt->iItem, 0), (LPARAM)&tb))
			{
				wcscpy(lpttt->pszText, (PCWSTR)tb.dwData);
			}
			break;
		}
		break;
	}

	return 0;
}

BOOL ZSDIFrameWnd::CreateClient(HWND hwnd, int nWidth, int nHeight, PVOID lpCreateParams)
{
	int top = 0; 
	RECT rc;

	if (CreateSB(hwnd) && CreateTB(hwnd))
	{
		POINT pt = {};
		if (ZToolBar::getHWND() && GetWindowRect(ZToolBar::getHWND(), &rc))
		{
			pt.y = rc.bottom;
			ScreenToClient(hwnd, &pt);
			top = pt.y;
		}

		if (ZStatusBar::getHWND() && GetWindowRect(ZStatusBar::getHWND(), &rc))
		{
			pt.y = rc.top;
			ScreenToClient(hwnd, &pt);
			nHeight = pt.y;
		}

		if (CreateClient(hwnd, 0, top, nWidth, nHeight - top))
		{
			if (PCWSTR id = ((INID*)lpCreateParams)->id)
			{
				SetIcons(hwnd, ((INID*)lpCreateParams)->hInstance, id);
			}

			if (_ZGLOBALS* p = ZGLOBALS::get())
			{
				p->hwndMain = hwnd;
			}

			ZIdle::Insert();

			return TRUE;
		}
	}

	return FALSE;
}

LRESULT ZSDIFrameWnd::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 320;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;

	case WM_NOTIFY:
		return OnNotify((LPNMHDR)lParam);

	case WM_NCDESTROY:
		RundownGUI();
		break;

	case WM_DESTROY:
		ZIdle::Remove();
		break;

	case WM_ERASEBKGND:
		return ZToolBar::EraseBackground((HDC)wParam, hwnd);

	case WM_COMMAND:
		if (_pActiveDoc && _nCmdId && findWORD(_nCmdId, _pCmdId, LOWORD(wParam)))
		{
			return _pActiveDoc->OnCmdMsg(wParam, lParam);
		}
		break;
	}

	return ZFrameMultiWnd::WindowProc(hwnd, uMsg, wParam, lParam);
}

BOOL ZSDIFrameWnd::CreateSB(HWND /*hwnd*/)
{
	return TRUE;
}

BOOL ZSDIFrameWnd::CreateTB(HWND /*hwnd*/)
{
	return TRUE;
}

HWND ZSDIFrameWnd::Create(PCWSTR lpWindowName, HINSTANCE hInstance, PCWSTR id, BOOL bNoMenu)
{
	WORD const * ppcCmdId;

	if (DWORD nCount = getDocumentCmdId(&ppcCmdId))
	{
		if (_pCmdId = new WORD[nCount])
		{
			_pcCmdId = ppcCmdId;
			_nCmdId = nCount;
			__stosw(_pCmdId, 0, nCount);
		}
		else
		{
			return 0;
		}
	}

	int x = CW_USEDEFAULT, y = CW_USEDEFAULT, nWidth = CW_USEDEFAULT, nHeight = CW_USEDEFAULT, 
		dwStyle = WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_CLIPSIBLINGS|WS_VISIBLE;

	INID ii = { hInstance, id };

	return ZFrameWnd::Create(0, lpWindowName, dwStyle, 
		x, y, nWidth, nHeight, 0, id && !bNoMenu ? CMenu::Load(hInstance, id) : 0, &ii);
}

void ZSDIFrameWnd::SetStatusText(int i, LPCWSTR pszStatusText)
{
	SendMessage(ZStatusBar::getHWND(), SB_SETTEXT, i, (LPARAM)pszStatusText);
}

HRESULT ZSDIFrameWnd::QI(REFIID riid, void **ppvObject)
{
	if (riid == __uuidof(ZSDIFrameWnd))
	{
		*ppvObject = static_cast<ZObject*>(this);
		AddRef();
		return S_OK;
	}

	return ZFrameMultiWnd::QI(riid, ppvObject);
}

HWND ZGenFrame::CreateView(HWND hWndParent, int nWidth, int nHeight, PVOID lpCreateParams)
{
	HWND hwnd = 0;
	if (ZWnd* p = reinterpret_cast<CFP*>(lpCreateParams)->pfnCreate())
	{
		hwnd = p->Create(0, 0, WS_CHILD|WS_VISIBLE, 0, 0, nWidth, nHeight, hWndParent, NULL, reinterpret_cast<CFP*>(lpCreateParams)->lpCreateParams);
		p->Release();
	}
	return hwnd;
}

_NT_END
