#pragma once

#include "window.h"

class ZSplitWndV;
class ZSplitWndH;

class WINZ_API Z_INTERFACE("A1C070DA-EEAD-4304-9528-C08DD3488A44") ZSplitWnd : public ZWnd, SIZE
{
	friend ZSplitWndV;
	friend ZSplitWndH;

	HWND _hwndCld[2];
	int _t, _tPrev;
	HDC _hdc;
protected:
	virtual HWND CreateChild(BOOL lt, HWND hwndParent, int x, int y, int nWidth, int nHeight) = 0;
	virtual BOOL CreateChilds(HWND hwnd) = 0;
	virtual BOOL PointInSplit(POINT pt) = 0;
	virtual void MoveChilds() = 0;
	virtual void DrawSplit(HDC hdc) = 0;
	virtual void DrawMovingSplit() = 0;
	virtual int GetT(POINT pt) = 0;
	virtual int GetT(LPARAM lParam) = 0;
	virtual int GetHitCode() = 0;
public:

	ZSplitWnd(int t);

	~ZSplitWnd();

	BOOL OnCreate(HWND hwnd);

	BOOL MouseOnSplit(HWND hwnd, LPARAM lParam);

	virtual HRESULT QI(REFIID riid, void **ppvObject);

	virtual LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	HWND getPaneWnd(BOOL lt) { return _hwndCld[!lt];}
};

class WINZ_API Z_INTERFACE("19360E34-091E-4be3-AC0C-0AD3E7DF9297") ZSplitWndV : public ZSplitWnd
{
	virtual BOOL CreateChilds(HWND hwnd);
	virtual BOOL PointInSplit(POINT pt);
	virtual void MoveChilds();
	virtual void DrawSplit(HDC hdc);
	virtual void DrawMovingSplit();
	virtual int GetT(POINT pt);
	virtual int GetT(LPARAM lParam);
	virtual int GetHitCode();
public:
	virtual HRESULT QI(REFIID riid, void **ppvObject);

	ZSplitWndV(int t) : ZSplitWnd(t)
	{
	}
};

class WINZ_API Z_INTERFACE("55AB9FCE-A308-4c22-87E8-50474F42CAFD") ZSplitWndH : public ZSplitWnd
{
	virtual BOOL CreateChilds(HWND hwnd);
	virtual BOOL PointInSplit(POINT pt);
	virtual void MoveChilds();
	virtual void DrawSplit(HDC hdc);
	virtual void DrawMovingSplit();
	virtual int GetT(POINT pt);
	virtual int GetT(LPARAM lParam);
	virtual int GetHitCode();
public:
	virtual HRESULT QI(REFIID riid, void **ppvObject);

	ZSplitWndH(int t) : ZSplitWnd(t)
	{
	}
};
