#include "StdAfx.h"

_NT_BEGIN

#include "../winz/Frame.h"
#include "../winz/cursors.h"
#include "resource.h"
#include "eval64.h"
#include "addressview.h"
#include "common.h"

class __declspec(uuid("79607151-AC72-424f-8E92-E3A496DAF7C0")) ZMemView : public ZAddressView
{
	static DWORD s_cxvscroll;

	virtual BOOL GoTo(HWND hwnd, INT_PTR Address);

	BOOL DoRead();

	BOOL DoRead(PVOID to, PVOID from, DWORD cb);

	INT_PTR AdjustAddress(INT_PTR Address);

	void CreateCaret(HWND hwnd);

	virtual LRESULT WindowProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

	virtual void DrawLine(HDC hdc, DWORD x, DWORD y, DWORD line, DWORD column, DWORD len);

	virtual PVOID BeginDraw(HDC hdc, PRECT prc);

	virtual void EndDraw(HDC hdc, PVOID pv);

	virtual void GetVirtualSize(SIZE& N);

	virtual void OnSize(HWND hwnd, int cx, int cy);

	virtual void OnUpdate(ZView* pSender, LPARAM lHint, PVOID pHint);

	virtual int ScrollLines(int nBar, int nLines, int& nPos);

	virtual int NewPos(int nBar, int nPos, BOOL bTrack);

	BOOL RecalcLayout(HWND hwnd, int cx, int cy);

	void SetCaretPos(int x, int y);

	DWORD TransformCaretPos(DWORD vCaret);

	BOOL SetCaretPos();

	BOOL SetCaretPos(DWORD vCaret);

	void DrawC(DWORD ofs, HDC hdc, DWORD I, DWORD N, int cx, int cy);
	
	void Draw4B(DWORD ofs, UCHAR t, HDC hdc, DWORD I, DWORD N, int cx, int cy);
	
	void Draw4B(DWORD ofs, UCHAR t);

	virtual BOOL FormatLineForDrag(DWORD line, PSTR buf, DWORD cb);

	virtual void OnLButtonDown(HWND hwnd, int x, int y);

	virtual HMENU GetContextMenu(HMENU& rhsubmenu);

	virtual void OnMenuItemSelect(HMENU hmenu, DWORD id, HWND hwnd);

	virtual PCUNICODE_STRING getRegName();

	virtual void OnDocumentActivate(BOOL bActivate)
	{
		ShowWindow(GetParent(getHWND()), bActivate ? SW_SHOW : SW_HIDE);
	}

	~ZMemView();

	ZMemView(ZDocument* pDocument);

public:

	static ZWnd* createObject(ZDocument* pDocument)
	{
		return new ZMemView(pDocument);
	}

	virtual HRESULT QI(REFIID riid, void **ppvObject);
};

DWORD ZMemView::s_cxvscroll;

void FormatLine(PWSTR sz, INT_PTR Address, PVOID Buffer, UCHAR mm, UCHAR cc)
{
	DWORD n = cc * mm;

	sz += swprintf(sz, L"%p ", (void*)Address);

	union
	{
		PVOID pv;
		PUCHAR pb;
		PUSHORT pw;
		PULONG pd;
		PULONGLONG pq;
	} buf = { Buffer };

	do 
	{
		switch (mm)
		{
		case 1:
			sz += swprintf(sz, L" %02X", *buf.pb++);
			break;
		case 2:
			sz += swprintf(sz, L" %04X", *buf.pw++);
			break;
		case 4:
			sz += swprintf(sz, L" %08X", *buf.pd++);
			break;
		case 8:
			sz += swprintf(sz, L" %016I64X", *buf.pq++);
			break;
		default:__assume(FALSE);
		}

	} while (--cc);

	*sz++ = ' ', *sz++ = ' ';

	buf.pv = Buffer;

	do 
	{
		UCHAR c = *buf.pb++;
		if ((UCHAR)(c - 0x20) > 0x5e) c = ' ';
		sz += swprintf(sz, L"%c", c);
	} while (--n);
}

ZMemView::~ZMemView()
{
}

ZMemView::ZMemView(ZDocument* pDocument) : ZAddressView(pDocument)
{
	_mm = 1;
	_vCaret = 0;
	_TrackEnabled = GetDbgDoc()->IsLocalMemory();
}

