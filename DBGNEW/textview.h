#pragma once

#include "../winz/window.h"

class __declspec(novtable) ZTxtView : public ZWnd
{
	void OnPaint(HWND hwnd);
public:
protected:
	void Transform(int ox, int& x, int& y);
	virtual LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual void OnSize(DWORD cx, DWORD cy, DWORD dx, DWORD dy);
	virtual BOOL PrePaint(HDC hdc, PRECT prc);
	virtual void PostPaint(HDC hdc, PRECT prc);
	virtual void DrawLine(HDC hdc, int i, int y, PRECT prc) = 0;
};

class ZFixedFont : SIZE
{
	HFONT _hfont;
public:
	ZFixedFont();

	~ZFixedFont();

	HFONT GetFont()
	{
		return _hfont;
	}

	int getWidth()
	{
		return cx;
	}

	int getHeight()
	{
		return cy;
	}

	static ZFixedFont* GetObject()
	{
		static ZFixedFont s_font;
		return &s_font;
	}
};
