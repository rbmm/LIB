#include "StdAfx.h"

_NT_BEGIN

#include "common.h"
#include "../inc/idcres.h"

struct ZType;
struct AaT;

void FillCB(HWND hwnd, PVOID UdtCtx);
BOOL ExpandUDT(ZDbgDoc* pDoc, HWND hwndTV, HTREEITEM hParent, AaT* p);
PVOID RootExpand(HWND hwndTV, PVOID Address, ZType* pType);
void GetNodeText(LPTVITEM item);
void FreeUdtData(AaT* p);

struct LAAT 
{
	ULONG_PTR Address, Type;
};

class ZWatch : public ZFrameWnd, ZView
{
	HWND _hwndTV;
	PVOID _pvRoot;

	virtual HWND CreateView(HWND hWndParent, int nWidth, int nHeight, PVOID lpCreateParams);

	virtual LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	ZWatch(ZDbgDoc* pDoc);

	virtual PCUNICODE_STRING getPosName();

	virtual ZWnd* getWnd();

	virtual ZView* getView();

	virtual void OnUpdate(ZView* pSender, LPARAM lHint, PVOID pHint);

	virtual void OnDocumentActivate(BOOL bActivate)
	{
		ShowWindow(getHWND(), bActivate ? SW_SHOW : SW_HIDE);
	}

	~ZWatch()
	{
		if (_pvRoot)
		{
			delete _pvRoot;
		}
	}
	friend void ZWatch_Create(ZDbgDoc* pDoc, ULONG_PTR _Address, ULONG_PTR _Type);
};

void DoCollapse(HWND hwnd, HTREEITEM hItem)
{
	TVITEM item = { TVIF_PARAM, hItem };

	if (TreeView_GetItem(hwnd, &item))
	{
		if (hItem = TreeView_GetChild(hwnd, hItem))
		{
			do 
			{
				HTREEITEM _hItem = hItem;
				hItem = TreeView_GetNextSibling(hwnd, hItem);
				TreeView_DeleteItem(hwnd, _hItem);
			} while (hItem);
		}
	}
}

PCUNICODE_STRING ZWatch::getPosName()
{
	STATIC_UNICODE_STRING_(Watch);
	return &Watch;
}

void ZWatch::OnUpdate(ZView* /*pSender*/, LPARAM lHint, PVOID /*pHint*/)
{
	switch (lHint)
	{
	case ALL_UPDATED:
	case BYTE_UPDATED:
		HTREEITEM hItem = TreeView_GetChild(_hwndTV, TVI_ROOT);
		TreeView_Expand(_hwndTV, hItem, TVE_COLLAPSE);
		DoCollapse(_hwndTV, hItem);
		break;
	}
}

ZView* ZWatch::getView()
{
	return this;
}

ZWnd* ZWatch::getWnd()
{
	return this;
}

HWND ZWatch::CreateView(HWND hWndParent, int nWidth, int nHeight, PVOID lpCreateParams)
{
	if (_hwndTV = CreateWindowExW(0, WC_TREEVIEW, 0, WS_CHILD|WS_VISIBLE|TVS_HASLINES|TVS_LINESATROOT|TVS_HASBUTTONS|TVS_DISABLEDRAGDROP|
		TVS_EDITLABELS, 0, 0, nWidth, nHeight, hWndParent, 0, 0, 0))
	{
		if (HFONT hfont = ZGLOBALS::getFont()->getStatusFont())
		{
			SendMessage(_hwndTV, WM_SETFONT, (WPARAM)hfont, 0);
		}

		_pvRoot = RootExpand(_hwndTV, (PVOID)((LAAT*)lpCreateParams)->Address, (ZType*)((LAAT*)lpCreateParams)->Type);

		if (!_pvRoot)
		{
			DestroyWindow(_hwndTV);
			_hwndTV = 0;
		}
	}

	return _hwndTV;
}

