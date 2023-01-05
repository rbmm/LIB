#include "StdAfx.h"

_NT_BEGIN

#include "ZDbgThread.h"
#include "common.h"

void CopyContext(HANDLE dwThreadId);

#include "../CVINFO/cvinfo.h"
#include "udt.h"

ZType* FindType(PVOID UdtCtx, PCSTR name);

ZMember* Find(ZType* type, PCSTR name)
{
	if (ZMember* member = type->_member)
	{
		do 
		{
			if (!strcmp(member->_name, name))
			{
				return member;
			}
		} while (member = member->_next);
	}

	return 0;
}

ULONG GetKernelStackOffset(ZDbgDoc* pDocument)
{
	if (LONG KernelStackOffset = pDocument->GetKernelStackOffset())
	{
		return 0 > KernelStackOffset ? 0 : KernelStackOffset;
	}

	if (ZType* type = FindType(pDocument->getUdtCtx(), "_KTHREAD"))
	{
		switch (type->_leaf)
		{
		case LF_CLASS:
		case LF_STRUCTURE:
			if (ZMember* member = Find(type, "KernelStack"))
			{
				type = member->_type;
				if (type->_leaf == LF_POINTER && type->_pti == (ZType*)T_VOID)
				{
					return pDocument->SetKernelStackOffset(member->_offset);
				}
			}
			break;
		}
	}

	pDocument->SetKernelStackOffset(-1);
	return 0;
}

//////////////////////////////////////////////////////////////////////////
// ZDbgThread

ZDbgThread::ZDbgThread(DWORD dwThreadId, HANDLE hThread, PVOID lpThreadLocalBase)
{
	_dwThreadId = dwThreadId, _hThread = hThread, _lpThreadLocalBase = lpThreadLocalBase;
	_flags = 0;
	_state = stF5;
	_Dr[0] = 0;
	_Dr[1] = 0;
	_Dr[2] = 0;
	_Dr[3] = 0;
	_pbpVa = 0;
	_Va = 0;
	InitializeListHead(this);
}

ZDbgThread::~ZDbgThread()
{
	if (_hThread)
	{
		NtClose(_hThread);
	}
	RemoveEntryList(this);
}

//////////////////////////////////////////////////////////////////////////
// ZTabFrame
BOOL CreateDbgTH(ZTabFrame** pp, ZDbgDoc* pDoc)
{
	if (ZTabFrame* p = new ZTabFrame(pDoc))
	{
		WCHAR sz[32];
		swprintf(sz, L"%x DbgWnds", pDoc->getId());
		
		if (p->ZFrameWnd::Create(WS_EX_TOOLWINDOW, sz, WS_POPUP|WS_OVERLAPPEDWINDOW|WS_VISIBLE, 
			400, 200, 800, 256, ZGLOBALS::getMainHWND(), 0, 0))
		{
			*pp = p;
			return TRUE;
		}
		p->Release();
	}

	return FALSE;
}

