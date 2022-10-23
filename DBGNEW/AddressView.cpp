#include "StdAfx.h"

_NT_BEGIN

#include "common.h"

//int IsAddressValid(INT_PTR p);
//BOOL findFirstNotValidVA(INT_PTR p, int d, INT_PTR& q);
BOOL GetValidRange(HANDLE hProcess, INT_PTR Address, INT_PTR& rLo, INT_PTR& rHi)
{
	if ((LONG_PTR)hProcess > MAXUSHORT)
	{
		DbgBreak();
	}

	if (0 > (INT_PTR)Address)
	{
		INT_PTR a[2];
		IO_STATUS_BLOCK iosb;
		if (0 <= ZwDeviceIoControlFile(g_hDrv, 0, 0, 0, &iosb, IOCTL_QueryMemory, &Address, sizeof(Address), a, sizeof(a)))
		{
			rLo = a[0], rHi = a[1];
			return TRUE;
		}
		return FALSE;
	}

	MEMORY_BASIC_INFORMATION mbi;

	if (
		0 > ZwQueryVirtualMemory(hProcess, (void*)Address, MemoryBasicInformation, &mbi, sizeof(mbi), 0) ||
		mbi.State != MEM_COMMIT
		)
	{
		return FALSE;
	}

	PVOID AllocationBase = mbi.AllocationBase;
	INT_PTR Lo = 0, Hi = (INT_PTR)AllocationBase;

	for (;;)
	{
		if (0 > ZwQueryVirtualMemory(hProcess, (PVOID)Hi, MemoryBasicInformation, &mbi, sizeof(mbi), 0))
		{
			return FALSE;
		}

		if (mbi.AllocationBase != AllocationBase)
		{
			break;
		}

		if (!Lo) Lo = Hi;

		if ((mbi.State != MEM_COMMIT) || (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))) 
		{
			if (Hi < Address) Lo = 0; else break;
		}

		Hi = (INT_PTR)mbi.BaseAddress + mbi.RegionSize;
	}

	if (Lo > Address || Address >= Hi)
	{
		return FALSE;
	}

	rLo = Lo, rHi = Hi;

	return TRUE;
}

void InsertStringLB(HWND hwndList, PCWSTR sz)
{
	if (LB_ERR == ListBox_FindStringExact(hwndList, 0, sz))
	{
		ListBox_InsertString(hwndList, 0, sz);
	}
}

void ZAddressView::InsertString(PCWSTR sz)
{
	InsertStringLB(_hwndList, sz);
	SetWindowText(_hwndEdit, sz);
}

ZAddressView::~ZAddressView()
{
	if (_pvBuf)
	{
		delete _pvBuf;
	}
}

ZAddressView::ZAddressView(ZDocument* pDocument) : ZView(pDocument)
{
	_valid = FALSE;
	_hwndEdit = 0, _hwndList = 0;
	_caret = 0;
	_focus = FALSE;
	_lPressed = FALSE;
	_capture = FALSE;
	_pvBuf = 0, _cbBuf = 0;
}

BOOL CALLBACK rn(ZDbgDoc* pDoc, PCSTR name, INT_PTR& res)
{
	if (PCSTR c = strchr(name, '!'))
	{
		ULONG len = (ULONG)strlen(name)+1;
		PSTR sz = (PSTR)alloca(len);
		memcpy(sz, name, len);
		sz[c - name] = '.';
		name = sz;
	}
	if (PVOID Va = pDoc->getVaByName(name))
	{
		res = (INT_PTR)Va;
		return TRUE;
	}
	return FALSE;
}

BOOL CALLBACK rm(ZDbgDoc* pDoc, PVOID Va, PVOID buf, DWORD cb)
{
	return 0 <= SymReadMemory(pDoc, Va, buf, cb);
}

BOOL ZAddressView::GoTo(HWND hwnd, PCWSTR sz)
{
	PCWSTR _sz = sz;
	PSTR cz = (PSTR)alloca(wcslen(sz) + 1), txt = cz;
	WCHAR c;
	do 
	{
		if (MAXUCHAR < (c = *sz++)) return FALSE;
		if (c == ';') c = 0;
		*cz++ = (CHAR)c;
	} while (c);

	CEvalutor64::RESOLVENAME pfn = 0;
	CEvalutor64::READMEM prm = 0;
	ZDbgDoc* pDoc = GetDbgDoc();
	PCONTEXT ctx = 0;

	if (pDoc)
	{
		pfn = (CEvalutor64::RESOLVENAME)rn;
		prm = (CEvalutor64::READMEM)rm;
		ctx = pDoc->getContext();
	}

	CEvalutor64 ev(ctx, prm, pfn, pDoc);
	
	INT_PTR res;
	
	if (ev.Evalute(txt, res))
	{
		if (GoTo(hwnd, res))
		{
			InsertStringLB(_hwndList, _sz);
			return TRUE;
		}
		Invalidate(hwnd);
		return FALSE;
	}

	return FALSE;
}