BOOL ZMemView::GoTo(HWND hwnd, INT_PTR Address)
{
	INT_PTR Lo, Hi;

	if (!GetDbgDoc()->GetValidRange(Address, Lo, Hi))
	{
		return FALSE;
	}

	_valid = TRUE;
	_BaseAddress = Lo;
	_HigestAddress = Hi -_BytesShown;
	_ViewSize = RtlPointerToOffset(Lo, Hi);

	_Address = AdjustAddress(Address);

	if (!DoRead())
	{
		return FALSE;
	}

	CreateCaret(hwnd);

	WCHAR sz[128];
	swprintf(sz, L"%x [%p, %p)", GetDbgDoc()->getId(), (void*)Lo, (void*)Hi);
	SetWindowText(GetParent(hwnd), sz);

	ZScrollWnd::GoTo(hwnd, 0, RtlPointerToOffset(_BaseAddress, _Address) / _BytesPerLine);

	return TRUE;
}

BOOL ZMemView::DoRead()
{
	return DoRead(_pvBuf, (PVOID)_Address, _BytesShown);
}

BOOL ZMemView::DoRead(PVOID to, PVOID from, DWORD cb)
{
	SIZE_T rcb;
	if (0 <= GetDbgDoc()->Read(from, to, cb, &rcb) && rcb == cb)
	{
		return TRUE;
	}

	if (cb == _BytesShown || GetDbgDoc()->IsLocalMemory())
	{
		Invalidate(getHWND());
	}

	return FALSE;
}

INT_PTR ZMemView::AdjustAddress(INT_PTR Address)
{
	if (Address < _BaseAddress)
	{
		return Address + ((RtlPointerToOffset(Address, _BaseAddress) + _BytesPerLine - 1) / _BytesPerLine) * _BytesPerLine;
	}

	if (_HigestAddress < Address)
	{
		return Address - ((RtlPointerToOffset(_HigestAddress, Address) + _BytesPerLine - 1) / _BytesPerLine) * _BytesPerLine;
	}

	return Address;
}

void ZMemView::CreateCaret(HWND hwnd)
{
	if (_valid && _focus && !_caret)
	{
		SIZE v;
		GetUnitSize(v);
		if (::CreateCaret(hwnd, 0, 1, v.cy - 6))
		{
			_caret = +1;
			SetCaretPos();
			::ShowCaret(hwnd);
		}
	}
}

PVOID ZMemView::BeginDraw(HDC hdc, PRECT prc)
{
	HideCaret(getHWND());
	FillRect(hdc, prc, (HBRUSH)(1+COLOR_WINDOW));//COLOR_INFOBK
	SetBkColor(hdc, GetSysColor(COLOR_WINDOW));//COLOR_INFOBK
	return ZTxtWnd::BeginDraw(hdc, prc);
}

void ZMemView::EndDraw(HDC hdc, PVOID pv)
{
	SetCaretPos();
	ZTxtWnd::EndDraw(hdc, pv);
}

void ZMemView::GetVirtualSize(SIZE& N)
{
	N.cx = 3 + SYM_IN_PTR + _cc * (1 + 3 * _mm);
	N.cy = _ViewSize / _BytesPerLine;
}

void ZMemView::OnSize(HWND hwnd, int cx, int cy)
{
	RecalcLayout(hwnd, cx, cy);
	
	if (_valid)
	{
		ZTxtWnd::OnSize(hwnd, cx, cy);
	}
}

BOOL ZMemView::RecalcLayout(HWND hwnd, int cx, int cy)
{
	SIZE u;
	GetUnitSize(u);

	if (!(GetWindowStyle(hwnd) & WS_VSCROLL))
	{
		if (!s_cxvscroll)
		{
			s_cxvscroll = GetSystemMetrics(SM_CXVSCROLL);
		}

		cx -= s_cxvscroll;

		if (cx < 0)
		{
			Invalidate(hwnd);
			return FALSE;
		}
	}

	int n = cx / u.cx;

	UCHAR cc = n > 3 + SYM_IN_PTR ? (UCHAR)((n - 3 - SYM_IN_PTR)/(1 + 3 * _mm)) : 0;

	if (!cc)
	{
		cc = 1;
	}

	DWORD BytesShown = _BytesShown;

	if (_cc != cc)
	{
		_cc = cc;
		InvalidateRect(hwnd, 0, TRUE);
	}

	_BytesPerLine = _mm * _cc, _nLines = (cy + u.cy - 1) / u.cy, _BytesShown = _BytesPerLine * _nLines;

	_maxCaret = ((cy + (u.cy / 3) - 1) / u.cy) * _BytesPerLine * 2;

	if (_vCaret >= _maxCaret)
	{
		_vCaret = 0;
	}

	_HigestAddress = _BaseAddress + _ViewSize -_BytesShown;

	POINT nPos;
	GetPos(nPos);
	nPos.y = RtlPointerToOffset(_BaseAddress, _Address) / _BytesPerLine;
	SetPos(nPos);

	DWORD cbBuf = (_BytesShown + 255) & ~255;
	BOOL bNeedRead = FALSE;

	if (cbBuf != _cbBuf)
	{
		if (_pvBuf) 
		{
			delete [] _pvBuf;
			_cbBuf = 0;
			bNeedRead = TRUE;
		}

		if (_pvBuf = new UCHAR[cbBuf])
		{
			_cbBuf = cbBuf;
		}
		else
		{
			Invalidate(hwnd);
			return FALSE;
		}
	}

	if (_valid)
	{
		INT_PTR Address = AdjustAddress(_Address);

		if (bNeedRead || _Address != Address || BytesShown < _BytesShown)
		{
			_Address = Address;

			return DoRead();
		}

		return TRUE;
	}

	return FALSE;
}

