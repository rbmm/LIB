#include "StdAfx.h"

_NT_BEGIN
#include "types.h"
#define StringEnd(sz) (&sz[RTL_NUMBER_OF(sz)])
#define STATUS_BUFFER_SMALL(status) ((status == STATUS_BUFFER_OVERFLOW) || (status == STATUS_BUFFER_TOO_SMALL) || (status == STATUS_INFO_LENGTH_MISMATCH))
template<typename T> inline T* offset_ptr(T* ptr, int offset)
{
	return (T*)((LPBYTE)ptr + offset);
}

#include "../winZ/frame.h"
#include "common.h"

struct WND_ICON_LIST 
{
	HIMAGELIST _himl;
	int _iImage;

	WND_ICON_LIST()
	{
		int cx = GetSystemMetrics(SM_CXSMICON), cy = GetSystemMetrics(SM_CYSMICON);

		_himl = 0, _iImage = -1;

		if (HIMAGELIST himl = ImageList_Create(cx, cy, ILC_COLOR32, 1, 0))
		{
			_himl = himl;

			if (HICON hi = (HICON)LoadImage((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, cx, cy, 0))
			{
				_iImage = ImageList_ReplaceIcon(himl, -1, hi);
				DestroyIcon(hi);
			}
		}
	}

	~WND_ICON_LIST()
	{
		if (_himl)
		{
			ImageList_Destroy(_himl);
		}
	}
} g_WIL;

struct TV_DATA : WINDOWINFO, TVINSERTSTRUCT
{
	HWND hwndTV;
	int iImage;
	int nItems, nLevel;
	BOOLEAN bPrintId;
	WCHAR buf[256];
};

PWSTR PrintHWND(PWSTR buf, HWND hwnd, BOOLEAN bPrintId)
{
	buf += swprintf(buf, 
#ifdef _WIN64
		(ULONG_PTR)hwnd > MAXULONG ? L"%p " :
#endif
		L"%08x ", (ULONG_PTR)hwnd);
	
	if (bPrintId)
	{
		ULONG pid, tid = GetWindowThreadProcessId(hwnd, &pid);

		if (tid)
		{
			buf += swprintf(buf, L"%x.%x ", pid, tid);
		}
	}

	return buf;
}

BOOL CALLBACK EnumThreadWndProc(HWND hwnd, TV_DATA& Data)
{
	if (0 > --Data.nItems) return FALSE;

	if (!GetWindowInfo(hwnd, &Data)) return TRUE;

	HTREEITEM hParent = Data.hParent;
	UINT state = Data.item.state;

	Data.item.lParam = (LPARAM)hwnd;

	Data.item.iSelectedImage = Data.item.iImage =
		(Data.item.state &= Data.dwStyle) ? Data.iImage : I_IMAGENONE;

	PWSTR buf = Data.buf;

	swprintf(buf + GetWindowText(hwnd, buf = PrintHWND(buf, hwnd, Data.bPrintId), 128),
		L" (%d,%d)-(%d,%d)",
		Data.rcWindow.left, Data.rcWindow.top,
		Data.rcWindow.right, Data.rcWindow.bottom);

	if (Data.hParent = TreeView_InsertItem(Data.hwndTV, static_cast<TVINSERTSTRUCT*>(&Data)))
	{
		if (--Data.nLevel)
		{
			if (hwnd = GetWindow(hwnd, GW_CHILD))
			{
				do
				{
					EnumThreadWndProc(hwnd, Data);
				} while (hwnd = GetWindow(hwnd, GW_HWNDNEXT));
			}
		}

		Data.nLevel++;
	}

	Data.item.state = state;
	Data.hParent = hParent;

	return TRUE;
}

void EnumAllWins(HWND hwnd)
{
	SetWindowTheme(hwnd, L"Explorer", 0);

	TV_DATA Data;
	Data.hwndTV = hwnd;

	Data.hParent = TVI_ROOT;
	Data.hInsertAfter = TVI_LAST;
	Data.item.mask = TVIF_TEXT | TVIF_PARAM;
	Data.item.pszText = Data.buf;
	Data.cbSize = sizeof(WINDOWINFO);
	Data.item.state = WS_VISIBLE;
	Data.nLevel = 0x100;
	Data.nItems = 0x10000;
	Data.bPrintId = TRUE;

	if (0 <= g_WIL._iImage)
	{
		TreeView_SetImageList(hwnd, g_WIL._himl, TVSIL_NORMAL);
		Data.iImage = g_WIL._iImage;
		Data.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
	}

	SendMessage(hwnd, WM_SETREDRAW, FALSE, 0);

	EnumThreadWndProc(GetDesktopWindow(), Data);

	TreeView_Expand(hwnd, TreeView_GetChild(hwnd, TVI_ROOT), TVE_EXPAND);

	SendMessage(hwnd ,WM_SETREDRAW, TRUE, 0);

	ShowWindow(hwnd, SW_SHOW);
}

struct ENUM_HWND_CTX : WINDOWINFO
{
	PWSTR end;
};

PWSTR PrintWindows(PWSTR buf, HWND hwnd, ENUM_HWND_CTX& ctx, ULONG level)
{
	if (buf + level + 512 < ctx.end && GetWindowInfo(hwnd, &ctx))
	{
		if (level)
		{
			__stosw((PUSHORT)buf, '\t', level);

			buf += level;
		}

		*buf++ = ctx.dwStyle & WS_VISIBLE ? '@' : ' ', *buf++ = ' ';
		
		buf = PrintHWND(buf, hwnd, TRUE);

		ULONG len;

		*buf++ = '<';
		if (len = GetClassName(hwnd, buf, 128))
		{
			buf += len;
		}
		else
		{
			buf += swprintf(buf, L"err=%u", GetLastError());
		}
		*buf++ = '>';

		*buf++=':';

		*buf++ = '<';

		if (len = GetWindowText(hwnd, buf, 256))
		{
			buf += len;
		}
		*buf++ = '>';
		
		buf += swprintf(buf, L" (%d,%d)-(%d,%d) %ux%u\r\n", 
			ctx.rcWindow.left, ctx.rcWindow.top, ctx.rcWindow.right, ctx.rcWindow.bottom, 
			ctx.rcWindow.right - ctx.rcWindow.left, ctx.rcWindow.bottom - ctx.rcWindow.top);

		if (++level < 256)
		{
			if (hwnd = GetWindow(hwnd, GW_CHILD))
			{
				do
				{
					buf = PrintWindows(buf, hwnd, ctx, level);
				} while (hwnd = GetWindow(hwnd, GW_HWNDNEXT));
			}
		}
	}

	return buf;
}

void PrintWindows()
{
	if (PWSTR buf = new WCHAR[0x40000])
	{
		ENUM_HWND_CTX ewd;
		ewd.cbSize = sizeof(WINDOWINFO);
		ewd.end = buf + 0x40000;
		PrintWindows(buf, GetDesktopWindow(), ewd, 0);
		ShowText(L"Windows", buf);
		delete [] buf;
	}
}

#ifndef _WIN64
//#define GetWindowLongPtrW   GetWindowLongW
#endif

class ZEnumWins : public ZFrameMultiWnd
{
	HWND _hwndLabel;

