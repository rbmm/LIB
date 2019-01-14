#pragma once

#include "scroll.h"

class WINZ_API Z_INTERFACE("0B71E773-1F19-4f1f-917C-E04694C7992D") ZTxtWnd : public ZScrollWnd, ZFontNotify
{
protected:
	
	DWORD getAscent(){ return 3; }

	virtual void GetUnitSize(SIZE& u);

	virtual void OnDrawIndent(HDC hdc, int y, int Vy, int ny, PRECT prc);

	virtual void OnDraw(HDC hdc, int x, int y, int Vx, int Vy, int nx, int ny, PRECT prc);

	virtual void OnNewFont(HFONT hFont);

	//---------------------------------------------

	virtual void DrawLine(HDC hdc, DWORD x, DWORD y, DWORD line, DWORD column, DWORD len) = 0;
	
	virtual void DrawIndent(HDC hdc, DWORD y, DWORD line);

	virtual PVOID BeginDraw(HDC hdc, PRECT prc);

	virtual void EndDraw(HDC hdc, PVOID pv);

public:

	virtual HRESULT QI(REFIID riid, void **ppvObject);
};