int ZMemView::ScrollLines(int nBar, int nLines, int& nPos)
{
	if (nBar == SB_HORZ)
	{
		return nLines;
	}

	INT_PTR Address = AdjustAddress(_Address + (INT_PTR)nLines * _BytesPerLine);

	if (nLines = (DWORD)((Address - _Address) / _BytesPerLine))
	{
		BOOL bMoveDown;

		DWORD Lines, Bytes, d;

		if (nLines < 0)
		{
			bMoveDown = TRUE, Lines = (DWORD)-nLines;
		}
		else
		{
			bMoveDown = FALSE, Lines = nLines;
		}

		if (Lines < _nLines)
		{
			Bytes = Lines * _BytesPerLine;

			if (bMoveDown)
			{
				memmove(RtlOffsetToPointer(_pvBuf, Bytes), _pvBuf, (_nLines - Lines) * _BytesPerLine);

				if (!DoRead(_pvBuf, (PVOID)Address, Bytes))
				{
					DoRead();
					return 0;
				}
			}
			else
			{
				d = (_nLines - Lines) * _BytesPerLine;

				memcpy(_pvBuf, RtlOffsetToPointer(_pvBuf, Bytes), d);

				if (!DoRead(RtlOffsetToPointer(_pvBuf, d), RtlOffsetToPointer(Address, d), Bytes))
				{
					DoRead();
					return 0;
				}
			}
		}
		else
		{
			if (!DoRead(_pvBuf, (PVOID)Address, _BytesShown))
			{
				return 0;
			}
		}

		_Address = Address;
		nPos = (int)((Address - _BaseAddress) / _BytesPerLine);
	}

	return nLines;
}

int ZMemView::NewPos(int nBar, int nPos, BOOL /*bTrack*/)
{
	if (nBar == SB_VERT)
	{
		INT_PTR Address = _BaseAddress + nPos * _BytesPerLine;

		if (_Address != Address)
		{
			_Address = Address;
			DoRead();
		}
	}

	return nPos;
}

void ZMemView::SetCaretPos(int x, int y)
{
	SIZE v;
	POINT nPos;
	GetUnitSize(v);
	GetPos(nPos);

	int kk = _mm << 1;
	DWORD xx = 1 + kk;
	int maxXPos = _cc * xx - 1;

	x = nPos.x + (x / v.cx) - (2 + SYM_IN_PTR);

	if (x < 0) x = 0;
	if (x > maxXPos) x = maxXPos;

	int j = x % xx;

	if (j == kk)
	{
		j = kk - 1;
	}

	_vCaret = (y / v.cy) * (_BytesPerLine << 1) + (x / xx) * kk + j;

	SetCaretPos();
}

DWORD ZMemView::TransformCaretPos(DWORD vCaret)
{
	int p = 1 - (vCaret & 1);

	vCaret >>= 1;

	DWORD BytesPerLine = _BytesPerLine, mm = _mm;

	int x = vCaret % BytesPerLine, y = vCaret / BytesPerLine;

	return ((y * BytesPerLine + (x / mm) * mm + mm - (x % mm) - 1) << 1) + p;
}

BOOL ZMemView::SetCaretPos()
{
	if (_focus)
	{
		if (SetCaretPos(_vCaret))
		{
			ShowCaret(getHWND());
			return TRUE;
		}
	}
	return FALSE;
}