void ZTabFrame::TempHide(BOOL bShow)
{
	HWND hwnd = ZWnd::getHWND();

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

PCUNICODE_STRING ZTabFrame::getPosName()
{
	STATIC_UNICODE_STRING_(DbgTH);
	return &DbgTH;
}

extern PVOID gLocalKernelImageBase;
extern ULONG gLocalKernelImageSize;

void ZTabFrame::ShowStackTrace(ZDbgThread* pThread, bool bKernel)
{
	CONTEXT ctx{};

	NTSTATUS status;

	ZDbgDoc* pDocument = _pDoc;

#ifdef _WIN64
	if (!pDocument->Is64BitProcess() && !pDocument->IsCurrentThread(pThread) && !pDocument->IsRemoteDebugger())
	{
		WOW64_CONTEXT x86_ctx;
		x86_ctx.ContextFlags = WOW64_CONTEXT_CONTROL;

		status = pThread->GetWowCtx(&x86_ctx);
		ctx.Rip = x86_ctx.Eip;
		ctx.Rsp = x86_ctx.Esp;
		ctx.Rbp = x86_ctx.Ebp;
		ctx.SegCs = 0;
	}
	else
#endif
	{
#ifdef _WIN64
		if (!pThread->GetThreadLocalBase())
		{
			goto __0;
		}
#endif
		ctx.ContextFlags = CONTEXT_CONTROL;
		status = pDocument->IsRemoteDebugger() ? pDocument->RemoteGetContext((WORD)pThread->getID(), &ctx) : pThread->GetContext(&ctx);
	}

	if (0 <= status)
	{
#ifdef _WIN64
		if (bKernel && !pDocument->IsRemoteDebugger())
		{
__0:
			if (ULONG KernelStackOffset = GetKernelStackOffset(pDocument))
			{
				ULONG_PTR id = pThread->getID(), Rsp, Rip, KernelStack[64], *pKernelStack = KernelStack;
				IO_STATUS_BLOCK iosb;
				if (0 <= NtDeviceIoControlFile(g_hDrv, 0, 0, 0, &iosb, IOCTL_LookupThreadByThreadId, &id, sizeof(id), 0, 0))
				{
					if (0 <= pDocument->Read((PVOID)(iosb.Information + KernelStackOffset), &Rsp, sizeof(Rsp)) &&
						0 <= pDocument->Read((void**)Rsp - 1, KernelStack, sizeof(KernelStack)))
					{
						int n = _countof(KernelStack);
						do 
						{
							Rip = *pKernelStack++;

							if (Rip - (ULONG_PTR)gLocalKernelImageBase < gLocalKernelImageSize)
							{
								struct LocalCall 
								{
									UCHAR pad[3];
									UCHAR E8;
									LONG ofs;
								} lc;
								
								if (0 <= pDocument->Read((PBYTE)Rip - 5, &lc.E8, 5) && lc.E8 == 0xE8 &&
									((Rip + (LONG_PTR)lc.ofs) - (ULONG_PTR)gLocalKernelImageBase < gLocalKernelImageSize))
								{
									ctx.SegCs = 0x33;
									ctx.Rip = Rip;
									ctx.Rsp = Rsp;
									ctx.P2Home = 1;
									break;
								}
							}
						} while (Rsp += sizeof(ULONG_PTR), --n);
					}
				}
			}
		}
#endif
		if (ctx.Xsp) ShowStackTrace(ctx);
		TabCtrl_SetCurSel(ZTabBar::getHWND(), 1);
		ShowWindow(_hwndST, SW_SHOW);
		ShowWindow(_hwndTH, SW_HIDE);
		_hwndCur = _hwndST;
	}
}

LRESULT ZTabFrame::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LVITEM item;

	switch (uMsg)
	{
	case WM_CLOSE:
		ShowWindow(hwnd, SW_HIDE);
		return 0;

	case WM_NOTIFY:
		
		item.mask = LVIF_PARAM;
		HWND hwndFrom = ((LPNMHDR)lParam)->hwndFrom;

		switch (((LPNMHDR)lParam)->code)
		{
		case TCN_SELCHANGE:
			if (_hwndCur != (hwnd = (HWND)ZTabBar::getCurParam()))
			{
				ShowWindow(hwnd, SW_SHOW);
				ShowWindow(_hwndCur, SW_HIDE);
				_hwndCur = hwnd;
			}
			break;

		case NM_RCLICK:

			item.iItem = ((LPNMITEMACTIVATE)lParam)->iItem;

			switch (((LPNMHDR)lParam)->idFrom)
			{
			case 1:
				if (!(_pDoc->IsRemoteDebugger() || _pDoc->IsDump()))
				{
					item.iSubItem = 0, item.mask = LVIF_PARAM;
					if (ListView_GetItem(hwndFrom, &item))
					{
						if (HMENU hmenu = LoadMenu((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDR_MENU2)))
						{
							POINT pt;
							GetCursorPos(&pt);
							switch (TrackPopupMenu(GetSubMenu(hmenu, 3), TPM_RETURNCMD, pt.x, pt.y, 0, hwnd, 0))
							{
							case ID_3_CONTEXT:
								CopyContext((HANDLE)item.lParam);
								break;
							case ID_3_WINDOWS:
								ShowThreadWindows((DWORD)item.lParam);
								break;
							case ID_3_KERNELSTACK:
								if (ZDbgThread* pThread = _pDoc->getThreadById((DWORD)item.lParam))
								{
									ShowStackTrace(pThread, true);
								}
								break;
							}

							DestroyMenu(hmenu);
						}
					}
				}
				break;
			case 2:
				if (ULONG n = ListView_GetItemCount(hwndFrom))
				{
					if (HMENU hmenu = LoadMenu((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDR_MENU2)))
					{
						POINT pt;
						GetCursorPos(&pt);
						switch (TrackPopupMenu(GetSubMenu(hmenu, 5), TPM_RETURNCMD, pt.x, pt.y, 0, hwnd, 0))
						{
						case ID_5_COPY:
							if (OpenClipboard(hwnd))
							{
								EmptyClipboard();
								enum { maxCCh = 0x1000 };
								if (HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, (1+0x102*n)*sizeof(WCHAR)))
								{
									int iItem = 0;
									item.iSubItem = 2;
									item.cchTextMax = 0x100;
									item.pszText = (PWSTR)GlobalLock(hg);
									do 
									{
										item.pszText += SendMessage(hwndFrom, LVM_GETITEMTEXT, iItem++, (LPARAM)&item);
										*item.pszText++ = '\r', *item.pszText++ = '\n', *item.pszText = 0;

									} while (--n);

									GlobalUnlock(hg);
									if (!SetClipboardData(CF_UNICODETEXT, hg)) GlobalFree(hg);
								}
								CloseClipboard();
							}
							break;
						}

						DestroyMenu(hmenu);
					}
				}
				break;
			}

			break;
		
		case NM_DBLCLK:
			
			item.iItem = ((LPNMITEMACTIVATE)lParam)->iItem;
			item.iSubItem = 0, item.mask = LVIF_PARAM;

			if (ListView_GetItem(hwndFrom, &item))
			{
				switch(((LPNMHDR)lParam)->idFrom)
				{
				case 1:
					if (ZDbgThread* pThread = _pDoc->getThreadById((DWORD)item.lParam))
					{
						ShowStackTrace(pThread, false);
					}
					break;
				case 2:
					ZDbgDoc* pDoc = _pDoc;
					if (pDoc->IsDump())
					{
						pDoc->ShowFrameContext(item.iItem);
					}
					pDoc->GoTo((PVOID)item.lParam);
					pDoc->ScrollAsmUp();
					break;
				}
			}
			break;
		}
		break;
	}
	return ZFrameMultiWnd::WindowProc(hwnd, uMsg, wParam, lParam);
}

