#pragma once

#include "../winz/cursors.h"
#include "../winz/Frame.h"
#include "../winZ/dragptr.h"

class ZDbgDoc;

class ZRegView : public ZWnd, ZFontNotify, ZDragPtr, public CONTEXT
{
	ZDbgDoc* _M_pDoc = 0;
	DWORD _vCaret, _Ticks;
	int _dxHotspot, _dyHotspot;
	CHAR _caret;
	BOOLEAN _focus, _capture, _lPressed, _bDisabled, _bTempHide;

	virtual void OnNewFont(HFONT /*hFont*/);

	virtual LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	void HideCaret(HWND hwnd);

	void ShowCaret(HWND hwnd);

	void DestroyCaret(HWND hwnd);

	void CreateCaret(HWND hwnd);

	BOOL SetCaretPos();

	BOOL SetCaretPos(DWORD vCaret);

	void PtrFromPoint(int x, int y);

public:
	ZRegView();

	void SetDoc(ZDbgDoc* pDoc)
	{
		_M_pDoc = pDoc;
	}

	void SetContext(CONTEXT* ctx);
	
	void SetDisabled();
	
	void TempHide(BOOL bShow);

	static void GetPos(PRECT prc);
};

HWND createRegView(ZRegView** ppReg, DWORD dwProcessId);