BOOL ZMemView::SetCaretPos(DWORD vCaret)
{
	SIZE v;
	POINT nPos;
	GetUnitSize(v);
	GetPos(nPos);

	DWORD mm = _mm << 1;
	int x = vCaret % (_BytesPerLine << 1);

	x = (2 + SYM_IN_PTR + (x / mm) * (1 + mm) + (x % mm) - nPos.x) * v.cx;

	if (x < 0)
	{
		return FALSE;
	}

	RECT rc;
	GetClientRect(getHWND(), &rc);

	if (x >= rc.right - v.cx)
	{
		return FALSE;
	}

	return ::SetCaretPos(x, 3 + (vCaret / (_BytesPerLine << 1)) * v.cy);
}

void ZMemView::Draw4B(DWORD ofs, UCHAR t, HDC hdc, DWORD I, DWORD N, int cx, int cy)
{
	DWORD mm = _mm;

	UCHAR c = *RtlOffsetToPointer(_pvBuf, ofs);
	
	DWORD j = ofs / _BytesPerLine, i = ofs % _BytesPerLine;

	i = SYM_IN_PTR + 2 + (i / mm) * (1 + (mm << 1)) + ((mm - 1 - (i % mm)) << 1) + (1 - t) - I;

	if (i < N)
	{
		WCHAR wc[3];
		swprintf(wc, L"%02X", c);

		TextOutW(hdc, i * cx, j * cy, wc + 1 - t, 1);
	}
}

void ZMemView::DrawC(DWORD ofs, HDC hdc, DWORD I, DWORD N, int cx, int cy)
{
	CHAR c = *RtlOffsetToPointer(_pvBuf, ofs);

	DWORD j = ofs / _BytesPerLine, i = SYM_IN_PTR + 3 + _cc * (1 + (_mm << 1)) + (ofs % _BytesPerLine) - I;

	if (i < N)
	{
		WCHAR wc[2];
		swprintf(wc, L"%c", (UCHAR)(c - 0x20) > 0x5e ? ' ' : c);

		TextOutW(hdc, i * cx, j * cy, wc, 1);
	}
}

void ZMemView::Draw4B(DWORD ofs, UCHAR t)
{
	HWND hwnd = getHWND();

	if (HDC hdc = GetDC(hwnd))
	{
		POINT nPos;
		GetPos(nPos);
		SIZE u;
		GetUnitSize(u);
		RECT rc;
		GetClientRect(hwnd, &rc);
		
		SetTextColor(hdc, RGB(255,0,0));
		HGDIOBJ o = SelectObject(hdc, ZGLOBALS::getFont()->getFont());

		HideCaret(hwnd);

		if (t < 2)
		{
			Draw4B(ofs, t, hdc, nPos.x, rc.right, u.cx, u.cy);
		}
		else
		{
			Draw4B(ofs, 0, hdc, nPos.x, rc.right, u.cx, u.cy);
			Draw4B(ofs, 1, hdc, nPos.x, rc.right, u.cx, u.cy);
		}
		DrawC(ofs, hdc, nPos.x, rc.right, u.cx, u.cy);

		ShowCaret(hwnd);

		SelectObject(hdc, o);

		ReleaseDC(hwnd, hdc);
	}
}

void ZMemView::OnLButtonDown(HWND hwnd, int x, int y)
{
	SetCaretPos(x, y);
	if (!_focus) SetFocus(hwnd);
}

BOOL ZMemView::FormatLineForDrag(DWORD line, PSTR buf, DWORD cb)
{
	if (!_valid || cb < SYM_IN_PTR + 2)
	{
		return FALSE;
	}

	PSTR sz = buf;

	DWORD BytesOffset = _BytesPerLine * line;

	union
	{
		PVOID pv;
		PULONG pd;
		PULONGLONG pq;
	} Address = { RtlOffsetToPointer(_pvBuf, BytesOffset) };

	sz += sprintf(sz, "%p ", (void*)(_Address + BytesOffset));

	UCHAR mm = _mm;

	cb -= SYM_IN_PTR + 1, cb /= 1 + 2*mm;

	UCHAR cc = min(_cc, (UCHAR)cb);

	do 
	{
		switch (mm)
		{
		case 4:
			sz += sprintf(sz, " %08X", *Address.pd++);
			break;
#ifdef _WIN64
		case 8:
			sz += sprintf(sz, " %016I64X", *Address.pq++);
			break;
#endif
		}

	} while (--cc);

	return TRUE;
}