ULONG ZTabFrame::GetCurrentThreadId()
{
	LVITEM item{LVIF_PARAM};
	if (0 <= (item.iItem = ListView_GetSelectionMark(_hwndTH)) && ListView_GetItem(_hwndTH, &item))
	{
		return (ULONG)item.lParam;
	}
	return 0;
}

BOOL ZTabFrame::CreateClient(HWND hWndParent, int nWidth, int nHeight, PVOID /*lpCreateParams*/)
{
	_hwndST = 0, _hwndTH = 0; 
	if (HWND hwnd = ZTabBar::Create(hWndParent, 0, 0, nWidth))
	{
		LV_COLUMN lvclmn = { LVCF_TEXT | LVCF_WIDTH };
		TCITEM item = { TCIF_PARAM|TCIF_TEXT, 0, 0, const_cast<PWSTR>(L"Threads") };
		HFONT hFont = ZGLOBALS::getFont()->getStatusFont();
		RECT rc = {};

		SIZE size;
		HDC hdc = GetDC(hwnd);
		HGDIOBJ o = SelectObject(hdc, hFont);

		GetTextExtentPoint32(hdc, L"W", 1, &size);

		if (_pDoc->IsDump())
		{
			goto __1;
		}

		TabCtrl_InsertItem(hwnd, MAXLONG, &item);
		TabCtrl_AdjustRect(hwnd, FALSE, &rc);
		MoveWindow(hwnd, 0, 0, nWidth, rc.top, FALSE);

		if (item.lParam = (LPARAM)CreateWindowExW(0, WC_LISTVIEW, 0, 
			WS_VISIBLE|WS_CHILD|LVS_REPORT|LVS_EDITLABELS|LVS_NOSORTHEADER|LVS_SHOWSELALWAYS|LVS_SINGLESEL|WS_HSCROLL|WS_VSCROLL,
			0, rc.top, nWidth, nHeight -= rc.top, hWndParent, (HMENU)1, 0, 0))
		{
			TabCtrl_SetItem(hwnd, 0, &item);

			ListView_SetExtendedListViewStyle((HWND)item.lParam, 
				LVS_EX_INFOTIP|LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP|LVS_EX_DOUBLEBUFFER);

			_hwndTH = (HWND)item.lParam, _hwndCur = (HWND)item.lParam;

			if (hFont)
			{
				SendMessage((HWND)item.lParam, WM_SETFONT, (WPARAM)hFont, 0);
			}

#ifdef _WIN64
#define STACK_REG L"Rsp"
#define ADDR_W 18
#else
#define STACK_REG L"Ebp"
#define ADDR_W 10
#endif

			lvclmn.pszText = const_cast<PWSTR>(L" Teb "), lvclmn.cx = ADDR_W * size.cx;
			ListView_InsertColumn((HWND)item.lParam, lvclmn.iSubItem = 0, &lvclmn);
			lvclmn.pszText = const_cast<PWSTR>(L" ID"), lvclmn.cx = 6 * size.cx;
			ListView_InsertColumn((HWND)item.lParam, lvclmn.iSubItem = 1, &lvclmn);
			lvclmn.pszText = const_cast<PWSTR>(L" StartAddress "), lvclmn.cx = (ADDR_W+2) * size.cx;
			ListView_InsertColumn((HWND)item.lParam, lvclmn.iSubItem = 2, &lvclmn);
		}

__1:		
		if (item.lParam = (LPARAM)CreateWindowExW(0, WC_LISTVIEW, 0, 
			WS_CHILD|LVS_REPORT|LVS_EDITLABELS|LVS_NOSORTHEADER|LVS_SHOWSELALWAYS|LVS_SINGLESEL|WS_HSCROLL|WS_VSCROLL,
			0, rc.top, nWidth, nHeight, hWndParent, (HMENU)2, 0, 0))
		{
			item.pszText = const_cast<PWSTR>(L"Stack");
			if (!TabCtrl_InsertItem(hwnd, MAXLONG, &item))
			{
				ShowWindow((HWND)item.lParam, SW_SHOW);
			}

			ListView_SetExtendedListViewStyle((HWND)item.lParam, 
				LVS_EX_INFOTIP|LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP|LVS_EX_DOUBLEBUFFER);

			if (hFont)
			{
				SendMessage((HWND)item.lParam, WM_SETFONT, (WPARAM)hFont, 0);
			}

			lvclmn.pszText = const_cast<PWSTR>(STACK_REG), lvclmn.cx = ADDR_W * size.cx;
			ListView_InsertColumn((HWND)item.lParam, lvclmn.iSubItem = 0, &lvclmn);
			lvclmn.pszText = const_cast<PWSTR>(L"RetAddr"), lvclmn.cx = ADDR_W * size.cx;
			ListView_InsertColumn((HWND)item.lParam, lvclmn.iSubItem = 1, &lvclmn);
			lvclmn.pszText = const_cast<PWSTR>(L"FuncName"), lvclmn.cx = 64 * size.cx;
			ListView_InsertColumn((HWND)item.lParam, lvclmn.iSubItem = 2, &lvclmn);

			_hwndST = (HWND)item.lParam;
		}

		_Stack = 0;

		SelectObject(hdc, o);
		ReleaseDC(hwnd, hdc);

		return TRUE;
	}

	return FALSE;
}