	static BOOL EnumWins(DWORD dwThreadId, HWND hwnd)
	{
		SetWindowTheme(hwnd, L"Explorer", 0);

		TV_DATA Data;
		Data.hwndTV = hwnd;

		Data.hParent = TVI_ROOT;
		Data.hInsertAfter = TVI_LAST;
		Data.item.mask = TVIF_TEXT | TVIF_PARAM;
		Data.item.pszText = Data.buf;
		Data.cbSize = sizeof(WINDOWINFO);
		Data.item.state = WS_VISIBLE;
		Data.nLevel = 0x100;
		Data.nItems = 0x10000;
		Data.bPrintId = FALSE;

		if (0 <= g_WIL._iImage)
		{
			TreeView_SetImageList(hwnd, g_WIL._himl, TVSIL_NORMAL);
			Data.iImage = g_WIL._iImage;
			Data.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		}

		SendMessage(hwnd, WM_SETREDRAW, FALSE, 0);

		EnumThreadWindows(dwThreadId, (WNDENUMPROC)EnumThreadWndProc, (LPARAM)&Data);

		SendMessage(hwnd ,WM_SETREDRAW, TRUE, 0);

		return TRUE;
	}

	virtual LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		if (uMsg == WM_NOTIFY && ((LPNMHDR)lParam)->code == TVN_SELCHANGED)
		{
			WCHAR sz[1024], szCaption[128], szClass[64];
			WINDOWINFO wi;
			wi.cbSize = sizeof wi;

			hwnd = (HWND)((LPNMTREEVIEW)lParam)->itemNew.lParam;

			if (!GetWindowInfo(hwnd, &wi)) return 0;

			if (!GetWindowText(hwnd, szCaption, RTL_NUMBER_OF(szCaption)))
				szCaption[0] = 0;

			if (!GetClassName(hwnd, szClass, RTL_NUMBER_OF(szClass)))
				szClass[0] = 0;

			PULONG pUniqueProcess = (PULONG)&((_TEB*)NtCurrentTeb())->ClientId.UniqueProcess;
			ULONG UniqueProcess = *pUniqueProcess;

			::GetWindowThreadProcessId(hwnd, pUniqueProcess);
			LPARAM pfn = (IsWindowUnicode(hwnd) ? GetWindowLongPtrW : GetWindowLongPtrA)(hwnd, GWLP_WNDPROC);
			*pUniqueProcess = UniqueProcess;

			_snwprintf(sz, RTL_NUMBER_OF(sz), 
				L"HWND = %X %s\r\n"
				L"Class : %s\r\n"
				L"Style = %08X ExStyle = %08X WNDPROC = %p\r\n"
				L"WindowRect (%d,%d)-(%d,%d) %dx%d\r\n"
				L"ClientRect (%d,%d)-(%d,%d) %dx%d", 
				(ULONG)(ULONG_PTR)hwnd, szCaption, szClass, wi.dwStyle, wi.dwExStyle, (void*)pfn,
				wi.rcWindow.left, wi.rcWindow.top,
				wi.rcWindow.right, wi.rcWindow.bottom,
				wi.rcWindow.right - wi.rcWindow.left,
				wi.rcWindow.bottom - wi.rcWindow.top,
				wi.rcClient.left, wi.rcClient.top,
				wi.rcClient.right, wi.rcClient.bottom,
				wi.rcClient.right - wi.rcClient.left,
				wi.rcClient.bottom - wi.rcClient.top);

			SetWindowText(_hwndLabel, sz);

			return 0;
		}

