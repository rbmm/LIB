#include "StdAfx.h"

_NT_BEGIN

#include "regview.h"
#include "DbgDoc.h"
#pragma warning(disable : 4477)
#ifdef _WIN64
#define N_REG 41
#define M_REG 12
#else
#define N_REG 25
#define M_REG 8
#endif

#ifdef _WIN64
#define _RAX_RBX L"RAX %p RBX %p"
#define _RCX_RDX L"RCX %p RDX %p"
#define _RDI_RSI L"RDI %p RSI %p"
#define _R8_R9   L"R8  %p R9  %p"
#define _R10_R11 L"R10 %p R11 %p"
#define _R12_R13 L"R12 %p R13 %p"
#define _R14_R15 L"R14 %p R15 %p"
#define _RBP_RSP L"RBP %p RSP %p"
#define _RIP_FLG L"RIP %p FLG         %08X"
#define _DR6_DR7 L"DR6             %04X DR7         %08X"
#else
#define _EAX_EBX L"EAX %p EBX %p"
#define _ECX_EDX L"ECX %p EDX %p"
#define _EDI_ESI L"EDI %p ESI %p"
#define _EBP_ESP L"EBP %p ESP %p"
#define _EIP_FLG L"EIP %p FLG %p"
#define _DR6_DR7 L"DR6     %04X DR7 %p"
#endif
#define _DR0_DR1 L"DR0 %p DR1 %p"
#define _DR2_DR3 L"DR2 %p DR3 %p"

STATIC_UNICODE_STRING_(RegPos);

ZRegView::ZRegView()
{
	_vCaret = 0, _caret = 0, _focus = FALSE, _lPressed = FALSE, _bDisabled = FALSE, _bTempHide = FALSE;
}

void ZRegView::TempHide(BOOL bShow)
{
	HWND hwnd = GetParent(ZWnd::getHWND());

	if (bShow)
	{
		if (_bTempHide)
		{
			_bTempHide = FALSE;
			ShowWindow(hwnd, SW_SHOW);
		}
	}
	else
	{
		if (IsWindowVisible(hwnd))
		{
			_bTempHide = TRUE;
			ShowWindow(hwnd, SW_HIDE);
		}
	}
}