void ZAddressView::HideCaret(HWND hwnd)
{
	if (0 < _caret) 
	{
		_caret = -1;
		::HideCaret(hwnd);
	}
}

void ZAddressView::ShowCaret(HWND hwnd)
{
	if (0 > _caret)
	{
		::ShowCaret(hwnd);
		_caret = +1;
	}
}

void ZAddressView::DestroyCaret(HWND hwnd)
{
	if (_caret)
	{
		HideCaret(hwnd);
		::DestroyCaret();
		_caret = 0;
	}
}

void ZAddressView::Invalidate(HWND hwnd)
{
	if (_valid)
	{
		_valid = FALSE;
		_BytesPerLine = 1, _cc = 1;

		DestroyCaret(hwnd);

		SCROLLINFO si = { sizeof(SCROLLINFO), SIF_ALL };
		SetScrollInfo(hwnd, SB_VERT, &si, FALSE);
		SetScrollInfo(hwnd, SB_HORZ, &si, FALSE);
		WCHAR sz[64];
		swprintf(sz, L"%x %wZ", GetDbgDoc()->getId(), getRegName());
		SetWindowText(GetParent(hwnd), sz);
		InvalidateRect(hwnd, 0, TRUE);
	}
}

BOOL ZAddressView::FormatLineForDrag(DWORD /*line*/, PSTR /*buf*/, DWORD /*cb*/)
{
	return FALSE;
}

void ZAddressView::PtrFromPoint(int x, int y)
{
	SIZE v;
	POINT nPos;
	GetUnitSize(v);
	GetPos(nPos);

	char buf[256], *sz = buf, *pb;

	DWORD line = y / v.cy;

	if (!FormatLineForDrag(line, buf, RTL_NUMBER_OF(buf)))
	{
		return ;
	}

	DWORD len = (DWORD)strlen(buf), ofs = nPos.x + ((x -= GetIndent()) / v.cx);

	if (ofs >= len) return ;

	sz = pb = buf + ofs;
	char c;

//#define NOT_INRANGE(a, o, b) ((UCHAR)(o - a) > (UCHAR)(b - a))

	static LONG mask[4] = {0x00000000,0x03ff2c00,0x010d027e,0x010d027e};//

	do
	{
		c = *sz;

		if (!_bittest(mask, c))
		{
			break;
		}

	} while (buf < sz--);

	sz++;

	for (;;)
	{
		c = *++pb;

		if (!_bittest(mask, c))
		{
			*pb = 0;
			break;
		}
	}

	if (!*sz)
	{
		return ;
	}

	_dxHotspot = x - (INT_PTR)RtlPointerToOffset(buf + nPos.x, sz) * v.cx, _dyHotspot = y - (line * v.cy), _ptrLen = (UCHAR)(pb - sz);

	CEvalutor64 eval(GetDbgDoc()->getContext(), 0, 0, 0);
	
	INT_PTR res;
	if (eval.Evalute(sz, res))
	{
		_ptr = res, _lPressed = TRUE;
		PWSTR wz = (PWSTR)alloca((strlen(sz)+1)<<1);
		swprintf(wz, L"%S", sz);
		SetText(wz);
	}
}

void ZAddressView::OnMenuItemSelect(HMENU /*hmenu*/, DWORD /*id*/, HWND /*hwnd*/)
{

}

