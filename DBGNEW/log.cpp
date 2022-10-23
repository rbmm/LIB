#include "StdAfx.h"

_NT_BEGIN

#include "log.h"

HWND ZLogFrame::CreateView(HWND hWndParent, int nWidth, int nHeight, PVOID /*lpCreateParams*/)
{
	if (HWND hwnd = CreateWindowEx(0, WC_EDIT, 0, WS_VISIBLE|WS_CHILD|
		WS_VSCROLL|WS_HSCROLL|ES_MULTILINE|ES_WANTRETURN, 0, 0, nWidth, nHeight, hWndParent, 0, 0, 0))
	{
		SendMessage(hwnd, WM_SETFONT, (WPARAM)ZGLOBALS::getFont()->getFont(), 0);
		SendMessage(hwnd, EM_LIMITTEXT, MAXLONG, 0);
		return hwnd;
	}

	return 0;
}

ZView* ZLogFrame::getView()
{
	return this;
}

ZWnd* ZLogFrame::getWnd()
{
	return this;
}

BOOL ZLogFrame::CanCloseFrame()
{
	return !_pDocument;
}

BOOL ZLogFrame::CanClose()
{
	return !_pDocument;
}

void ZLogFrame::DestroyView()
{
	Detach();
	if (_pFrame->GetActive(0) == getHWND())
	{
		_pFrame->SetActiveDoc(0);
	}
}

ZLogFrame::ZLogFrame(ZDocument* pDocument) : ZView(pDocument)
{
}

void ZLogFrame::OnNewFont(HFONT hFont)
{
	SendMessage(_hwndView, WM_SETFONT, (WPARAM)hFont, TRUE);
}

HWND CreateLogView(ZDocument* pDocument, DWORD dwProcessId)
{
	HWND hwnd = 0;

	if (ZLogFrame* p = new ZLogFrame(pDocument))
	{
		WCHAR title[64];
		swprintf(title, L"%X Log", dwProcessId);
		if (p->Create(title, 0))
		{
			hwnd = p->GetPane();
		}
		p->Release();
	}

	return hwnd;
}

_NT_END