LRESULT ZWatch::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code)
		{
		case TVN_ITEMEXPANDING:
			switch (((LPNMTREEVIEW)lParam)->action)
			{
			case TVE_EXPAND:
				return ExpandUDT(GetDbgDoc(), _hwndTV, 
					((LPNMTREEVIEW)lParam)->itemNew.hItem, 
					(AaT*)((LPNMTREEVIEW)lParam)->itemNew.lParam);
			}
			return FALSE;

		case TVN_ITEMEXPANDED:
			switch (((LPNMTREEVIEW)lParam)->action)
			{
			case TVE_COLLAPSE:
				DoCollapse(_hwndTV, ((LPNMTREEVIEW)lParam)->itemNew.hItem);
				break;
			}
			return FALSE;

		case TVN_DELETEITEM:
			FreeUdtData((AaT*)((LPNMTREEVIEW)lParam)->itemOld.lParam);
			break;

		case TVN_GETDISPINFO:
			GetNodeText(&((LPNMTVDISPINFO) lParam )->item);
			break;
		}
		break;
	}
	return ZFrameWnd::WindowProc(hwnd, uMsg, wParam, lParam);
}

ZWatch::ZWatch(ZDbgDoc* pDoc) : ZView(pDoc)
{
	_hwndTV = 0;
	_pvRoot = 0;
}

void ZWatch_Create(ZDbgDoc* pDoc, ULONG_PTR Address, ULONG_PTR Type)
{
	if (ZWatch* p = new ZWatch(pDoc))
	{
		WCHAR sz[32];
		swprintf(sz, L"%x Watch", pDoc->getId());

		LAAT s = { Address, Type };

		p->ZFrameWnd::Create(WS_EX_TOOLWINDOW, sz, WS_POPUP|WS_OVERLAPPEDWINDOW|WS_VISIBLE, 500, 300, 800, 256, ZGLOBALS::getMainHWND(), 0, &s);

		p->Release();
	}
}

class ZAddWatch : public ZDlg, ZDetachNotify
{
	ZDbgDoc* _pDoc;
	ULONG_PTR _Address, _Type;

	virtual void OnDetach()
	{
		DestroyWindow(getHWND());
	}

	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		int i;

		switch (uMsg)
		{
		case WM_DESTROY:
			_pDoc->RemoveNotify(this);
			break;

		case WM_INITDIALOG:
			_pDoc->AddNotify(this);
			FillCB(GetDlgItem(hwndDlg, IDC_COMBO1), _pDoc->getUdtCtx());
			break;

		case WM_COMMAND:
			switch (wParam)
			{
			case MAKEWPARAM(IDC_EDIT1, EN_UPDATE):
				_Address = 0;
				if ((i = GetWindowTextLength((HWND)lParam)) && i <= 2*sizeof(PVOID))
				{
					PWSTR sz = (PWSTR)alloca(++i<<1);
					GetWindowText((HWND)lParam, sz, i);

					ULONG_PTR u = uptoul(sz, &sz, 16);
					if (!*sz)
					{
						_Address = u;
					}
				}
				goto __m;

			case MAKEWPARAM(IDC_COMBO1, CBN_CLOSEUP):
				_Type = 0 > (i = ComboBox_GetCurSel((HWND)lParam)) ? 0 : ComboBox_GetItemData((HWND)lParam, i);
__m:
				EnableWindow(GetDlgItem(hwndDlg, IDOK), _Address && _Type);
				break;

			case IDOK:
				if (_Address && _Type)
				{
					ZWatch_Create(_pDoc, _Address, _Type);
				}
				break;
			case IDCANCEL:
				DestroyWindow(hwndDlg);
				break;
			}
			break;
		}

		return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
	}

	~ZAddWatch()
	{
		_pDoc->Release();
	}
public:

	ZAddWatch(ZDbgDoc* pDoc)
	{
		_Address = 0;
		_Type = 0;
		_pDoc = pDoc;
		pDoc->AddRef();
	}
};

void AddWatch(ZDbgDoc* pDoc)
{
	if (ZAddWatch* p = new ZAddWatch(pDoc))
	{
		p->Create((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDD_DIALOG14), ZGLOBALS::getMainHWND(), 0);
		p->Release();
	}
}

_NT_END
