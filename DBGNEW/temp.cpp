#include "StdAfx.h"

_NT_BEGIN

#include "common.h"

struct PC_LEVEL 
{
	LONG_PTR Pc : 48;
	LONG_PTR Lv : 16;
};

struct TRACE_DATA 
{
	ULONG_PTR _LastPC, _LastStack;
	ULONG_PTR _Stack[64];
	PETHREAD _Thread;
	PC_LEVEL* _p;
	int _Level;
	ULONG _n, _k, _s;
	PC_LEVEL _data[];
};

class ZTraceView2 : public ZSubClass
{
	HTREEITEM		_Nodes[64];
	ZDbgDoc*		_pDoc;

	virtual LRESULT WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	~ZTraceView2()
	{
	}

	void CreateReport(LPARAM lParam);
public:

	ZTraceView2(ZDbgDoc* pDoc)
	{
		_pDoc = pDoc;
	}
};

LRESULT ZTraceView2::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	TVITEM item;

	switch (uMsg)
	{
	case ZDragPtr::WM_DROP:
		CreateReport(lParam);;
		break;

	case WM_RBUTTONDOWN:
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

				switch (TrackPopupMenu(GetSubMenu(hmenu, 2), TPM_RETURNCMD, pt.x, pt.y, 0, hWnd, 0))
				{
				case ID_2_GOTOFROM:
					if (PWSTR lpsz = wcschr(item.pszText, L'('))
					{
						ULONG_PTR Va;
						if ((Va = uptoul(lpsz + 1, &lpsz, 16)) && *lpsz == L')')
						{
							_pDoc->GoTo((PVOID)Va);
						}
					}
					break;
				case ID_2_GOTOTARGET:
					_pDoc->GoTo((PVOID)item.lParam);
					break;
				}
				DestroyMenu(hmenu);
			}
		}
		return 0;
	case WM_DESTROY:
		break;
	}
	return ZSubClass::WindowProc(hWnd, uMsg, wParam, lParam);
}

void NT::ZTraceView2::CreateReport(LPARAM lParam)
{
	HWND hwnd = getHWND();
	TreeView_DeleteAllItems(hwnd);
	
	TRACE_DATA* p = (TRACE_DATA*)alloca(0x40000);
	ZDbgDoc* pDoc = _pDoc;
	if (!pDoc->Read((PVOID)lParam, p, 0x40000))
	{
		PC_LEVEL* pcl = p->_data+2;
		DWORD id = 0;

		//if (pcl < _pcl)
		{
			LONG_PTR From, To;
			ULONG_PTR L, MaxValidLevel = 0, Lv;
			From = pcl->Pc;
			L = pcl++->Lv;
			if (L)
			{
				__DbgBreak();
				return;
			}
			ULONG n = 0xffff-p->_n;

			HTREEITEM* Nodes = _Nodes;

			WCHAR sz[256];
			char ns[32], buf[256];

			TVINSERTSTRUCT tv;
			tv.hParent = TVI_ROOT;
			tv.hInsertAfter = TVI_LAST;
			tv.item.mask = TVIF_TEXT|TVIF_PARAM;
			tv.item.pszText = sz;
			tv.item.lParam = From;

			swprintf(sz, L"%p [0]", (void*)From);

			Nodes[0] = TreeView_InsertItem(hwnd, &tv);

			do 
			{
				From = pcl->Pc;
				L = pcl++->Lv;
				if (MaxValidLevel < L)
				{
					__DbgBreak();
					return ;
				}
				tv.hParent = Nodes[L];
				To = pcl->Pc;
				Lv = pcl++->Lv;
				if (RTL_NUMBER_OF(_Nodes) < Lv)
				{
					__DbgBreak();
					return ;
				}

				MaxValidLevel = Lv;

				BOOL bNewLevel = FALSE;

				switch (Lv - L)
				{
				case 0://jmp
					swprintf(sz, L"jmp %p (%p)", (void*)To, (void*)From);
					break;
				case +1://call
					bNewLevel = TRUE;
					{
						PCSTR Name = 0;

						if (ZDll* pDll = pDoc->getDllByVaNoRef((PVOID)To))
						{
							if (PCSTR name = pDll->getNameByVa((PVOID)To, 0))
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

						if (!Name) sprintf(ns, "Fn%p", (void*)To), Name = ns;
						_snwprintf(sz, RTL_NUMBER_OF(sz)-1, L"<%d>%S (%p)[%u]", (int)Lv, Name, (void*)From, id);
						sz[RTL_NUMBER_OF(sz)-1]=0;

					}
					break;
				default://ret
					swprintf(sz, L"ret (%p) [%u]", (void*)From, id++);
				}

				tv.item.lParam = To;

				HTREEITEM h = TreeView_InsertItem(hwnd, &tv);
				if (bNewLevel) Nodes[Lv] = h;

			} while (--n);
		}
	}
}

void TempCreateView(ZDbgDoc* pDoc)
{
	if (HWND hwnd = CreateWindowExW(0, WC_TREEVIEW, L"Trace2", WS_POPUP|WS_VISIBLE|WS_CAPTION |
		WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_THICKFRAME |
		TVS_LINESATROOT|TVS_HASLINES|TVS_HASBUTTONS|TVS_DISABLEDRAGDROP|
		TVS_TRACKSELECT|TVS_EDITLABELS, 320, 256, 800, 640, ZGLOBALS::getMainHWND(), 0, 0, 0))
	{
		SendMessage(hwnd, WM_SETFONT, (WPARAM)ZGLOBALS::getFont()->getFont(), 0);

		if (ZTraceView2* pTraceView = new ZTraceView2(pDoc))
		{
			pTraceView->Subclass(hwnd);
		}
	}
}

_NT_END