void ZTabFrame::AddThread(DWORD dwThreadId, PVOID lpThreadLocalBase, PVOID lpStartAddress)
{
	WCHAR sz[32];
	swprintf(sz, L"%p", lpThreadLocalBase);

	LVITEM item;
	item.mask = LVIF_PARAM|LVIF_TEXT;
	item.lParam = dwThreadId;
	item.iSubItem = 0;
	item.pszText = sz;
	item.iItem = MAXLONG;
	item.mask = LVIF_PARAM|LVIF_TEXT;

	item.iItem = ListView_InsertItem(_hwndTH, &item);

	item.mask = LVIF_TEXT;

	swprintf(sz, L"%x", dwThreadId);
	item.iSubItem++;
	ListView_SetItem(_hwndTH, &item);

	swprintf(sz, L"%p", lpStartAddress);
	item.iSubItem++;
	ListView_SetItem(_hwndTH, &item);
}

void ZTabFrame::DelThread(DWORD dwThreadId)
{
	LVFINDINFO fi = { LVFI_PARAM, 0, dwThreadId };
	int i = ListView_FindItem(_hwndTH, 0, &fi);
	if (0 <= i)
	{
		ListView_DeleteItem(_hwndTH, i);
	}
}

ULONG_PTR GetFrame(CONTEXT& ctx, ZDbgDoc* pDoc)
{
#ifdef _WIN64
	return (ctx.SegCs == 0x33 || (pDoc->IsDump() && pDoc->Is64BitProcess())) ? ctx.Rsp : ctx.Rbp;
#else
	return ctx.Ebp;
#endif
}