void ZMemView::DrawLine(HDC hdc, DWORD x, DWORD y, DWORD line, DWORD column, DWORD len)
{
	UCHAR cc = _cc, mm = _mm;

	DWORD n = (1 + 3 * mm) * cc + 3 + SYM_IN_PTR;
	
	if (column >= n) return;

	if (column + len > n)
	{
		len = n - column;
	}

	PWSTR buf = (PWSTR)alloca((n + 1) << 1);

	POINT nPos;
	GetPos(nPos);

	DWORD BytesOffset = _BytesPerLine * (line - nPos.y);

	FormatLine(buf, _Address + BytesOffset, RtlOffsetToPointer(_pvBuf, BytesOffset), mm, cc);

	TextOutW(hdc, x, y, buf + column, len);
}

STATIC_UNICODE_STRING_(Memory);

void ZMemView::OnMenuItemSelect(HMENU /*hmenu*/, DWORD id, HWND hwnd)
{
	switch (id)
	{
	case ID_0_BYTE:
	case ID_0_WORD:
	case ID_0_DWORD:
	case ID_0_QWORD:

		if (_valid)
		{
			DWORD vCaret = TransformCaretPos(_vCaret);

			_mm = 1 << (id - ID_0_BYTE);

			RECT rc;
			GetClientRect(hwnd, &rc);

			if (RecalcLayout(hwnd, rc.right, rc.bottom))
			{
				vCaret = TransformCaretPos(vCaret);

				if (vCaret < _maxCaret)
				{
					if (_vCaret != vCaret)
					{
						_vCaret = vCaret;
						SetCaretPos();
					}
				}

				ZScrollWnd::GoTo(hwnd, 0, RtlPointerToOffset(_BaseAddress, _Address) / _BytesPerLine);
			}
		}
	}
}

HMENU ZMemView::GetContextMenu(HMENU& rhsubmenu)
{
	if (!_valid) return 0;

	if (HMENU hmenu = LoadMenu((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDR_MENU2)))
	{
		HMENU hsubmenu = GetSubMenu(hmenu, 0);
		
		UINT uIDEnableItem;
		
		switch (_mm)
		{
		case 1: uIDEnableItem = ID_0_BYTE;
			break;
		case 2: uIDEnableItem = ID_0_WORD;
			break;
		case 4: uIDEnableItem = ID_0_DWORD;
			break;
		case 8: uIDEnableItem = ID_0_QWORD;
			break;
		default:__assume(false);
		}
		
		EnableMenuItem(hsubmenu, uIDEnableItem, MF_BYCOMMAND|MF_GRAYED);
		
		rhsubmenu = hsubmenu;

		return hmenu;
	}

	return 0;
}

PCUNICODE_STRING ZMemView::getRegName()
{
	return &Memory;
}

void DrawDiff(HDC hdc, int x, int y, int cx, int cy, PWSTR sz, PWSTR wz, DWORD n);

void ZMemView::OnUpdate(ZView* /*pSender*/, LPARAM lHint, PVOID pHint)
{
	if(!_valid) return;

	ULONG_PTR ofs;
	PBYTE pv, qv;
	HWND hwnd = getHWND();

	switch (lHint)
	{
	case DLL_UNLOADED:
		if (_BaseAddress <= (INT_PTR)pHint && (INT_PTR)pHint < _HigestAddress)
		{
			Invalidate(hwnd);
		}
		break;

	case BYTE_UPDATED:
		ofs = (INT_PTR)pHint - _Address;
		if (ofs < _BytesShown)
		{
			CHAR *pc = RtlOffsetToPointer(_pvBuf, ofs), c = *pc;
			if (0 > GetDbgDoc()->Read(pHint, pc, 1))
			{
				Invalidate(hwnd);
				return ;
			}

			if (c != *pc)
			{
				Draw4B((DWORD)ofs, 2);
			}
		}
		break;
	case ALL_UPDATED:
		qv = (PBYTE)alloca(_BytesShown), pv = (PBYTE)_pvBuf;
		memcpy(qv, pv, _BytesShown);
		if (DoRead())
		{
			if (HDC hdc = GetDC(hwnd))
			{
				POINT nPos;
				GetPos(nPos);
				SIZE u;
				GetUnitSize(u);
				int y = 0, x = -(u.cx * nPos.x);

				HGDIOBJ o = SelectObject(hdc, ZGLOBALS::getFont()->getFont());

				HideCaret(hwnd);

				UCHAR mm = _mm, cc = _cc;

				DWORD n = (1 + 3 * mm) * cc + 4 + SYM_IN_PTR;

				PWSTR buf1 = (PWSTR)alloca(n<<1), buf2 = (PWSTR)alloca(n<<1);

				DWORD nLines = _nLines, BytesPerLine = _BytesPerLine;
				INT_PTR Address = _Address;

				do 
				{
					FormatLine(buf1, Address, qv, mm, cc);
					FormatLine(buf2, Address, pv, mm, cc);

					DrawDiff(hdc, x, y, u.cx, u.cy, buf1, buf2, n);

				} while (y += u.cy, Address += BytesPerLine, pv += BytesPerLine, qv += BytesPerLine, --nLines);

				ShowCaret(hwnd);

				SelectObject(hdc, o);

				ReleaseDC(hwnd, hdc);
			}
		}
		break;
	}
}