		return ZFrameMultiWnd::WindowProc(hwnd, uMsg, wParam, lParam);
	}

	virtual BOOL CreateClient(HWND hWndParent, int nWidth, int nHeight, PVOID lpCreateParams)
	{
		HWND hwnd = CreateWindowEx(0, WC_EDIT , 0, 
			WS_VSCROLL|WS_VISIBLE|WS_CHILD|WS_HSCROLL|ES_READONLY| 
			ES_AUTOHSCROLL | ES_MULTILINE | ES_WANTRETURN, 0, 0, nWidth, 200, hWndParent, 0, 0, 0);

		if (!hwnd)
		{
			return FALSE;
		}

		_hwndLabel = hwnd;

		WPARAM hfont = (WPARAM)ZGLOBALS::getFont()->getFont();

		SendMessage(hwnd, WM_SETFONT, hfont, 0);

		hwnd = CreateWindowEx(0, WC_TREEVIEW, 0,
			WS_VISIBLE | WS_CHILD | TVS_HASBUTTONS| TVS_HASLINES|
			TVS_LINESATROOT | TVS_DISABLEDRAGDROP | TVS_NOTOOLTIPS, 0, 200, nWidth, nHeight-200, hWndParent, 0, 0, 0);

		if (!hwnd)
		{
			return FALSE;
		}

		SendMessage(hwnd, WM_SETFONT, hfont, 0);

		return EnumWins((DWORD)(ULONG_PTR)lpCreateParams, hwnd);
	}
};

void ShowThreadWindows(DWORD dwThreadId)
{
	if (ZEnumWins* p = new ZEnumWins)
	{
		WCHAR sz[64];
		swprintf(sz, L"windows for thread %X", dwThreadId);
		p->Create(0, sz, WS_OVERLAPPEDWINDOW|WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, ZGLOBALS::getMainHWND(), 0, (PVOID)dwThreadId);
		p->Release();
	}
}

typedef void (CALLBACK * void_std_handle_dword_hwnd)(HANDLE , DWORD , HWND );