void ZTabFrame::ShowStackTrace(CONTEXT& ctx)
{
	ZDbgDoc* pDoc = _pDoc;

	ULONG_PTR Stack = GetFrame(ctx, pDoc);

	if (Stack == _Stack)
	{
		return ;
	}

	_Stack = Stack;

	SendMessage(_hwndST, WM_SETREDRAW, FALSE, 0);

	ListView_DeleteAllItems(_hwndST);

	PVOID Va = (PVOID)ctx.Xip;
	WCHAR wz[2048];
	LVITEM item;
	item.pszText = wz;

	while (pDoc->DoUnwind(ctx))
	{
		char sz[256];
		PCSTR Name = 0;
		INT_PTR NameVa = 0;
		int d = 0;
		PCWSTR szDllName = L"";

		if (ZDll* pDll = pDoc->getDllByVaNoRef(Va))
		{
			szDllName = pDll->name();
			if (PCSTR name = pDll->getNameByVa2(Va, &NameVa))
			{
				if (IS_INTRESOURCE(name))
				{
					char oname[16];
					sprintf(oname, "#%u", (ULONG)(ULONG_PTR)name);
					name = oname;
				}

				Name = unDNameEx(sz, (PCSTR)name, RTL_NUMBER_OF(sz), UNDNAME_DEFAULT);
			}
			else
			{
				NameVa = (INT_PTR)pDll->getBase();
				Name = "";
			}
			d = RtlPointerToOffset(NameVa, Va);
		}

		swprintf(wz, L"%p", (void*)GetFrame(ctx, pDoc));

		item.mask = LVIF_PARAM|LVIF_TEXT;
		item.lParam = (LPARAM)Va;
		item.iSubItem = 0;
		item.iItem = MAXLONG;

		item.iItem = ListView_InsertItem(_hwndST, &item);
		item.mask = LVIF_TEXT;
		Va = (PVOID)ctx.Xip;

		swprintf(wz, L"%p", Va);
		item.iSubItem++;
		ListView_SetItem(_hwndST, &item);

		if (Name)
		{
			swprintf_s(wz, _countof(wz), L"%s!%S%c+ %x", szDllName, Name, d ? ' ' : 0, d);
			item.iSubItem++;
			ListView_SetItem(_hwndST, &item);
		}
	}

	SendMessage(_hwndST, WM_SETREDRAW, TRUE, 0);
#ifdef _WIN64
	if (ctx.P1Home && pDoc->IsLocalMemory())
	{
		delete [] (void*)ctx.P1Home;
	}
#endif
}
//////////////////////////////////////////////////////////////////////////
//
#if defined(_AMD64_)
EXTERN_C extern const volatile PVOID __guard_dispatch_icall_fptr;
#endif

PVOID pKiUserExceptionDispatcher, pKiUserCallbackDispatcher, pKiUserApcDispatcher;