LRESULT ZMemView::WindowProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch (uMsg)
	{
	case WM_KEYDOWN:
		if (_valid)
		{
			DWORD v = MINLONG;
			switch (wParam)
			{
			case VK_DOWN:
				v = _vCaret + (_BytesPerLine << 1);
				break;
			case VK_UP:
				v = _vCaret - (_BytesPerLine << 1);
				break;
			case VK_LEFT:
				v = _vCaret - 1;
				break;
			case VK_RIGHT:
				v = _vCaret + 1;
				break;
			case VK_F5:
				OnUpdate(0, ALL_UPDATED, 0);
				break;
			default:
				if (wParam - '0' < 10)
				{
					wParam -= '0';
				}
				else if (wParam - 'a' < 6)
				{
					wParam -= 'a' - 10;
				}
				else if (wParam - 'A' < 6)
				{
					wParam -= 'A' - 10;
				}
				else goto __m;

				v = TransformCaretPos(_vCaret);

				DWORD ofs = v >> 1, s = (v & 1) << 2;

				CHAR *pc = RtlOffsetToPointer(_pvBuf, ofs), c = *pc;
				
				c = ((UCHAR)wParam << s) | ( c & (0xf0 >> s));

				if (c != *pc)
				{
					if (0 <= GetDbgDoc()->Write((PVOID)(_Address + ofs), c))
					{
						*pc = c;
						Draw4B(ofs, v & 1);
						GetDbgDoc()->UpdateAllViews(this, BYTE_UPDATED, (PVOID)(_Address + ofs));
					}
					else
					{
						return 0;
					}
				}

				v = _vCaret + 1;
			}

			if (v < _maxCaret)
			{
				_vCaret = v;
				if (SetCaretPos()) return 0;
			}
		}
		break;

	case WM_KILLFOCUS:
		DestroyCaret(hwnd);
		_focus = FALSE;
		break;

	case WM_SETFOCUS:
		_focus = TRUE;
		CreateCaret(hwnd);
		break;
	}
__m:
	return ZAddressView::WindowProc(hwnd, uMsg, wParam, lParam);
}

HRESULT ZMemView::QI(REFIID riid, void **ppvObject)
{
	if (riid == __uuidof(ZMemView))
	{
		*ppvObject = static_cast<ZObject*>(this);
		AddRef();
		return S_OK;
	}

	return ZTxtWnd::QI(riid, ppvObject);
}

class ZMemoryFrame : public ZFrameMultiWnd
{
	PCUNICODE_STRING getPosName()
	{
		STATIC_UNICODE_STRING_(sMemory);
		return &sMemory;
	}

	virtual BOOL CreateClient(HWND hwnd, int nWidth, int nHeight, PVOID lpCreateParams)
	{
		if (hwnd = ZAddressCreateClient(hwnd, nWidth, nHeight, ZMemView::createObject, (ZDocument*)lpCreateParams))
		{
			_hwndView = hwnd;
			return TRUE;
		}
		return FALSE;
	}
};

void CreateMemoryWindow(ZDbgDoc* pDocument)
{
	if (ZMemoryFrame* p = new ZMemoryFrame)
	{
		WCHAR sz[32];
		swprintf(sz, L"%x %wZ", pDocument->getId(), &Memory);
		p->Create(WS_EX_TOOLWINDOW, sz,
			WS_VISIBLE|WS_POPUP|WS_CAPTION|WS_THICKFRAME|WS_SYSMENU|WS_CLIPCHILDREN|WS_CLIPSIBLINGS, 
			200, 200, 360, 320, ZGLOBALS::getMainHWND(), 0, pDocument);

		p->Release();
	}
}

_NT_END