PWSTR FlagsToStringW(PWSTR buf, PWSTR end, ULONG flags, PCWSTR (* pfn)(ULONG))
{
	PWSTR c = 0;

	DWORD flag = 0x80000000;

	do 
	{
		if (PCWSTR sz = flags & flag ? pfn(flag) : 0)
		{
			if (c) buf[-1] = L'|';

			c = buf + wcslen(sz) + 1;

			if (end < c) return buf;

			memcpy(buf, sz, RtlPointerToOffset(buf, c)), buf = c;
		}

	} while(flag >>= 1);

	return buf - 1;
}

#ifdef CASE
#undef CASE
#endif
#define CASE(x) case PAGE_ ## x : return L ## #x

LPCWSTR CALLBACK GetProtectName(ULONG Protect)
{
	switch(Protect) 
	{
		CASE(GUARD);
		CASE(NOCACHE);
		CASE(WRITECOMBINE);
		CASE(NOACCESS);
		CASE(READONLY);
		CASE(READWRITE);
		CASE(WRITECOPY);
		CASE(EXECUTE);
		CASE(EXECUTE_READ);
		CASE(EXECUTE_READWRITE);
		CASE(EXECUTE_WRITECOPY);
	default: return L"?";
	}
}

#ifdef _WIN64
#define STF L"%5I64X"
#else
#define STF L"%5X"
#endif

void FormatNodeName(HANDLE hProcess, HWND hwndTV, TVINSERTSTRUCT& tvis, LPVOID AllocationBase, SIZE_T _PageCount, SIZE_T PageCount, DWORD AllocationProtect, DWORD Protect, DWORD Type)
{	
	STATIC_UNICODE_STRING(szMEM_RESERVE, "MEM_RESERVE");
	STATIC_UNICODE_STRING(szMEM_PRIVATE, "");
	STATIC_UNICODE_STRING(szMEM_IMAGE, "<Image>");
	STATIC_UNICODE_STRING(szMEM_MAPPED, "<Section>");
	STATIC_UNICODE_STRING(szMEM_UNKNOWN, "?");

	if (tvis.hParent == TVI_ROOT) return;

	if ((PageCount != _PageCount) && !TreeView_InsertItem(hwndTV, &tvis)) __debugbreak();

	SIZE_T cb, len;
	LPWSTR begin = tvis.item.pszText, lpsz, end = begin + MAX_PATH;

	NTSTATUS status;
	PCUNICODE_STRING pus = 0;
	PVOID stack = alloca(guz);

	lpsz = tvis.item.pszText + swprintf(tvis.item.pszText, L"[%p %p) " STF L" ", 
		AllocationBase, (LPBYTE)AllocationBase + (_PageCount << PAGE_SHIFT), _PageCount);

	if (Type != MEM_IMAGE) 
	{
		lpsz = FlagsToStringW(lpsz, end, Protect, GetProtectName);
		if (Protect != AllocationProtect)
		{
			*lpsz++='(';
			lpsz = FlagsToStringW(lpsz, end, AllocationProtect, GetProtectName);
			*lpsz++=')', *lpsz++=0;
		}
		else ++lpsz;
	}

	switch(Type) 
	{
	case MEM_IMAGE:
	case MEM_MAPPED:

		cb = 0, len = MAX_PATH*sizeof(WCHAR);
		do 
		{
			if (cb < len) cb = RtlPointerToOffset(pus = (PUNICODE_STRING)alloca(len - cb), stack);
			status = ZwQueryVirtualMemory(hProcess, AllocationBase, MemoryMappedFilenameInformation, (void*)pus, cb, &len);	
		} while(STATUS_BUFFER_SMALL(status));

		if (0 > status) pus = (Type == MEM_IMAGE) ? &szMEM_IMAGE : &szMEM_MAPPED;
		break;
	case MEM_RESERVE:
		pus = &szMEM_RESERVE;
		break;
	case MEM_PRIVATE:
		pus = &szMEM_PRIVATE;
		break;
	default: pus = &szMEM_UNKNOWN;
	}

	cb = (len = RtlPointerToOffset(begin, lpsz)) + pus->Length + sizeof WCHAR;

	if (MAX_PATH * sizeof WCHAR < cb) 
	{
		memcpy(tvis.item.pszText = (LPWSTR)alloca(cb), begin, len);
		lpsz = offset_ptr(tvis.item.pszText, (int)len);
	} 

	lpsz[-1] = L' ';
	swprintf(lpsz, L"%wZ", pus);

	if (!TreeView_SetItem(hwndTV, &tvis.item)) __debugbreak();

	tvis.hParent = TVI_ROOT;
	tvis.item.pszText = begin;
}

