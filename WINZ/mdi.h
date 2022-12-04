#pragma once

#include "app.h"
#include "Frame.h"
#include "ctrl.h"

class ZMDIChildFrame;

class WINZ_API Z_INTERFACE("DE58B1DD-2F00-4c9a-9F82-F0EF96AA03B2") ZMDIFrameWnd : public ZSDIFrameWnd, public ZTabBar
{
	friend ZMDIChildFrame;
protected:

	void OnCreateChild(HWND hwnd);

	void OnDestroyChild(HWND hwnd);

	virtual BOOL CreateClient(HWND hwnd, int x, int y, int nWidth, int nHeight);

	virtual void OnIdle();

	HWND _hwndMDI;
	BOOLEAN _bChildChanged, _bClientEdge;

	virtual LRESULT OnNotify(LPNMHDR lpnm);
	virtual LRESULT DefWinProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

public:

	virtual HRESULT QI(REFIID riid, void **ppvObject);

	ZMDIFrameWnd();

	void SetTitleText(HWND hwnd, PCWSTR pszText);
	
	HWND GetActive(PBOOL pbZoomed = 0);
};

class WINZ_API Z_INTERFACE("B656C55A-EDE6-47f6-833E-5D714A549F85") ZMDIChildFrame : public ZWnd
{
protected:
	ZMDIFrameWnd* _pFrame;
	HWND _hwndView;

	virtual LRESULT OnNotify(LPNMHDR lpnm);

	virtual void DoResize(WPARAM wParam, LPARAM lParam);

	virtual LRESULT DefWinProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	
	virtual LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	virtual HWND CreateView(HWND hWndParent, int nWidth, int nHeight, PVOID lpCreateParams) = 0;

	virtual BOOL CanClose();
public:

	virtual HRESULT QI(REFIID riid, void **ppvObject);

	HWND Create(PCWSTR lpWindowName, PVOID lpParam);

	void Activate();

	static void _Activate(HWND hwndMDIClient, HWND hwnd);

	ZMDIChildFrame();

	virtual ZView* getView();

	HWND GetPane() { return _hwndView; }
};

class WINZ_API Z_INTERFACE("D2215DFF-C39E-47b6-B926-0D602FC013C1") ZMDIChildMultiFrame : public ZMDIChildFrame, public CUILayot
{
protected:

	virtual HRESULT QI(REFIID riid, void **ppvObject);

	virtual BOOL CreateClient(HWND hWndParent, int nWidth, int nHeight, PVOID lpCreateParams) = 0;

	virtual HWND CreateView(HWND hWndParent, int nWidth, int nHeight, PVOID lpCreateParams);

	virtual void DoResize(WPARAM wParam, LPARAM lParam);

	void SetView(HWND hwndView) { _hwndView = hwndView; }
};