ZTraceView::ZTraceView(ZDbgDoc* pDoc, DWORD dwThreadId, CONTEXT* ctx)
{
	if (!pKiUserExceptionDispatcher)
	{
		STATIC_ANSI_STRING(aKiUserExceptionDispatcher,"KiUserExceptionDispatcher");
		STATIC_ANSI_STRING(aKiUserCallbackDispatcher,"KiUserCallbackDispatcher");
		STATIC_ANSI_STRING(aKiUserApcDispatcher,"KiUserApcDispatcher");

		HMODULE hmod = GetModuleHandle(L"ntdll");
		LdrGetProcedureAddress(hmod, &aKiUserExceptionDispatcher, 0, &pKiUserExceptionDispatcher);
		LdrGetProcedureAddress(hmod, &aKiUserCallbackDispatcher, 0, &pKiUserCallbackDispatcher);
		LdrGetProcedureAddress(hmod, &aKiUserApcDispatcher, 0, &pKiUserApcDispatcher);
	}

	_id = 0;
	ctx->Dr0 = 0;
	ctx->Dr1 = 0;
	ctx->Dr2 = 0;
	ctx->Dr3 = (ULONG_PTR)pKiUserExceptionDispatcher;
	ctx->Dr7 = 0x55;
	ctx->EFlags |= TRACE_FLAG|RESUME_FLAG;
	_pDoc = pDoc, _dwThreadId = dwThreadId, _LastPC = ctx->Xip, _LastStack = ctx->Xsp;
	_Level = 0;
	_bStop = FALSE;
	_Nodes[0] = TVI_ROOT;
#ifdef _WIN64
	if (ctx->SegCs == 0x23)
	{
		_LastStack |= MINLONG_PTR;
	}
#endif
	_HighLevelStack[0] = _LastStack;
	_pDoc->AddRef();
	_pDoc->AddNotify(this);
}

ZTraceView::~ZTraceView()
{
	OnDetach();
}

void ZTraceView::OnDetach()
{
	if (_pDoc) 
	{
		_pDoc->RemoveNotify(this);
		_pDoc->StopTrace(this);
		_pDoc->Release();
		_pDoc = 0;
	}
}

void ZTraceView::AddFirstReport()
{
	AddReport(_LastPC, 0, MAXDWORD);
	_HighLevelStack[_Level = 1] = _LastStack;
}

NTSTATUS Save(HWND hwndTV, TVITEM* item, ULONG dwSize);

BOOL ZTraceView::CreateClient(HWND hWndParent, int nWidth, int nHeight, PVOID /*lpCreateParams*/)
{
	if (HWND hwnd = ZStatusBar::Create(hWndParent))
	{
		SendMessage(hwnd, SB_SIMPLE, TRUE, 0);

		RECT rc;
		if (GetWindowRect(hwnd, &rc))
		{
			if (hwnd = CreateWindowExW(0, WC_TREEVIEW, L"Trace", WS_CHILD|WS_VISIBLE|
				TVS_LINESATROOT|TVS_HASLINES|TVS_HASBUTTONS|TVS_DISABLEDRAGDROP|
				TVS_TRACKSELECT|TVS_EDITLABELS, 0, 0, nWidth, nHeight - (rc.bottom - rc.top), hWndParent, (HMENU)1, 0, 0))
			{
				_hwndTV = hwnd;
				SendMessage(hwnd, WM_SETFONT, (WPARAM)ZGLOBALS::getFont()->getFont(), 0);
				return TRUE;
			}
		}

		return TRUE;
	}

	return FALSE;
}

