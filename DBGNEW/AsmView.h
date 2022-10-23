#pragma once

#include "addressview.h"
#include "Dll.h"

class ZDbgDoc;
class ZSrcView;

class __declspec(uuid("E5D28598-41C3-485a-9932-E68FDBB23E69")) ZAsmView : public ZAddressView
{
	friend ZDbgDoc;

	enum {
		fIsNameLine = 0x01,
		fIsBpLine = 0x02,
		fIsBpDisable = 0x04,
		fIsBpExp = 0x08,
		fInvalidLine = 0x10,
		fFinalLine = 0x20,
		fLinkLine = 0x40
	};

	struct LINEINFO 
	{
		union
		{
			INT_PTR jumpEip;
			ULONG Index;
		};
		INT_PTR Va;
		BYTE Flags, len, aLink, zLink;
		BYTE buf[16];
	};

	struct FORMATDATA  
	{
		ZDll* pDll;
		ZDbgDoc* pDoc;
		LINEINFO* pLI;
		size_t fixupLen;
	};

	DIS* _pDisasm;
	ZDll* _pDll;
	LINEINFO* _pLI;
	INT_PTR _StackEip[16], _pcVa, _Va;
	DIS::DIST _iDist;
	ULONG _iStack;
	short _xMouse, _yMouse;
	BOOLEAN _bLinkActive, _bTrackActive, _bDrawActive;

	static size_t __stdcall fixupSet(DIS const *,unsigned __int64,size_t,wchar_t *,size_t,unsigned __int64 *);
	static size_t __stdcall addrSet(DIS const *,unsigned __int64,wchar_t *,size_t,unsigned __int64 *);

	DWORD DisUp(INT_PTR Va, LINEINFO* pLI, DWORD n, ULONG _index);

	DWORD DisDown(INT_PTR Va, LINEINFO* pLI, DWORD n, ULONG _index);

	void DrawLink(int iLine, BOOL fActivate);

	void DeActivateLinkEx(BOOL fDraw = TRUE);

	void ActivateLink(BOOL fActivate, BOOL fDraw = TRUE);

	void OnMouseMove(int x, int y);

	void OnIndentDown(int line);

	INT_PTR AdjustAddress(INT_PTR Address);

	BOOL RecalcLayout(HWND hwnd, int cy);

	virtual BOOL CanCloseFrame();

	virtual void OnUpdate(ZView* pSender, LPARAM lHint, PVOID pHint);

	virtual int ScrollLines(int nBar, int nLines, int& nPos);

	virtual int NewPos(int nBar, int nPos, BOOL bTrack);

	virtual BOOL GoTo(HWND hwnd, INT_PTR Address);

	virtual void OnSize(HWND hwnd, int cx, int cy);

	virtual LRESULT WindowProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

	virtual void DrawLine(HDC hdc, DWORD x, DWORD y, DWORD line, DWORD column, DWORD len);

	virtual BOOL FormatLineForDrag(DWORD line, PSTR buf, DWORD cb);

	virtual int GetIndent();

	virtual void DrawIndent(HDC hdc, DWORD y, DWORD line);

	virtual PVOID BeginDraw(HDC hdc, PRECT prc);

	virtual void EndDraw(HDC hdc, PVOID pv);

	virtual void GetVirtualSize(SIZE& N);

	virtual PCUNICODE_STRING getRegName();

	virtual void OnMenuItemSelect(HMENU hmenu, DWORD id, HWND hwnd);

	virtual HMENU GetContextMenu(HMENU& rhsubmenu);

	virtual void OnLButtonDown(HWND hwnd, int x, int y);

	BOOL GotoSrc(ULONG_PTR Address);

	BOOL VaToLine(ULONG_PTR Address, ZSrcView **ppView, PULONG pLine);

	~ZAsmView();

	ZAsmView(ZDocument* pDocument);

	PWSTR FormatLine(DWORD line, PWSTR buf, ULONG cch);

public:

	enum ACTION {
		BpNone,
		BpAdded,
		BpRemoved,
		BpActivated,
		BpDisabled,
		BpExpAdded,
		BpExpRemoved,
		actPC,
		actMarker
	};

	DIS::DIST GetTarget(){ return _iDist; }

	BOOL SetTarget(DIS::DIST i);

	BOOL canBack() { return _iStack; }

	void ShowTarget();

	void JumpBack();

	void InvalidateVa(INT_PTR Va, ACTION action);

	void setVA(INT_PTR Va);

	void _setPC(INT_PTR Va);

	void setPC(INT_PTR Va);

	void OnUnloadDll(ZDll* pDll);

	virtual HRESULT QI(REFIID riid, void **ppvObject);

	static ZWnd* createObject(ZDocument* pDocument)
	{
		return new ZAsmView(pDocument);
	}

	void Activate();
};