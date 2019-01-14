#include "stdafx.h"

_NT_BEGIN

#include "view.h"
#include "app.h"
#include "Frame.h"

void ZView::Detach()
{
	RemoveEntryList(this);
	InitializeListHead(this);

	if (_pDocument)
	{
		_pDocument->Release();
		_pDocument = 0;
	}
}

ZView::ZView(ZDocument* pDocument)
{
	InitializeListHead(this);

	_pDocument = pDocument;
	pDocument->AddRef();
	pDocument->AddView(this);
}

ZView::~ZView()
{
	Detach();
}

ZView* ZView::get(HWND hwnd)
{
	ZView* pView = 0;

	if (hwnd)
	{
		if (ZWnd* p = ZWnd::FromHWND(hwnd))
		{
			pView = p->getView();

			p->Release();
		}
	}
	return pView;
}

BOOL ZView::CanClose(HWND hwnd)
{
	BOOL f = TRUE;

	if (hwnd)
	{
		if (ZWnd* p = ZWnd::FromHWND(hwnd))
		{
			if (ZView* pView = p->getView())
			{
				f = pView->getWnd()->CanCloseFrame();
			}
			p->Release();
		}
	}

	return f;
}

void ZView::DestroyView()
{
	HWND hwnd = getWnd()->getHWND();

	if ((GetWindowLongW(hwnd, GWL_STYLE) & WS_CHILD) && !(GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_MDICHILD))
	{
		hwnd = GetParent(hwnd);
	}
	
	if (GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_MDICHILD)
	{
		SendMessage(GetParent(hwnd), WM_MDIDESTROY, (WPARAM)hwnd, 0);
	}
	else
	{
		DestroyWindow(hwnd);
	}
	//SendMessage(hwnd, WM_CLOSE, 0, 0);
}

void ZView::OnUpdate(ZView* /*pSender*/, LPARAM /*lHint*/, PVOID /*pHint*/)
{
}

void ZView::OnDocumentActivate(BOOL /*bActivate*/)
{
}

_NT_END