#ifndef INVALID_ADDRESS
#define INVALID_ADDRESS ((PCH)-1)
#endif

void CALLBACK DumpHandles(HANDLE /*hProcess*/, DWORD dwProcessId, HWND hwndTV)
{
	PSYSTEM_HANDLE_INFORMATION_EX pshti = 0;

	SIZE_T RegionSize = 0x800000;
	
	if (0 <= ZwAllocateVirtualMemory(NtCurrentProcess(), (void**)&pshti, 0, &RegionSize, MEM_COMMIT, PAGE_READWRITE))
	{
		FQH fqh = { dwProcessId, g_ThreadIndex, g_ProcessIndex, g_FileIndex};
		
		IO_STATUS_BLOCK iosb;
		if (0 <= ZwDeviceIoControlFile(g_hDrv, 0, 0, 0, &iosb, IOCTL_QueryHandles, &fqh, sizeof(fqh), pshti, (ULONG)RegionSize))
		{
			if (ULONG_PTR NumberOfHandles = pshti->NumberOfHandles)
			{
				PSYSTEM_HANDLE_TABLE_ENTRY_INFO_EX Handles = pshti->Handles;

				DWORD NumberOfTypes = g_AOTI.count();
				WCHAR sz[300];
				TVINSERTSTRUCT tvis;
				tvis.hInsertAfter = TVI_LAST;
				tvis.item.mask = TVIF_TEXT;
				tvis.item.pszText = sz;

				HTREEITEM* phItems = (HTREEITEM*)alloca(NumberOfTypes*sizeof(HTREEITEM));
				RtlZeroMemory(phItems, NumberOfTypes*sizeof(HTREEITEM));

				do 
				{
					if (Handles->Reserved)
					{
						ULONG Index = g_AOTI.TypeIndexToIndex(Handles->ObjectTypeIndex);

						if (const OBJECT_TYPE_INFORMATION* poti = g_AOTI[Index])
						{
							if (!(tvis.hParent = phItems[Index]))
							{
								swprintf(sz, L"%wZ", &poti->TypeName);
								phItems[Index] = tvis.hParent = TreeView_InsertItem(hwndTV, &tvis);
							}

							_snwprintf(sz, RTL_NUMBER_OF(sz), L"%x[%08x](%p) {%x} %.*s",
								(ULONG)Handles->HandleValue, Handles->GrantedAccess, Handles->Object,
								Handles->HandleAttributes,
								Handles->CreatorBackTraceIndex>>1, 
								(PWSTR)RtlOffsetToPointer(pshti, Handles->Reserved));
							TreeView_InsertItem(hwndTV, &tvis);
						}
					}
				} while (Handles++, --NumberOfHandles);
			}
		}
		ZwFreeVirtualMemory(NtCurrentProcess(), (void**)&pshti, &RegionSize, MEM_RELEASE);
	}
}