LRESULT ZTraceView::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	TVITEM item;

	ZDbgDoc* pDoc = _pDoc;
	switch (uMsg)
	{
	case WM_NOTIFY:
		switch (((NMHDR*)lParam)->idFrom)
		{
		case 1:
			switch (((NMHDR*)lParam)->code)
			{
			case NM_RCLICK:
				hWnd = ((NMHDR*)lParam)->hwndFrom;
				if (item.hItem = TreeView_GetSelection(hWnd))
				{
					if (item.hItem == TVI_ROOT)
					{
						return 0;
					}

					WCHAR sz[512];
					item.mask = TVIF_TEXT|TVIF_PARAM;
					item.pszText = sz;
					item.cchTextMax = RTL_NUMBER_OF(sz);
					TreeView_GetItem(hWnd, &item);

					if (HMENU hmenu = LoadMenu((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDR_MENU2)))
					{
						POINT pt;
						GetCursorPos(&pt);

						HMENU hSubMenu = GetSubMenu(hmenu, 2);

						BOOL bInTrace = pDoc && pDoc->InTrace();

						EnableMenuItem(hSubMenu, ID_2_GOTOFROM, pDoc ? MF_BYCOMMAND|MF_ENABLED : MF_BYCOMMAND|MF_GRAYED);
						EnableMenuItem(hSubMenu, ID_2_GOTOTARGET, pDoc ? MF_BYCOMMAND|MF_ENABLED : MF_BYCOMMAND|MF_GRAYED);
						EnableMenuItem(hSubMenu, ID_2_STOP, bInTrace ? MF_BYCOMMAND|MF_ENABLED : MF_BYCOMMAND|MF_GRAYED);
						EnableMenuItem(hSubMenu, ID_2_SAVE, bInTrace ? MF_BYCOMMAND|MF_GRAYED : MF_BYCOMMAND|MF_ENABLED);

						switch (TrackPopupMenu(hSubMenu, TPM_RETURNCMD, pt.x, pt.y, 0, hWnd, 0))
						{
						case ID_2_GOTOFROM:
							if (PWSTR lpsz = wcschr(item.pszText, L'('))
							{
								ULONG_PTR Va;
								if ((Va = uptoul(lpsz + 1, &lpsz, 16)) && *lpsz == L')')
								{
									if (pDoc) pDoc->GoTo((PVOID)Va);
								}
							}
							break;
						case ID_2_GOTOTARGET:
							if (pDoc) pDoc->GoTo((PVOID)item.lParam);
							break;
						case ID_2_STOP:
							_bStop = TRUE;
							break;
						case ID_2_SAVE:
							item.mask = TVIF_TEXT;
							Save(hWnd, &item, _dwSize);
						}
						DestroyMenu(hmenu);
					}
				}
			}
			break;
		}
		return 0;

	case WM_DESTROY:
		if (pDoc) pDoc->StopTrace(this);
		break;
	}

	return __super::WindowProc(hWnd, uMsg, wParam, lParam);
}

void ZTraceView::AddReport(ULONG_PTR Pc, ULONG_PTR From, DWORD dw)
{
	DWORD id = _id++;

	WCHAR sz[512], from[20];
	char ns[32], buf[256];

	if (dw != MAXDWORD) 
	{
		// ret
		if (dw > 16*sizeof(PVOID)) return;
		swprintf(sz, L"ret %u (%p)[%u]", dw, (void*)From, id);
	}
	else
	{
		// call
		if (From) swprintf(from, L"(%p)", (void*)From); else *from = 0;

		PCSTR Name = 0;

		if (ZDll* pDll = _pDoc->getDllByVaNoRef((PVOID)Pc))
		{
			if (PCSTR name = pDll->getNameByVa((PVOID)Pc, 0))
			{
				if (IS_INTRESOURCE(name))
				{
					char oname[17];
					sprintf(oname, "#%u", (WORD)(ULONG_PTR)name);
					name = oname;
				}

				Name = unDNameEx(buf, (PCSTR)name, RTL_NUMBER_OF(buf), UNDNAME_NAME_ONLY);
			}
		}

		if (!Name) sprintf(ns, "Fn%p", (void*)Pc), Name = ns;
		_snwprintf(sz, RTL_NUMBER_OF(sz)-1, L"<%d>%S %s[%u]", _Level + 1, Name, from, id);
		sz[RTL_NUMBER_OF(sz)-1]=0;
	}

	TVINSERTSTRUCT tv;
	tv.hParent = _Nodes[_Level];
	tv.hInsertAfter = TVI_LAST;
	tv.item.mask = TVIF_TEXT|TVIF_PARAM;
	tv.item.pszText = sz;
	tv.item.lParam = Pc;

	_Nodes[_Level + 1] = TreeView_InsertItem(_hwndTV, &tv);

	_dwSize += WideCharToMultiByte(CP_UTF8, 0, sz, MAXULONG, 0, 0, 0, 0) + 1;

	if (_time < GetTickCount())
	{
		SetStatus();
		_time = GetTickCount() + 500;
	}
}

