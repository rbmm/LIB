#include "stdafx.h"

_NT_BEGIN

#include "TxtWnd.h"

void ZTxtWnd::OnNewFont(HFONT /*hFont*/)
{
	RECT rc;
	HWND hwnd = getHWND();
	if (GetClientRect(hwnd, &rc))
	{
		OnSize(hwnd, rc.right, rc.bottom);
		InvalidateRect(hwnd, 0, TRUE);
	}
}

void ZTxtWnd::GetUnitSize(SIZE& u)
{
	ZGLOBALS::getFont()->getSIZE(&u);
	u.cy += getAscent();
}

PVOID ZTxtWnd::BeginDraw(HDC hdc, PRECT /*prc*/)
{
	return SelectObject(hdc, ZGLOBALS::getFont()->getFont());
}

void ZTxtWnd::DrawIndent(HDC /*hdc*/, DWORD /*y*/, DWORD /*line*/)
{

}

void ZTxtWnd::EndDraw(HDC hdc, PVOID pv)
{
	SelectObject(hdc, (HGDIOBJ)pv);
}

void ZTxtWnd::OnDraw(HDC hdc, int x, int y, int Vx, int Vy, int nx, int ny, PRECT prc)
{
	PVOID pv = BeginDraw(hdc, prc);
	
	SIZE u;
	GetUnitSize(u);

	if (ny && nx)
	{
		//y += 1;//getAscent();

		do 
		{
			DrawLine(hdc, x, y, Vy++, Vx, nx);
			y += u.cy;
		} while (--ny);
	}

	EndDraw(hdc, pv);
}

void ZTxtWnd::OnDrawIndent(HDC hdc, int y, int Vy, int ny, PRECT prc)
{
	PVOID pv = BeginDraw(hdc, prc);

	SIZE u;
	GetUnitSize(u);

	if (ny)
	{
		do 
		{
			DrawIndent(hdc, y, Vy++);
			y += u.cy;
		} while (--ny);
	}

	EndDraw(hdc, pv);
}

HRESULT ZTxtWnd::QI(REFIID riid, void **ppvObject)
{
	if (riid == __uuidof(ZTxtWnd))
	{
		*ppvObject = static_cast<ZObject*>(this);
		AddRef();
		return S_OK;
	}

	return ZScrollWnd::QI(riid, ppvObject);
}

_NT_END