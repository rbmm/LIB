#pragma once

#include "../winz/dragptr.h"
#include "../winz/txtwnd.h"
#include "../winz/cursors.h"
#include "adresswnd.h"
#include "eval64.h"
#include "../inc/rtlframe.h"

BOOL GetValidRange(HANDLE hProcess, INT_PTR Address, INT_PTR& rLo, INT_PTR& rHi);
void InsertStringLB(HWND hwndList, PCWSTR sz);

class __declspec(novtable) ZAddressView : public ZTxtWnd, public ZView, protected ZDragPtr
{
	void PtrFromPoint(int x, int y);
protected:
	HWND _hwndEdit, _hwndList;
	PVOID _pvBuf;
	INT_PTR _BaseAddress, _Address, _HigestAddress;
	DWORD _ViewSize, _cbBuf, _nLines, _BytesPerLine, _BytesShown, _vCaret, _maxCaret;
	int _dxHotspot, _dyHotspot;
	UCHAR _mm, _cc, _lPressed, _ptrLen;
	CHAR _caret;
	BOOLEAN _valid, _focus, _capture;

	BOOL GoTo(HWND hwnd, PCWSTR sz);

	void Invalidate(HWND hwnd);

	void HideCaret(HWND hwnd);

	void ShowCaret(HWND hwnd);

	void DestroyCaret(HWND hwnd);

	virtual LRESULT WindowProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
	
	virtual BOOL GoTo(HWND hwnd, INT_PTR Address) = 0;

	virtual HMENU GetContextMenu(HMENU& rhsubmenu) = 0;
	
	virtual void OnMenuItemSelect(HMENU hmenu, DWORD id, HWND hwnd);

	virtual PCUNICODE_STRING getRegName() = 0;

	virtual BOOL FormatLineForDrag(DWORD line, PSTR buf, DWORD cb);

	virtual void OnLButtonDown(HWND hwnd, int x, int y) = 0;

	ZAddressView(ZDocument* pDocument = 0);

	~ZAddressView();

	virtual ZWnd* getWnd();

	virtual ZView* getView();

public:
	void InsertString(PCWSTR sz);
};

typedef ZWnd* (*PFNCREATEOBJECT)(ZDocument* pDocument);

HWND ZAddressCreateClient(HWND hwnd, int nWidth, int nHeight, PFNCREATEOBJECT pfnCreateObject, ZDocument* pDocument);

#define SYM_IN_PTR (2*sizeof(PVOID))

