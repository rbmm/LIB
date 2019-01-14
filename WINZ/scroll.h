#pragma once

#include "view.h"

class WINZ_API Z_INTERFACE("79862D10-44E0-4f9a-9211-9CD0BA82F325") ZScrollWnd : public ZWnd
{
	POINT _nPos;
protected:
	BOOLEAN _TrackEnabled;

	void OnScroll(HWND hwnd, int nBar, int code);

	void Resize(HWND hwnd, int cx, int cy, BOOL bNeedRepaint);

	void GoTo(HWND hwnd, int x, int y);

	void GetPos(POINT& nPos);

	void SetPos(POINT& nPos);

	virtual void OnDrawIndent(HDC hdc, int y, int Vy, int ny, PRECT prc) = 0;

	virtual void OnDraw(HDC hdc, int x, int y, int Vx, int Vy, int nx, int ny, PRECT prc) = 0;

	virtual void GetUnitSize(SIZE& u) = 0;

	virtual void GetVirtualSize(SIZE& N) = 0;

	virtual int ScrollLines(int nBar, int nLines, int& nPos);

	virtual int NewPos(int nBar, int nPos, BOOL bTrack);

	virtual int GetIndent();

	virtual void OnSize(HWND hwnd, int cx, int cy);

	virtual LRESULT WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

	ZScrollWnd();
public:
	virtual HRESULT QI(REFIID riid, void **ppvObject);
};