LRESULT ZAddressView::WindowProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	BOOLEAN lPressed = _lPressed;

	if (lPressed && uMsg - WM_MOUSEFIRST <= WM_MOUSELAST - WM_MOUSEFIRST)
	{
		_lPressed = FALSE;
	}

	switch (uMsg)
	{
	case ZDragPtr::WM_DROP:
		if (GoTo(hwnd, lParam))
		{
			PWSTR txt = (PWSTR)wParam;

			if (!txt)
			{
				txt = (PWSTR)alloca(32*sizeof(WCHAR));
				swprintf(txt, L"%p", (void*)lParam);
			}

			InsertStringLB(_hwndList, txt);
			SetWindowText(_hwndEdit, txt);
		}
		return 0;

	case WM_DESTROY:
		if (ZRegistry* p = ZGLOBALS::getRegistry())
		{
			p->SaveWinPos(getRegName(), GetParent(hwnd));
		}
		break;

	case WM_MOUSEMOVE:
		if (_capture)
		{
			DragMove(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		}
		else if (lPressed && (wParam & MK_LBUTTON))
		{
			HideCaret(hwnd);

			if (BeginDrag(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), _dxHotspot, _dyHotspot, _ptrLen))
			{
				_capture = TRUE;
				return 0;
			}

			ShowCaret(hwnd);
		}
		break;

	case WM_LBUTTONUP:
		if (_capture)
		{
			_capture = FALSE;
			EndDrag();
			ShowCaret(hwnd);
		}
		break;

	case WM_LBUTTONDOWN:
		if (_valid)
		{
			int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
			PtrFromPoint(x, y);
			OnLButtonDown(hwnd, x, y);
		}
		break;

	case WM_CONTEXTMENU:
		{
			HMENU hm;
			if (HMENU hmenu = GetContextMenu(hm))
			{
				POINT pt;
				GetCursorPos(&pt);
				OnMenuItemSelect(hm, TrackPopupMenu(hm, TPM_RETURNCMD, pt.x, pt.y, 0, hwnd, 0), hwnd);
				DestroyMenu(hmenu);
			}
		}
		break;

	case WM_NCHITTEST:
		if (_valid)
		{
			POINT pt = {GET_X_LPARAM(lParam)};
			ScreenToClient(hwnd, &pt);
			if (pt.x < GetIndent()-12)
			{
				return HTBORDER;
			}
		}
		break;

	case WM_SETCURSOR:
		SetCursor(CCursorCashe::GetCursor(LOWORD(lParam) == HTCLIENT ? CCursorCashe::IBEAM : CCursorCashe::ARROW));
		return TRUE;

	case WM_COMMAND:

		switch (wParam)
		{
		case MAKEWPARAM(AFX_IDW_PANE_FIRST, CBN_SELCHANGE):
			if (_hwndList)
			{
				int i = ListBox_GetCurSel(_hwndList);
				if (0 < i)
				{
					int len = ListBox_GetTextLen(_hwndList, i);
					if (0 < len)
					{
						PWSTR text = (PWSTR)alloca(++len << 1);
						ListBox_GetText(_hwndList, i, text);
					}
				}
			}
			return 0;

		case IDOK:
			if (_hwndEdit)
			{
				int len = GetWindowTextLength(_hwndEdit);

				if (0 < len)
				{
					PWSTR text = (PWSTR)alloca((1 + len) << 1);

					if (GetWindowText(_hwndEdit, text, 1 + len) == len)
					{
						GoTo(hwnd, text);
					}
				}
			}
			return 0;
		}
		break;

	case WM_USER:
		switch (wParam)
		{
		case AFX_IDW_PANE_FIRST:
			_hwndEdit = ((PCOMBOBOXINFO)lParam)->hwndItem;
			_hwndList = ((PCOMBOBOXINFO)lParam)->hwndList;
			return 0;
		}
		break;

	case WM_PAINT:
		if (!_valid)
		{
			PAINTSTRUCT ps;
			if (BeginPaint(hwnd, &ps))
			{
				FillRect(ps.hdc, &ps.rcPaint, (HBRUSH)(1 + COLOR_MENUBAR));
				EndPaint(hwnd, &ps);
			}
			return 0;
		}
		break;
	}

	return ZTxtWnd::WindowProc(hwnd, uMsg, wParam, lParam);
}

ZWnd* ZAddressView::getWnd()
{
	return this;
}

ZView* ZAddressView::getView()
{
	return this;
}

HWND CreateView(int x, int y, int nWidth, int nHeight, HWND hWndParent, PFNCREATEOBJECT pfnCreateObject, ZDocument* pDocument)
{
	HWND hwnd = 0;

	if (ZWnd* p = pfnCreateObject(pDocument))
	{
		hwnd = p->Create(0, 0, WS_CHILD|WS_VISIBLE|WS_BORDER, x, y, nWidth, nHeight, hWndParent, 0, 0);
		p->Release();
	}

	return hwnd;
}

HWND ZAddressCreateClient(HWND hwnd, int nWidth, int nHeight, PFNCREATEOBJECT pfnCreateObject, ZDocument* pDocument)
{
	static bool s_first = true;
	static int s_nHeight;

	if (s_first)
	{
		if (HWND hwndTmp = CreateWindowEx(0, WC_COMBOBOX, 0, WS_CHILD|CBS_DROPDOWN, 
			0, 0, nWidth, 0, hwnd, 0, 0, 0))
		{
			if (HFONT hFont = ZGLOBALS::get()->Font->getStatusFont())
			{
				SendMessage(hwndTmp, WM_SETFONT, (WPARAM)hFont, 0);
			}

			RECT rc;
			if (GetWindowRect(hwndTmp, &rc))
			{
				s_nHeight = rc.bottom - rc.top + 6;
			}

			DestroyWindow(hwndTmp);
		}
		s_first = false;
	}

	if (s_nHeight <= 6 || nHeight < s_nHeight)
	{
		return 0;
	}

	if (HWND hwndClient = CreateView(-1, s_nHeight, 2 + nWidth, nHeight - s_nHeight + 1, hwnd, pfnCreateObject, pDocument))
	{
		if (ZAddressWnd* p = new ZAddressWnd(hwndClient))
		{
			BOOL fOk = p->ZAddressWnd::Create(0, 0, nWidth, s_nHeight, hwnd) != 0;
			
			p->Release();

			if (fOk)
			{
				return hwndClient;
			}
		}

		DestroyWindow(hwndClient);
	}

	return 0;
}

_NT_END