void CALLBACK DumpMemory(HANDLE hProcess, DWORD Type, HWND hwndTV)
{
	WCHAR sz[MAX_PATH], *lpsz;
	MEMORY_BASIC_INFORMATION mbi;
	TVINSERTSTRUCT tvis;
	tvis.hInsertAfter = TVI_LAST;
	tvis.item.mask = 0;
	tvis.item.pszText = sz;
	tvis.hParent = TVI_ROOT;
	mbi.State = MEM_FREE;
	PVOID AllocationBase = INVALID_ADDRESS;
	DWORD AllocationProtect = 0, Protect = 0;
	SIZE_T PageCount = 0, _PageCount = 0;//4700

	PVOID BaseAddress = 0;

	while (0 <= (INT_PTR)BaseAddress && 0 <= ZwQueryVirtualMemory(hProcess, BaseAddress, MemoryBasicInformation, &mbi, sizeof(mbi), 0)) 
	{
		BaseAddress = (PBYTE)mbi.BaseAddress + mbi.RegionSize;

		if (mbi.State == MEM_FREE) continue;

		if (mbi.AllocationBase != AllocationBase) 
		{
			//if (mbi.AllocationBase == (PVOID)0x77750000) __DbgBreak();

			FormatNodeName(hProcess, hwndTV, tvis, AllocationBase, 
				_PageCount, PageCount, AllocationProtect, Protect, Type);

			Type = mbi.State == MEM_RESERVE ? MEM_RESERVE : mbi.Type;
			_PageCount = 0;
			AllocationProtect = mbi.AllocationProtect;
			Protect = mbi.Protect;
			AllocationBase = mbi.AllocationBase;

			tvis.item.mask = 0;

			if (!(tvis.hParent = TreeView_InsertItem(hwndTV, &tvis))) return;

			tvis.item.mask = TVIF_TEXT, tvis.item.hItem = tvis.hParent;

		} 
		else if (!TreeView_InsertItem(hwndTV, &tvis)) return;

		PageCount = mbi.RegionSize >> PAGE_SHIFT, _PageCount += PageCount;

		switch(mbi.State) 
		{
		case MEM_COMMIT:

			lpsz = sz + swprintf(sz, L"[%p %p) " STF L" ", mbi.BaseAddress, BaseAddress, PageCount);

			lpsz = FlagsToStringW(lpsz, StringEnd(sz), mbi.Protect, GetProtectName);

			if (Protect != AllocationProtect)
			{
				*lpsz++='(';
				lpsz = FlagsToStringW(lpsz, StringEnd(sz), mbi.AllocationProtect, GetProtectName);
				*lpsz++=')', *lpsz++=0;;
			}

			break;

		case MEM_RESERVE:

			swprintf(sz, L"[%p %p) " STF L" MEM_RESERVE", mbi.BaseAddress, BaseAddress, PageCount);
			break;

		default: 
			swprintf(sz, L"[%p %p) " STF L" ?", mbi.BaseAddress, BaseAddress, PageCount);
			break;
		}		
	}

	FormatNodeName(hProcess, hwndTV, tvis, AllocationBase, _PageCount, PageCount, AllocationProtect, Protect, Type);
}

void ShowHanlesOrMemory(HANDLE hProcess, DWORD dwProcessId, PCWSTR caption, void_std_handle_dword_hwnd pfn)
{
	PUNICODE_STRING ImageFileName = 0;
	PVOID stack = alloca((64 + wcslen(caption)) << 1);

	ULONG cb = 0, rcb = MAX_PATH;

	NTSTATUS status;
	do 
	{
		if (cb < rcb) cb = RtlPointerToOffset(ImageFileName = (PUNICODE_STRING)alloca(rcb - cb), stack);

		status = ZwQueryInformationProcess(hProcess, ProcessImageFileName, ImageFileName, cb, &rcb);

	} while (status == STATUS_INFO_LENGTH_MISMATCH);

	if (0 > status) ImageFileName->Length = 0;

	if (!ImageFileName->Length) ImageFileName->Buffer = (LPWSTR)(1 + ImageFileName);

	PWSTR lpsz = offset_ptr(ImageFileName->Buffer, ImageFileName->Length);

	*lpsz++ = L' ';

	swprintf(lpsz, L"[%X] %s", dwProcessId, caption);

	_ZGLOBALS* globals = ZGLOBALS::get();

	if (HWND hwnd = CreateWindowEx(WS_EX_CLIENTEDGE,
		WC_TREEVIEW, ImageFileName->Buffer, 
		WS_OVERLAPPEDWINDOW|TVS_HASBUTTONS|TVS_HASLINES|TVS_LINESATROOT|TVS_EDITLABELS|TVS_DISABLEDRAGDROP|TVS_NOTOOLTIPS,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, globals->hwndMain, 0, 0, 0))
	{
		SendMessage(hwnd, WM_SETFONT, (WPARAM)globals->Font->getFont(), 0);

		pfn(hProcess, dwProcessId, hwnd);

		ShowWindow(hwnd, SW_SHOWNORMAL);
	}
}

void ShowHanlesOrMemory(DWORD dwProcessId, PCWSTR caption, void_std_handle_dword_hwnd pfn)
{
	HANDLE hProcess;

	CLIENT_ID cid = { (HANDLE)dwProcessId };

	if (0 <= MyOpenProcess(&hProcess, PROCESS_QUERY_INFORMATION|PROCESS_DUP_HANDLE, &zoa, &cid))
	{
		ShowHanlesOrMemory(hProcess, dwProcessId, caption, pfn);
		NtClose(hProcess);
	}
}