void ZTraceView::SetStatus()
{
	WCHAR sz[0x80];
	if (0 < swprintf_s(sz, _countof(sz), L"%u nodes level = %u, size = %u", _id, _Level + 1, _dwSize))
	{
		SendMessage(ZStatusBar::getHWND(), SB_SETTEXT, SB_SIMPLEID, (LPARAM)sz);
	}
}

NTSTATUS ZTraceView::OnException(CONTEXT* ctx)
{
	if (_bStop || _Level < 0 || RTL_NUMBER_OF(_Nodes) - 1 <= _Level)
	{
		return 0;
	}

	ULONG_PTR LastPC = _LastPC, Pc = ctx->Xip, LastStack = _LastStack, Stack = ctx->Xsp;
#ifdef _WIN64
	if (ctx->SegCs == 0x23)
	{
		Stack |= MINLONG_PTR;
	}
#endif
	_LastPC = Pc;
	_LastStack = Stack;

	ctx->Dr0 = 0;
	ctx->Dr1 = 0;
	ctx->Dr2 = 0;
	ctx->Dr3 = (ULONG_PTR)pKiUserExceptionDispatcher;
	ctx->Dr7 = 0x55;
	ctx->EFlags |= TRACE_FLAG|RESUME_FLAG;

	if (Stack == LastStack)
	{
		//rep XXX
		if (LastPC == Pc)
		{
			ctx->Dr0 = Pc + 2;
			ctx->Dr1 = Pc + 3;
			ctx->Dr2 = Pc + 4;
			ctx->Dr3 = Pc + 5;
			ctx->EFlags &= ~TRACE_FLAG;
		}
		return DBG_CONTINUE;
	}

	if (Pc == (ULONG_PTR)pKiUserExceptionDispatcher || Pc == (ULONG_PTR)pKiUserCallbackDispatcher || Pc == (ULONG_PTR)pKiUserApcDispatcher)
	{
		//DbgPrint("[%u]>>%p %p %p\n", _id, _HighLevelStack[_Level], LastStack, Stack);
		while (_HighLevelStack[_Level] < Stack) 
		{
			if (--_Level < 0) return 0;
			//DbgPrint(">>%p %p %p\n", _HighLevelStack[_Level], Stack);
		}

		AddReport(Pc, Pc == (ULONG_PTR)pKiUserExceptionDispatcher ? (ULONG_PTR)_ExceptionAddress : 0);
		_HighLevelStack[++_Level] = Stack;
		return DBG_CONTINUE;
	}

#ifdef _WIN64
	DWORD StackStepSize = 8;
	if (ctx->SegCs == 0x23)
	{
		StackStepSize = 4;
	}
#else
#define StackStepSize 4
#endif

	// call xxx
	if ((7 < Pc - LastPC) && (LastStack == Stack + StackStepSize))
	{
#if defined(_AMD64_)
		//if (__guard_dispatch_icall_fptr == (PVOID)Pc)
		//{
		//	Pc = ctx->Rax;
		//}

		if (ZDll::LdrpDispatchUserCallTarget == (PVOID)Pc || ZDll::LdrpDispatchUserCallTargetES == (PVOID)Pc)
		{
			Pc = ctx->Rax;
		}
#endif
		AddReport(Pc, LastPC);
		_HighLevelStack[++_Level] = Stack;

		return DBG_CONTINUE;
	}

	if (_HighLevelStack[_Level] < Stack)
	{
		AddReport(Pc, LastPC, (DWORD)(Stack - LastStack) - StackStepSize);
		while (0 <= --_Level && _HighLevelStack[_Level] < Stack) ;
		if (0 > _Level)
		{
			return 0;
		}
	}

	return DBG_CONTINUE;
}

_NT_END