void ZRegView::OnNewFont(HFONT /*hFont*/)
{
	SIZE s;
	ZGLOBALS::getFont()->getSIZE(&s);

	RECT rc;
	HWND hwnd = getHWND();
	GetWindowRect(hwnd, &rc);
	rc.right = rc.left + s.cx * N_REG + 2;
	rc.bottom = rc.top + s.cy * M_REG + 2;

	AdjustWindowRectEx(&rc, WS_CAPTION|WS_SYSMENU, FALSE, WS_EX_TOOLWINDOW);
	MoveWindow(GetParent(hwnd), rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
	InvalidateRect(hwnd, 0, TRUE);
}

void ZRegView::HideCaret(HWND hwnd)
{
	if (0 < _caret) 
	{
		_caret = -1;
		::HideCaret(hwnd);
	}
}

void ZRegView::ShowCaret(HWND hwnd)
{
	if (0 > _caret)
	{
		::ShowCaret(hwnd);
		_caret = +1;
	}
}

void ZRegView::DestroyCaret(HWND hwnd)
{
	if (_caret)
	{
		HideCaret(hwnd);
		::DestroyCaret();
		_caret = 0;
	}
}

void ZRegView::CreateCaret(HWND hwnd)
{
	if (_focus && !_caret)
	{
		SIZE v;
		ZGLOBALS::getFont()->getSIZE(&v);
		if (::CreateCaret(hwnd, 0, 1, v.cy - 6))
		{
			_caret = +1;
			SetCaretPos();
			::ShowCaret(hwnd);
		}
	}
}

BOOL ZRegView::SetCaretPos()
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

BOOL ZRegView::SetCaretPos(DWORD vCaret)
{
	SIZE v;
	ZGLOBALS::getFont()->getSIZE(&v);

	int i = vCaret & (sizeof(PVOID)*4 - 1);

	return ::SetCaretPos(1 + (i + (i < sizeof(PVOID)*2 ? 4 : 9)) * v.cx, 4 + (vCaret >> (3 + sizeof(PVOID)/4)) * v.cy);
}

void ZRegView::PtrFromPoint(int x, int y)
{
	SIZE v;
	ZGLOBALS::getFont()->getSIZE(&v);

	DWORD i = (y - 1) / v.cy;

	if (M_REG - 1 < i)
	{
		return ;
	}

	WCHAR buf[sizeof(PVOID)*2 + 1];
	PCWSTR sz = buf;
	int u = -1;
	DWORD j = (x - 1) / v.cx;

	if (j < 3)
	{
		j = 0;
		switch (i)
		{
#ifdef _WIN64
		case 0: sz = L"RAX";_ptr = Rax; break;
		case 1: sz = L"RCX";_ptr = Rcx; break;
		case 2: sz = L"RDI";_ptr = Rdi; break;
		case 3: sz = L"R8"; _ptr = R8; break;
		case 4: sz = L"R10";_ptr = R10; break;
		case 5: sz = L"R12";_ptr = R12; break;
		case 6: sz = L"R14";_ptr = R14; break;
		case 7: sz = L"RBP";_ptr = Rbp; break;
		case 8: sz = L"RIP";_ptr = Rip; break;
		case 9: sz = L"DR0";_ptr = Dr0; break;
		case 10: sz = L"DR2";_ptr = Dr2; break;
#else
		case 0: sz = L"EAX";_ptr = Eax; break;
		case 1: sz = L"ECX";_ptr = Ecx; break;
		case 2: sz = L"EDI";_ptr = Edi; break;
		case 3: sz = L"EBP";_ptr = Ebp; break;
		case 4: sz = L"EIP";_ptr = Eip; break;
		case 5: sz = L"DR0";_ptr = Dr0; break;
		case 6: sz = L"DR2";_ptr = Dr2; break;
#endif
		case M_REG - 1: return;
		}
	}
	else if (j < 4)
	{
		return ;
	}
	else if (j < 4 + sizeof(PVOID)*2)
	{
		u = j - 4, j = 4;
		switch (i)
		{
#ifdef _WIN64
		case 0: swprintf(buf, L"%p", (void*)Rax);_ptr = Rax; break;
		case 1: swprintf(buf, L"%p", (void*)Rcx);_ptr = Rcx; break;
		case 2: swprintf(buf, L"%p", (void*)Rdi);_ptr = Rdi; break;
		case 3: swprintf(buf, L"%p", (void*)R8); _ptr = R8; break;
		case 4: swprintf(buf, L"%p", (void*)R10);_ptr = R10; break;
		case 5: swprintf(buf, L"%p", (void*)R12);_ptr = R12; break;
		case 6: swprintf(buf, L"%p", (void*)R14);_ptr = R14; break;
		case 7: swprintf(buf, L"%p", (void*)Rbp);_ptr = Rbp; break;
		case 8: swprintf(buf, L"%p", (void*)Rip);_ptr = Rip; break;
		case 9: swprintf(buf, L"%p", (void*)Dr0);_ptr = Dr0; break;
		case 10: swprintf(buf, L"%p", (void*)Dr2);_ptr = Dr2; break;
#else
		case 0: swprintf(buf, L"%p", Eax);_ptr = Eax; break;
		case 1: swprintf(buf, L"%p", Ecx);_ptr = Ecx; break;
		case 2: swprintf(buf, L"%p", Edi);_ptr = Edi; break;
		case 3: swprintf(buf, L"%p", Ebp);_ptr = Ebp; break;
		case 4: swprintf(buf, L"%p", Eip);_ptr = Eip; break;
		case 5: swprintf(buf, L"%p", Dr0);_ptr = Dr0; break;
		case 6: swprintf(buf, L"%p", Dr2);_ptr = Dr2; break;
#endif
		case M_REG - 1: return ;
		}
	}
	else if (j < 5 + sizeof(PVOID)*2)
	{
		return ;
	}
	else if (j < 8 + sizeof(PVOID)*2)
	{
		j = 5 + sizeof(PVOID)*2;
		switch (i)
		{
#ifdef _WIN64
		case 0: sz = L"RBX";_ptr = Rbx; break;
		case 1: sz = L"RDX";_ptr = Rdx; break;
		case 2: sz = L"RSI";_ptr = Rsi; break;
		case 3: sz = L"R9"; _ptr = R9; break;
		case 4: sz = L"R11";_ptr = R11; break;
		case 5: sz = L"R13";_ptr = R13; break;
		case 6: sz = L"R15";_ptr = R15; break;
		case 7: sz = L"RSP";_ptr = Rsp; break;
		case 8: return;
		case 9: sz = L"DR1";_ptr = Dr1; break;
		case 10: sz = L"DR3";_ptr = Dr3; break;
#else
		case 0: sz = L"EBX";_ptr = Ebx; break;
		case 1: sz = L"EDX";_ptr = Edx; break;
		case 2: sz = L"ESI";_ptr = Esi; break;
		case 3: sz = L"ESP";_ptr = Esp; break;
		case 4: return;
		case 5: sz = L"DR1";_ptr = Dr1; break;
		case 6: sz = L"DR3";_ptr = Dr3; break;
#endif
		case M_REG - 1: return;
		}
	}
	else if (j < 9 + sizeof(PVOID)*2)
	{
		return ;
	}
	else
	{
		u = j - 9, j = 9 + sizeof(PVOID)*2;
		switch (i)
		{
#ifdef _WIN64
		case 0: swprintf(buf, L"%p", (void*)Rbx);_ptr = Rbx; break;
		case 1: swprintf(buf, L"%p", (void*)Rdx);_ptr = Rdx; break;
		case 2: swprintf(buf, L"%p", (void*)Rsi);_ptr = Rsi; break;
		case 3: swprintf(buf, L"%p", (void*)R9); _ptr = R9; break;
		case 4: swprintf(buf, L"%p", (void*)R11);_ptr = R11; break;
		case 5: swprintf(buf, L"%p", (void*)R13);_ptr = R13; break;
		case 6: swprintf(buf, L"%p", (void*)R15);_ptr = R15; break;
		case 7: swprintf(buf, L"%p", (void*)Rsp);_ptr = Rsp; break;
		case 9: swprintf(buf, L"%p", (void*)Dr1);_ptr = Dr1; break;
		case 10: swprintf(buf, L"%p", (void*)Dr3);_ptr = Dr3; break;
		case 11:
		case 8:
			if (u < 3*sizeof(PVOID))
			{
				return;
			}
			sz = 0; 
			break;
#else
		case 0: swprintf(buf, L"%p", (void*)Ebx);_ptr = Ebx; break;
		case 1: swprintf(buf, L"%p", (void*)Edx);_ptr = Edx; break;
		case 2: swprintf(buf, L"%p", (void*)Esi);_ptr = Esi; break;
		case 3: swprintf(buf, L"%p", (void*)Esp);_ptr = Esp; break;
		case 5: swprintf(buf, L"%p", (void*)Dr1);_ptr = Dr1; break;
		case 6: swprintf(buf, L"%p", (void*)Dr3);_ptr = Dr3; break;
		case 4:
		case 7: sz = 0; break;
#endif			
		}
	}

	if (0 <= u)
	{
		SetCaretPos(_vCaret = u + i * sizeof(PVOID)*4);
	}

	if (sz)
	{
		_dxHotspot = x - j * v.cx - 1, _dyHotspot = y - i * v.cy - 1, _lPressed = TRUE;

		if (sz != buf)
		{
			SetText(sz);
		}
	}
}

LRESULT ZRegView::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	BOOLEAN lPressed = _lPressed;

	if (lPressed && uMsg - WM_MOUSEFIRST <= WM_MOUSELAST - WM_MOUSEFIRST)
	{
		_lPressed = FALSE;
	}

	DWORD v, y;
	SIZE s;
	PULONG_PTR pReg;
	PAINTSTRUCT ps;
	WCHAR sz[N_REG+1];
	ZFont* font;
	HGDIOBJ h;

	switch (uMsg)
	{
	case WM_LBUTTONUP:
__f:
		if (_capture)
		{
			_capture = FALSE;
			EndDrag();
			ShowCaret(hwnd);
		}
		break;

	case WM_LBUTTONDOWN:
		if (!_bDisabled) PtrFromPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;

	case WM_MOUSEMOVE:
		if (_capture)
		{
			DragMove(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		}
		else if (lPressed && (wParam & MK_LBUTTON))
		{
			HideCaret(hwnd);

			if (BeginDrag(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), _dxHotspot, _dyHotspot, sizeof(PVOID)*2))
			{
				_capture = TRUE;
				return 0;
			}

			ShowCaret(hwnd);
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

	case WM_KEYDOWN:
		if (_bDisabled) return 0;
		v = MINLONG;
		switch (wParam)
		{
		case VK_DOWN:
			v = _vCaret + sizeof(PVOID)*4;
			break;
		case VK_UP:
			v = _vCaret - sizeof(PVOID)*4;
			break;
		case VK_LEFT:
			v = _vCaret - 1;
			break;
		case VK_RIGHT:
			v = _vCaret + 1;
			break;
		case VK_ESCAPE:
			goto __f;

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
			else return 0;

			switch (y = _vCaret >> (2 + sizeof(PVOID)/4 ))
			{
			default: __assume(false);
#ifdef _WIN64
			case 0: pReg = &Rax; break;
			case 1: pReg = &Rbx; break;
			case 2: pReg = &Rcx; break;
			case 3: pReg = &Rdx; break;
			case 4: pReg = &Rdi; break;
			case 5: pReg = &Rsi; break;
			case 6: pReg = &R8; break;
			case 7: pReg = &R9; break;
			case 8: pReg = &R10; break;
			case 9: pReg = &R11; break;
			case 10: pReg = &R12; break;
			case 11: pReg = &R13; break;
			case 12: pReg = &R14; break;
			case 13: pReg = &R15; break;
			case 14: pReg = &Rbp; break;
			case 15: pReg = &Rsp; break;
			case 16: pReg = &Rip; break;
			case 17: pReg = (PULONG_PTR)&EFlags; break;
			case 18: pReg = &Dr0; break;
			case 19: pReg = &Dr1; break;
			case 20: pReg = &Dr2; break;
			case 21: pReg = &Dr3; break;
			case 22: return 0;
			case 23: pReg = &Dr7; break;
#else
			case 0: pReg = &Eax; break;
			case 1: pReg = &Ebx; break;
			case 2: pReg = &Ecx; break;
			case 3: pReg = &Edx; break;
			case 4: pReg = &Edi; break;
			case 5: pReg = &Esi; break;
			case 6: pReg = &Ebp; break;
			case 7: pReg = &Esp; break;
			case 8: pReg = &Eip; break;
			case 9: pReg = &EFlags; break;
			case 10: pReg = &Dr0; break;
			case 11: pReg = &Dr1; break;
			case 12: pReg = &Dr2; break;
			case 13: pReg = &Dr3; break;
			case 14: return 0;
			case 15: pReg = &Dr7; break;
#endif
			}
			v = (15 - _vCaret & (sizeof(PVOID)*2-1)) << 2;

			*pReg = ((lParam = *pReg) & ~((ULONG_PTR)0xf << v)) | (wParam << v);

			if (((ULONG_PTR)lParam != *pReg) && (ps.hdc = GetDC(hwnd)))
			{
				swprintf(sz, L"%X", (ULONG)wParam);
				font = ZGLOBALS::getFont();
				font->getSIZE(&s);
				h = SelectObject(ps.hdc, font->getFont());
				SetTextColor(ps.hdc, RGB(255,0,0));
				::HideCaret(hwnd);
				TextOut(ps.hdc, 1 + ((_vCaret & (sizeof(PVOID)*2-1)) + (y & 1 ? 9 + sizeof(PVOID)*2 : 4)) * s.cx, 1 + (y >> 1) * s.cy, sz, 1);
				::ShowCaret(hwnd);
				SelectObject(ps.hdc, h);
				ReleaseDC(hwnd, ps.hdc);
			}

			v = _vCaret + 1;
			wParam = VK_RIGHT;
		}

		if (v < (sizeof(PVOID) << 2)*M_REG)
		{
#ifdef _WIN64
			if (v - 34*sizeof(PVOID) < 8)
			{
				switch (wParam)
				{
				case VK_DOWN:
					v += sizeof(PVOID)*4;
					break;
				case VK_RIGHT:
					v = 34*sizeof(PVOID) + 8;
					break;
				case VK_UP:
					v -= sizeof(PVOID)*4;
					break;
				case VK_LEFT:
					v = 34*sizeof(PVOID) - 1;
					break;
				}
			}

			if (v - 44*sizeof(PVOID) < 24)
			{
				switch (wParam)
				{
				case VK_UP:
				case VK_DOWN:
					return 0;
				case VK_RIGHT:
					v = 44*sizeof(PVOID) + 24;
					break;
				case VK_LEFT:
					v = 44*sizeof(PVOID) - 1;
					break;
				}
			}
#else
			if (v - 28*sizeof(PVOID) < 4)
			{
				switch (wParam)
				{
				case VK_UP:
				case VK_DOWN:
					return 0;
				case VK_RIGHT:
					v = 28*sizeof(PVOID) + 4;
					break;
				case VK_LEFT:
					v = 28*sizeof(PVOID) - 1;
					break;
				}
			}
#endif

			_vCaret = v;
			SetCaretPos();
		}
		break;

	case WM_PAINT:
		if (BeginPaint(hwnd, &ps))
		{
			HideCaret(hwnd);
			FillRect(ps.hdc, &ps.rcPaint, (HBRUSH)GetStockObject(WHITE_BRUSH));
			font = ZGLOBALS::getFont();
			font->getSIZE(&s);
			h = SelectObject(ps.hdc, font->getFont());

			y = 1;

#ifdef _WIN64
			TextOut(ps.hdc, 1,         1, sz, swprintf(sz, _RAX_RBX, Rax, Rbx));
			TextOut(ps.hdc, 1, y += s.cy, sz, swprintf(sz, _RCX_RDX, Rcx, Rdx));
			TextOut(ps.hdc, 1, y += s.cy, sz, swprintf(sz, _RDI_RSI, Rdi, Rsi));
			TextOut(ps.hdc, 1, y += s.cy, sz, swprintf(sz, _R8_R9, R8, R9));
			TextOut(ps.hdc, 1, y += s.cy, sz, swprintf(sz, _R10_R11, R10, R11));
			TextOut(ps.hdc, 1, y += s.cy, sz, swprintf(sz, _R12_R13, R12, R13));
			TextOut(ps.hdc, 1, y += s.cy, sz, swprintf(sz, _R14_R15, R14, R15));
			TextOut(ps.hdc, 1, y += s.cy, sz, swprintf(sz, _RBP_RSP, Rbp, Rsp));
			TextOut(ps.hdc, 1, y += s.cy, sz, swprintf(sz, _RIP_FLG, Rip, EFlags));
			TextOut(ps.hdc, 1, y += s.cy, sz, swprintf(sz, _DR0_DR1, Dr0, Dr1));
			TextOut(ps.hdc, 1, y += s.cy, sz, swprintf(sz, _DR2_DR3, Dr2, Dr3));
			TextOut(ps.hdc, 1, y += s.cy, sz, swprintf(sz, _DR6_DR7, Dr6&0xffff, Dr7));
#else
			TextOut(ps.hdc, 1,         1, sz, swprintf(sz, _EAX_EBX, Eax, Ebx));
			TextOut(ps.hdc, 1, y += s.cy, sz, swprintf(sz, _ECX_EDX, Ecx, Edx));
			TextOut(ps.hdc, 1, y += s.cy, sz, swprintf(sz, _EDI_ESI, Edi, Esi));
			TextOut(ps.hdc, 1, y += s.cy, sz, swprintf(sz, _EBP_ESP, Ebp, Esp));
			TextOut(ps.hdc, 1, y += s.cy, sz, swprintf(sz, _EIP_FLG, Eip, EFlags));
			TextOut(ps.hdc, 1, y += s.cy, sz, swprintf(sz, _DR0_DR1, Dr0, Dr1));
			TextOut(ps.hdc, 1, y += s.cy, sz, swprintf(sz, _DR2_DR3, Dr2, Dr3));
			TextOut(ps.hdc, 1, y += s.cy, sz, swprintf(sz, _DR6_DR7, Dr6&0xffff, Dr7));
#endif

			SelectObject(ps.hdc, h);
			ShowCaret(hwnd);
			EndPaint(hwnd, &ps);
		}
		break;

	case WM_ERASEBKGND:
		return TRUE;

	case WM_SETCURSOR:
		SetCursor(CCursorCashe::GetCursor(LOWORD(lParam) == HTCLIENT ? CCursorCashe::IBEAM : CCursorCashe::ARROW));
		return TRUE;

	case WM_TIMER:
		if (_bDisabled)
		{
			if (_Ticks < GetTickCount())
			{
	case WM_CLOSE+WM_USER:
				_bDisabled = FALSE;
				ShowWindow(GetParent(hwnd), SW_HIDE);
			}
		}
		if (_M_pDoc)
		{
			_M_pDoc->OnIdle();
		}
		return IDCANCEL;

	case WM_CREATE:
		SetTimer(hwnd, 1, 1000, 0);
		break;

	case WM_MOUSEACTIVATE:
		return MA_ACTIVATE;

	case WM_DESTROY:
		KillTimer(hwnd, 1);
		GetWindowRect(hwnd, &ps.rcPaint);
		DWORD xy = MAKELONG(ps.rcPaint.left, ps.rcPaint.top);
		ZGLOBALS::getRegistry()->SetValue(&RegPos, REG_DWORD, &xy, sizeof(xy));
		break;
	}
	return ZWnd::WindowProc(hwnd, uMsg, wParam, lParam);
}

void ZRegView::SetDisabled() 
{ 
	_bDisabled = TRUE; _Ticks = GetTickCount() + (_M_pDoc && _M_pDoc->IsRemoteDebugger() ? 2000 : 1000); 
}

void DrawDiff(HDC hdc, int x, int y, int cx, int cy, PWSTR sz, PWSTR wz, DWORD n)
{
	int len = 0;
	RECT rc = { 0, y, n*cx, y + cy };
	SetTextColor(hdc, RGB(0,0,0));
	ExtTextOut(hdc, 0, y, ETO_OPAQUE, &rc, wz, n - 1, 0);
	SetTextColor(hdc, RGB(255,0,0));
	do 
	{
		if (*sz == *wz)
		{
			if (len)
			{
				rc.right = x;
				ExtTextOut(hdc, rc.left, y, ETO_OPAQUE, &rc, wz - len, len, 0);
				len = 0;
			}

		}
		else
		{
			if (!len++)
			{
				rc.left = x;
			}
		}
	} while (x += cx, sz++, wz++, --n);
}

void ZRegView::SetContext(CONTEXT* ctx) 
{
	HWND hwnd = getHWND();
	BOOL bVisible = IsWindowVisible(hwnd);

	if (bVisible)
	{
		if (HDC hdc = GetDC(hwnd))
		{
			HideCaret(hwnd);
			SIZE s;
			ZFont* font = ZGLOBALS::getFont();
			font->getSIZE(&s);
			HGDIOBJ h = SelectObject(hdc, font->getFont());

			int y = 1;
			WCHAR sz[64], wz[64];

#ifdef _WIN64
			swprintf(sz, _RAX_RBX, Rax, Rbx);
			DrawDiff(hdc, 1, 1, s.cx, s.cy, sz, wz, 1 + swprintf(wz, _RAX_RBX, ctx->Rax, ctx->Rbx));
			swprintf(sz, _RCX_RDX, Rcx, Rdx);
			DrawDiff(hdc, 1, y += s.cy, s.cx, s.cy, sz, wz, 1 + swprintf(wz, _RCX_RDX, ctx->Rcx, ctx->Rdx));
			swprintf(sz, _RDI_RSI, Rdi, Rsi);
			DrawDiff(hdc, 1, y += s.cy, s.cx, s.cy, sz, wz, 1 + swprintf(wz, _RDI_RSI, ctx->Rdi, ctx->Rsi));
			swprintf(sz, _R8_R9, R8, R9);
			DrawDiff(hdc, 1, y += s.cy, s.cx, s.cy, sz, wz, 1 + swprintf(wz, _R8_R9, ctx->R8, ctx->R9));
			swprintf(sz, _R10_R11, R10, R11);
			DrawDiff(hdc, 1, y += s.cy, s.cx, s.cy, sz, wz, 1 + swprintf(wz, _R10_R11, ctx->R10, ctx->R11));
			swprintf(sz, _R12_R13, R12, R13);
			DrawDiff(hdc, 1, y += s.cy, s.cx, s.cy, sz, wz, 1 + swprintf(wz, _R12_R13, ctx->R12, ctx->R13));
			swprintf(sz, _R14_R15, R14, R15);
			DrawDiff(hdc, 1, y += s.cy, s.cx, s.cy, sz, wz, 1 + swprintf(wz, _R14_R15, ctx->R14, ctx->R15));
			swprintf(sz, _RBP_RSP, Rbp, Rsp);
			DrawDiff(hdc, 1, y += s.cy, s.cx, s.cy, sz, wz, 1 + swprintf(wz, _RBP_RSP, ctx->Rbp, ctx->Rsp));
			swprintf(sz, _RIP_FLG, Rip, EFlags);
			DrawDiff(hdc, 1, y += s.cy, s.cx, s.cy, sz, wz, 1 + swprintf(wz, _RIP_FLG, ctx->Rip, ctx->EFlags));
			swprintf(sz, _DR0_DR1, Dr0, Dr1);
			DrawDiff(hdc, 1, y += s.cy, s.cx, s.cy, sz, wz, 1 + swprintf(wz, _DR0_DR1, ctx->Dr0, ctx->Dr1));
			swprintf(sz, _DR2_DR3, Dr2, Dr3);
			DrawDiff(hdc, 1, y += s.cy, s.cx, s.cy, sz, wz, 1 + swprintf(wz, _DR2_DR3, ctx->Dr2, ctx->Dr3));
			swprintf(sz, _DR6_DR7, Dr6&0xffff, Dr7);
			DrawDiff(hdc, 1, y += s.cy, s.cx, s.cy, sz, wz, 1 + swprintf(wz, _DR6_DR7, ctx->Dr6&0xffff, ctx->Dr7));
#else
			swprintf(sz, _EAX_EBX, Eax, Ebx);
			DrawDiff(hdc, 1, 1, s.cx, s.cy, sz, wz, 1 + swprintf(wz, _EAX_EBX, ctx->Eax, ctx->Ebx));
			swprintf(sz, _ECX_EDX, Ecx, Edx);
			DrawDiff(hdc, 1, y += s.cy, s.cx, s.cy, sz, wz, 1 + swprintf(wz, _ECX_EDX, ctx->Ecx, ctx->Edx));
			swprintf(sz, _EDI_ESI, Edi, Esi);
			DrawDiff(hdc, 1, y += s.cy, s.cx, s.cy, sz, wz, 1 + swprintf(wz, _EDI_ESI, ctx->Edi, ctx->Esi));
			swprintf(sz, _EBP_ESP, Ebp, Esp);
			DrawDiff(hdc, 1, y += s.cy, s.cx, s.cy, sz, wz, 1 + swprintf(wz, _EBP_ESP, ctx->Ebp, ctx->Esp));
			swprintf(sz, _EIP_FLG, Eip, EFlags);
			DrawDiff(hdc, 1, y += s.cy, s.cx, s.cy, sz, wz, 1 + swprintf(wz, _EIP_FLG, ctx->Eip, ctx->EFlags));
			swprintf(sz, _DR0_DR1, Dr0, Dr1);
			DrawDiff(hdc, 1, y += s.cy, s.cx, s.cy, sz, wz, 1 + swprintf(wz, _DR0_DR1, ctx->Dr0, ctx->Dr1));
			swprintf(sz, _DR2_DR3, Dr2, Dr3);
			DrawDiff(hdc, 1, y += s.cy, s.cx, s.cy, sz, wz, 1 + swprintf(wz, _DR2_DR3, ctx->Dr2, ctx->Dr3));
			swprintf(sz, _DR6_DR7, Dr6&0xffff, Dr7);
			DrawDiff(hdc, 1, y += s.cy, s.cx, s.cy, sz, wz, 1 + swprintf(wz, _DR6_DR7, ctx->Dr6&0xffff, ctx->Dr7));
#endif

			SelectObject(hdc, h);
			ShowCaret(hwnd);
			ReleaseDC(hwnd, hdc);
		}
	}
	_bDisabled = FALSE;
	memcpy(static_cast<CONTEXT*>(this), ctx, sizeof(CONTEXT));

	if (!bVisible)
	{
		ShowWindow(GetParent(hwnd), SW_SHOW);
	}
}

void ZRegView::GetPos(PRECT prc)
{
	int x = 64, y = 64;

	KEY_VALUE_PARTIAL_INFORMATION kvpi;

	if (0 <= ZGLOBALS::getRegistry()->GetValue(&RegPos, KeyValuePartialInformation, &kvpi, sizeof(kvpi), &kvpi.TitleIndex))
	{
		x = GET_X_LPARAM(*(DWORD*)&kvpi.Data), y = GET_Y_LPARAM(*(DWORD*)&kvpi.Data);

		if (0 > x || 0 > y || GetSystemMetrics(SM_CXSCREEN) - 16 < x || GetSystemMetrics(SM_CYSCREEN) < y)
		{
			x = 64, y = 64;
		}
	}

	SIZE s;
	ZGLOBALS::getFont()->getSIZE(&s);

	prc->left = x, prc->right = x + s.cx * N_REG + 2;
	prc->top = y, prc->bottom = y + s.cy * M_REG + 2;

	AdjustWindowRectEx(prc, WS_CAPTION|WS_SYSMENU, FALSE, WS_EX_TOOLWINDOW);
}

class ZRegFrame : public ZFrameWnd
{
	virtual BOOL CanClose()
	{
		ShowWindow(getHWND(), SW_HIDE);
		return FALSE;
	}

	virtual HWND CreateView(HWND hWndParent, int nWidth, int nHeight, PVOID lpCreateParams)
	{
		HWND hwnd = 0;

		if (ZRegView* p = new ZRegView)
		{
			if (hwnd = p->Create(0, 0, WS_CHILD|WS_VISIBLE, 0, 0, nWidth, nHeight, hWndParent, 0, 0))
			{
				*(ZRegView**)lpCreateParams = p;
				p->AddRef();
			}
			p->Release();
		}

		return hwnd;
	}
};

HWND createRegView(ZRegView** ppReg, DWORD dwProcessId)
{
	HWND hwnd = 0;
	if (ZRegFrame* p = new ZRegFrame)
	{
		WCHAR sz[32];
		swprintf(sz, L"%x Registry", dwProcessId);
		RECT rc;
		ZRegView::GetPos(&rc);
		hwnd = p->Create(WS_EX_TOOLWINDOW, sz, WS_POPUP|WS_CAPTION|WS_SYSMENU, 
			rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, ZGLOBALS::getMainHWND(), 0, ppReg);
		p->Release();
	}
	return hwnd;
}

_NT_END