void ShowProcessMemory(HANDLE hProcess, DWORD dwProcessId)
{
	ShowHanlesOrMemory(hProcess, dwProcessId, L"VirtualMemory", DumpMemory);	
}

void ShowProcessMemory(DWORD dwProcessId)
{
	ShowHanlesOrMemory(dwProcessId, L"VirtualMemory", DumpMemory);	
}

void ShowProcessHandles(HANDLE hProcess, DWORD dwProcessId)
{
	ShowHanlesOrMemory(hProcess, dwProcessId, L"Handles", DumpHandles);	
}

void ShowProcessHandles(DWORD dwProcessId)
{
	ShowHanlesOrMemory(0, dwProcessId, L"Handles", DumpHandles);	
}

struct DTH 
{
	HANDLE hObject;
	HWND hwnd;
	void (* DumpFN)(HWND , HANDLE );
};

void DumpToken(HWND hwnd, HANDLE hToken);
void DumpObjectSecurity(HWND hwnd, HANDLE hObject);

void CALLBACK _ShowToken(DTH* p)
{
	p->DumpFN(p->hwnd, p->hObject);
	NtClose(p->hObject);
	delete p;
}

void ShowObject(HANDLE hObject, PCWSTR caption, void (* DumpFN)(HWND , HANDLE ))
{
	_ZGLOBALS* globals = ZGLOBALS::get();

	if (HWND hwnd = CreateWindowExW(0, WC_EDIT, caption, WS_OVERLAPPEDWINDOW|WS_VSCROLL|ES_MULTILINE,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, globals->hwndMain, 0, 0, 0))
	{
		SendMessage(hwnd, WM_SETFONT, (WPARAM)globals->Font->getFont(), 0);

		if (DTH* p = new DTH)
		{
			p->hObject = hObject;
			p->hwnd = hwnd;
			p->DumpFN = DumpFN;

			if (QueueUserWorkItem((PTHREAD_START_ROUTINE)_ShowToken, p, 0))
			{
				ShowWindow(hwnd, SW_SHOWNORMAL);
				return;
			}

			delete p;
		}

		DestroyWindow(hwnd);
	}

	NtClose(hObject);
}

void ShowToken(HANDLE hToken, ULONG id, PCWSTR caption)
{
	WCHAR sz[64];
	swprintf(sz, L"%x %s Token", id, caption);
	ShowObject(hToken, sz, DumpToken);
}

void ShowObjectSecurity(HANDLE hObject, ULONG id, PCWSTR caption)
{
	WCHAR sz[64];
	swprintf(sz, L"%x %s Security Descriptor", id, caption);
	ShowObject(hObject, sz, DumpObjectSecurity);
}

PWSTR BuildProcessList(PWSTR lpsz, IMemoryDump* pDump, PVOID UdtCtx);

void ShowDumpProcesses(ULONG id, PVOID UdtCtx, IMemoryDump* pDump)
{
	PVOID BaseAddress = 0;
	SIZE_T ViewSize = 0x10000;
	if (0 <= ZwAllocateVirtualMemory(NtCurrentProcess(), &BaseAddress, 0, &ViewSize, MEM_COMMIT, PAGE_READWRITE))
	{
		__try {
			if (BuildProcessList((PWSTR)BaseAddress, pDump, UdtCtx) != BaseAddress)
			{
				WCHAR sz[64];
				swprintf(sz, L"%x Process List", id);
				_ZGLOBALS* globals = ZGLOBALS::get();

				if (HWND hwnd = CreateWindowExW(0, WC_EDIT, sz, WS_OVERLAPPEDWINDOW|WS_VSCROLL|ES_MULTILINE,
					CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, globals->hwndMain, 0, 0, 0))
				{
					SendMessage(hwnd, WM_SETFONT, (WPARAM)globals->Font->getFont(), 0);

					SetWindowTextW(hwnd, (PWSTR)BaseAddress);

					ShowWindow(hwnd, SW_SHOWNORMAL);
				}
			}
		}
		__except(EXCEPTION_EXECUTE_HANDLER){
		}
		ZwFreeVirtualMemory(NtCurrentProcess(), &BaseAddress, &ViewSize,  MEM_RELEASE);
	}
}

_NT_END