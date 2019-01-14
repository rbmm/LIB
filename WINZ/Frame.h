#pragma once

#include "window.h"
#include "app.h"
#include "layout.h"
#include "ctrl.h"
#include "document.h"

class WINZ_API Z_INTERFACE("25309676-9598-4e5a-913F-F1B090980438") ZFrameWnd : public ZWnd
{
protected:

	HWND _hwndView;

	virtual HRESULT QI(REFIID riid, void **ppvObject);

	virtual HWND CreateView(HWND hWndParent, int nWidth, int nHeight, PVOID lpCreateParams) = 0;

	virtual LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	virtual void DoResize(WPARAM wParam, LPARAM lParam);

	ZFrameWnd() { _hwndView = 0; }

	virtual PCUNICODE_STRING getPosName();

	virtual BOOL CanClose();
public:
	
	virtual ZView* getView();

	HWND Create(
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
		);

	HWND GetPane() { return _hwndView; }
};

class WINZ_API Z_INTERFACE("3966478D-3D09-4b39-B126-94E0F25DECAC") ZFrameMultiWnd : public ZFrameWnd, public CUILayot
{
protected:

	virtual HRESULT QI(REFIID riid, void **ppvObject);

	virtual HWND CreateView(HWND hWndParent, int nWidth, int nHeight, PVOID lpCreateParams);

	virtual BOOL CreateClient(HWND hWndParent, int nWidth, int nHeight, PVOID lpCreateParams) = 0;
	
	virtual void DoResize(WPARAM wParam, LPARAM lParam);

	void SetView(HWND hwndView) { _hwndView = hwndView; }
};

class WINZ_API Z_INTERFACE("889D42C2-933F-4c21-8CEB-0AF86DD7180E") ZSDIFrameWnd : public ZFrameMultiWnd, 
	public ZToolBar, 
	public ZStatusBar, 
	public CIcons, 
	public CMenu,
	public ZIdle
{
	struct INID 
	{
		HINSTANCE hInstance;
		LPCWSTR id;
	};

	BOOL OnCreate(HWND hwnd, INID* lpCreateParams);
	virtual BOOL CreateClient(HWND hWndParent, int nWidth, int nHeight, PVOID lpCreateParams);

protected:

	ZDocument* _pActiveDoc;
	WORD const* _pcCmdId;
	PWORD _pCmdId;
	DWORD _nCmdId;

	virtual LRESULT OnNotify(LPNMHDR lpnm);
	virtual LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual BOOL CreateSB(HWND hwnd);
	virtual BOOL CreateTB(HWND hwnd);
	virtual BOOL CreateClient(HWND hwnd, int x, int y, int nWidth, int nHeight) = 0;
	virtual DWORD getDocumentCmdId(WORD const** ppCmdId);
	virtual void OnIdle();

public:
	HWND Create(LPCWSTR lpWindowName, HINSTANCE hInstance, LPCWSTR id, BOOL bNoMenu = FALSE);
	
	void SetStatusText(int i, LPCWSTR pszStatusText);

	void SetActiveDoc(ZDocument* pDoc);

	ZDocument* GetActiveDoc() { return _pActiveDoc; }

	ZSDIFrameWnd();

	virtual HRESULT QI(REFIID riid, void **ppvObject);

	virtual ~ZSDIFrameWnd();
};

class ZGenFrame : public ZFrameWnd
{
public:
	struct CFP 
	{
		ZWnd* (WINAPI * pfnCreate)();
		PVOID lpCreateParams;
	};
private:
	virtual HWND CreateView(HWND hWndParent, int nWidth, int nHeight, PVOID lpCreateParams);
};
