#include "StdAfx.h"

_NT_BEGIN

#include "app.h"
#include "window.h"

ULONG GetNONCLIENTMETRICSWSize()
{
	static ULONG m;
	if (!m)
	{
		RtlGetNtVersionNumbers(&m, 0, 0);
	}

	return m < 6 ? sizeof(NONCLIENTMETRICS) - 4 : sizeof(NONCLIENTMETRICS);
}
//////////////////////////////////////////////////////////////////////////
// ZRegistry

ZRegistry::ZRegistry()
{
	_hKey = 0;

	if (ZGLOBALS* globals = ZGLOBALS::get())
	{
		globals->Reg = this;
	}
}

ZRegistry::~ZRegistry()
{
	if (ZGLOBALS* globals = ZGLOBALS::get())
	{
		globals->Reg = 0;
	}

	if (_hKey)
	{
		NtClose(_hKey);
	}
}

NTSTATUS ZRegistry::Open(PHANDLE KeyHandle, PCUNICODE_STRING SubKey)
{
	OBJECT_ATTRIBUTES oa = { sizeof(oa), _hKey, (PUNICODE_STRING)SubKey, OBJ_CASE_INSENSITIVE };
	return ZwOpenKey(KeyHandle, KEY_ALL_ACCESS, &oa);
}

NTSTATUS ZRegistry::Create(PHANDLE KeyHandle, PCUNICODE_STRING SubKey)
{
	OBJECT_ATTRIBUTES oa = { sizeof(oa), _hKey, (PUNICODE_STRING)SubKey, OBJ_CASE_INSENSITIVE };
	return ZwCreateKey(KeyHandle, KEY_ALL_ACCESS, &oa, 0, 0, 0, 0);
}

NTSTATUS ZRegistry::Open(PCWSTR path)
{
	UNICODE_STRING RegistryPath;

	NTSTATUS status = RtlFormatCurrentUserKeyPath(&RegistryPath);

	if (0 <= status)
	{
		PWSTR sz = (PWSTR)alloca((wcslen(path) << 1) + RegistryPath.Length + 2 * sizeof(WCHAR));

		swprintf(sz, L"%wZ\\%s", &RegistryPath, path);

		status = ZwCreateKey(&_hKey, KEY_ALL_ACCESS, &CObjectAttributes(sz), 0, 0, 0, 0);

		RtlFreeUnicodeString(&RegistryPath);
	}

	return status;
}

NTSTATUS ZRegistry::Create(PCWSTR path)
{
	UNICODE_STRING RegistryPath;
	
	NTSTATUS status = RtlFormatCurrentUserKeyPath(&RegistryPath);
	
	if (0 <= status)
	{
		PWSTR sz = (PWSTR)alloca((wcslen(path) << 1) + RegistryPath.Length + 2 * sizeof(WCHAR));

		swprintf(sz, L"%wZ\\%s", &RegistryPath, path);

		status = ZwCreateKey(&_hKey, KEY_ALL_ACCESS, &CObjectAttributes(sz), 0, 0, 0, 0);

		RtlFreeUnicodeString(&RegistryPath);
	}

	return status;
}

NTSTATUS ZRegistry::SaveRect(PCUNICODE_STRING Name, PRECT prc)
{
	return SetValue(Name, REG_BINARY, prc, sizeof(RECT));
}

NTSTATUS ZRegistry::LoadRect(PCUNICODE_STRING Name, PRECT prc)
{
	PKEY_VALUE_PARTIAL_INFORMATION pkvpi = (PKEY_VALUE_PARTIAL_INFORMATION)alloca(FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data[sizeof(RECT)]));
	NTSTATUS status = GetValue(Name, KeyValuePartialInformation, pkvpi, FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data[sizeof(RECT)]), &pkvpi->TitleIndex);
	if (0 <= status) memcpy(prc, pkvpi->Data, sizeof(RECT));
	return status;
}

void ZRegistry::SaveWinPos(PCUNICODE_STRING Name, HWND hwnd)
{
	WINDOWPLACEMENT wp = { sizeof(WINDOWPLACEMENT) };

	if (GetWindowPlacement(hwnd, &wp))
	{
		if (SW_SHOWMAXIMIZED == wp.showCmd)
		{
			wp.rcNormalPosition.top |= 0x80000000;
		}

		SaveRect(Name, &wp.rcNormalPosition);
	}
}

void ZRegistry::LoadWinPos(PCUNICODE_STRING Name, int& x, int& y, int& width, int& height, DWORD& dwStyle)
{
	RECT rc;

	if (0 > LoadRect(Name, &rc))
	{
		return;
	}

	if (rc.top & 0x80000000)
	{
		rc.top &= ~0x80000000;
		dwStyle |= WS_MAXIMIZE;
	}

	int nWidth = rc.right - rc.left, nHeight = rc.bottom - rc.top;

	int cx = GetSystemMetrics(SM_CXSCREEN), cy = GetSystemMetrics(SM_CYSCREEN), mx = GetSystemMetrics(SM_CXMINTRACK), my = GetSystemMetrics(SM_CYMINTRACK);

	if (
		nWidth > cx || rc.left > cx - mx || rc.left < 0 || 
		nHeight > cy || rc.top > cy - my || rc.top < 0
		)
	{
		return;
	}

	x = rc.left, y = rc.top, width = nWidth, height = nHeight;
}

//////////////////////////////////////////////////////////////////////////
// ZApp

ZApp::ZApp()
{
	InitializeListHead(&_headTM);
	InitializeListHead(&_headIdle);
	if (ZGLOBALS* globals = ZGLOBALS::get())
	{
		globals->App = this;
	}
}

ZApp::~ZApp()
{
	if (ZGLOBALS* globals = ZGLOBALS::get())
	{
		globals->App = 0;
	}
}

void ZApp::InsertTM(PLIST_ENTRY entry)
{
	InsertHeadList(&_headTM, entry);
}

void ZApp::InsertIdle(PLIST_ENTRY entry)
{
	InsertHeadList(&_headIdle, entry);
}

BOOL ZApp::PreTranslateMessage(PMSG lpMsg)
{
	PLIST_ENTRY head = &_headTM, entry = head;

	while ((entry = entry->Flink) != head)
	{
		if (static_cast<ZTranslateMsg*>(entry)->PreTranslateMessage(lpMsg))
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOL ZApp::IsIdleMessage(UINT uMsg)
{
	switch (uMsg)
	{
	case 0x118:
	case WM_TIMER:
	case WM_PAINT:
	case WM_MOUSEMOVE:
	case WM_NCMOUSEMOVE:
	case WM_MOUSELEAVE:
	case WM_MOUSEHOVER:
	case WM_NCMOUSELEAVE:
	case WM_NCMOUSEHOVER:
		return FALSE;
	}

	return TRUE;
}

void ZApp::OnIdle()
{
	PLIST_ENTRY head = &_headIdle, entry = head;

	while ((entry = entry->Flink) != head)
	{
		static_cast<ZIdle*>(entry)->OnIdle();
	}
}

void ZApp::Run()
{
	for (;;)
	{
		HANDLE* pHandles;

		DWORD nCount = GetWaitHandles(&pHandles);

		DWORD r = MsgWaitForMultipleObjectsEx(nCount, pHandles, GetTimeout(), QS_ALLINPUT, MWMO_ALERTABLE|MWMO_INPUTAVAILABLE);

		if (r == nCount)
		{
			BOOL bIdle = FALSE;

			MSG msg;

			while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
			{
				if (!bIdle)
				{
					bIdle = IsIdleMessage(msg.message);
				}

				if (PreTranslateMessage(&msg)) continue;

				if (msg.message == WM_QUIT) 
				{
					return ;
				}

				if (!IsDialogMessageEx(&msg))
				{
					if (msg.message - WM_KEYFIRST <= WM_KEYLAST - WM_KEYFIRST)
					{
						TranslateMessage(&msg);
					}
					DispatchMessage(&msg);
				}
			}

			if (bIdle)
			{
				OnIdle();
			}

			continue;
		}

		if (r < nCount)
		{
			OnSignalObject(r);
			continue;
		}

		switch(r)
		{
		case STATUS_USER_APC:
			OnApcAlert();
			continue;
		case STATUS_TIMEOUT:
			OnTimeout();
			continue;
		}

		if ((r -= STATUS_ABANDONED) < nCount)
		{
			OnAbandoned(r);
			continue;
		}

		__debugbreak();
	}
}

void ZApp::OnTimeout()
{
}

DWORD ZApp::GetTimeout()
{
	return INFINITE;
}

DWORD ZApp::GetWaitHandles(HANDLE **)
{
	return 0;
}

void ZApp::OnSignalObject(DWORD)
{
}

void ZApp::OnApcAlert()
{
}

void ZApp::OnAbandoned(DWORD )
{
}

BOOL ZApp::CanClose(HWND /*hwnd*/)
{
	return TRUE;
}
//////////////////////////////////////////////////////////////////////////
// ZAppEx

void ZSignalObject::OnAbandoned()
{
	__debugbreak();
}

BOOL ZAppEx::addWaitObject(ZSignalObject* pObject, HANDLE hObject)
{
	if (_nCount < RTL_NUMBER_OF(_Handles))
	{
		_Handles[_nCount] = hObject;
		_objects[_nCount++] = pObject;
		
		return TRUE;
	}
	
	return FALSE;
}

BOOL ZAppEx::delWaitObject(ZSignalObject* pObject)
{
	if (DWORD nCount = _nCount)
	{
		ZSignalObject** ppObjects = _objects;
		HANDLE* pHandles = _Handles;
		do 
		{
			if (*ppObjects++ == pObject)
			{
				if (--nCount)
				{
					memcpy(ppObjects - 1, ppObjects, nCount * sizeof(PVOID));
					memcpy(pHandles, pHandles + 1, nCount * sizeof(PVOID));
				}
				_nCount--;
				return TRUE;
			}
		} while (pHandles++, --nCount);
	}

	return FALSE;
}

DWORD ZAppEx::GetWaitHandles(HANDLE ** ppHandles)
{
	*ppHandles = _Handles;
	return _nCount;
}

void ZAppEx::OnSignalObject(DWORD i)
{
	if (i < _nCount)
	{
		_objects[i]->OnSignal();
	}
}

void ZAppEx::OnAbandoned(DWORD i)
{
	if (i < _nCount)
	{
		_objects[i]->OnSignal();
	}
}

ZAppEx::ZAppEx()
{
	_nCount = 0;
}

ZAppEx::~ZAppEx()
{
}

//////////////////////////////////////////////////////////////////////////
// ZIdle
ZIdle::ZIdle()
{
	InitializeListHead(this);
}

ZIdle::~ZIdle()
{
	RemoveEntryList(this);
}

void ZIdle::Insert()
{
	if (ZGLOBALS* globals = ZGLOBALS::get())
	{
		globals->App->InsertIdle(this);
	}
}

void ZIdle::Remove()
{
	RemoveEntryList(this);
	InitializeListHead(this);
}

//////////////////////////////////////////////////////////////////////////
// ZFontNotify

ZFontNotify::ZFontNotify()
{
	InitializeListHead(this);
	if (ZGLOBALS* globals = ZGLOBALS::get())
	{
		InsertHeadList(&globals->_fontListHead, this);
	}
}

ZFontNotify::~ZFontNotify()
{
	RemoveEntryList(this);
}

//////////////////////////////////////////////////////////////////////////
// ZTranslateMsg

ZTranslateMsg::ZTranslateMsg()
{
	InitializeListHead(this);
}

ZTranslateMsg::~ZTranslateMsg()
{
	RemoveEntryList(this);
}

void ZTranslateMsg::Insert()
{
	if (ZGLOBALS* globals = ZGLOBALS::get())
	{
		globals->App->InsertTM(this);
	}
}

void ZTranslateMsg::Remove()
{
	RemoveEntryList(this);
	InitializeListHead(this);
}

//////////////////////////////////////////////////////////////////////////
// ZFont

STATIC_WSTRING(gszCourierNew, "Courier New");

ZFont::ZFont(BOOL bMain)
{
	__hfont = 0;
	_hfont = 0;
	_hStatusFont = 0;

	ZGLOBALS* globals = ZGLOBALS::get();

	if (bMain)
	{
		NONCLIENTMETRICS ncm = { GetNONCLIENTMETRICSWSize() };
		if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
		{
			memcpy(static_cast<PLOGFONT>(this), &ncm.lfMenuFont, sizeof(LOGFONT));
			lfHeight = -ncm.iMenuHeight;
		}
		else
		{
			lfHeight = 21;
			lfWidth = 0;
			lfEscapement = 0;
			lfOrientation = 0;
			lfCharSet = 0;
			lfOutPrecision = 0;
			lfClipPrecision = 0;
		}

		lfItalic = 0;
		lfUnderline = 0;
		lfStrikeOut = 0;
		lfWeight = FW_NORMAL;
		lfQuality = CLEARTYPE_QUALITY;
		lfPitchAndFamily = FIXED_PITCH|FF_MODERN;
		wcscpy(lfFaceName, gszCourierNew);

		if (globals)
		{
			globals->Font = this;
		}
	}
	else
	{
		if (globals)
		{
			if (PLOGFONT plf = globals->Font)
			{
				*static_cast<PLOGFONT>(this) = *static_cast<PLOGFONT>(plf);
			}
		}
	}
}

ZFont::~ZFont()
{
	if (ZGLOBALS* globals = ZGLOBALS::get())
	{
		if (globals->Font == this)
		{
			globals->Font = 0;
		}
	}

	if (_hStatusFont)
	{
		DeleteObject(_hStatusFont);
	}

	if (__hfont)
	{
		DeleteObject(__hfont);
	}

	if (_hfont)
	{
		DeleteObject(_hfont);
	}
}

HFONT ZFont::getStatusFont()
{
	if (!_hStatusFont)
	{
		NONCLIENTMETRICS ncm = { GetNONCLIENTMETRICSWSize() };
		if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
		{
			ncm.lfMessageFont.lfQuality = CLEARTYPE_QUALITY;
			ncm.lfMessageFont.lfPitchAndFamily = FIXED_PITCH|FF_MODERN;
			ncm.lfMessageFont.lfWeight = FW_NORMAL;
			wcscpy(ncm.lfMessageFont.lfFaceName, gszCourierNew);

			if (HFONT hfont = CreateFontIndirect(&ncm.lfMessageFont))
			{
				if (InterlockedCompareExchangePointer((void**)&_hStatusFont, (void*)hfont, 0))
				{
					DeleteObject(hfont);
				}
			}
		}
	}

	return _hStatusFont;
}

BOOL ZFont::CopyFont(ZFont* font)
{
	font->getLogfont(this);
	return SetNewFont(this, FALSE);
}

STATIC_UNICODE_STRING_(FontFaceName);
STATIC_UNICODE_STRING_(FontHeight);

BOOL ZFont::Init()
{
	if (ZRegistry* p = ZGLOBALS::getRegistry())
	{
		PKEY_VALUE_PARTIAL_INFORMATION pkvpi = (PKEY_VALUE_PARTIAL_INFORMATION)alloca(128);
		DWORD rcb;

		if (
			0 <= p->GetValue(&FontFaceName, KeyValuePartialInformation, pkvpi, 128, &rcb) 
			&& 
			IsRegSz(pkvpi)
			&&
			pkvpi->DataLength <= FIELD_SIZE(LOGFONT, lfFaceName)
			)
		{
			memcpy(lfFaceName, pkvpi->Data, pkvpi->DataLength);
		}

		if (
			0 <= p->GetValue(&FontHeight, KeyValuePartialInformation, pkvpi, 128, &rcb) 
			&& 
			pkvpi->Type == REG_DWORD 
			&&
			pkvpi->DataLength == sizeof(DWORD)
			)
		{
			int Data = *(PDWORD)pkvpi->Data, b = abs(Data), a = abs(lfHeight);

			if (a <= b && b < (a << 1))
			{
				lfHeight = Data;
			}
		}
	}

	return SetNewFont(this, FALSE);
}

BOOL ZFont::SetNewFont(PLOGFONT plf, BOOL bSave)
{
	LONG Width = plf->lfWidth;
	
	plf->lfWidth = 0;

	if (HFONT hfont = CreateFontIndirect(plf))
	{
		if (HDC hdc = GetDC(0))
		{
			HGDIOBJ o = SelectObject(hdc, hfont);
			TEXTMETRIC tm;
			if (GetTextMetrics(hdc, &tm))
			{
				plf->lfWidth = tm.tmAveCharWidth;
			}
			SelectObject(hdc, o);
			ReleaseDC(0, hdc);
		}

		if (plf->lfWidth)
		{
			if (_hfont)
			{
				DeleteObject(_hfont);
			}

			_hfont = hfont;

			if (plf != static_cast<PLOGFONT>(this))
			{
				memcpy(static_cast<PLOGFONT>(this), plf, sizeof(LOGFONT));
			}

			plf->lfUnderline = TRUE;

			if (HFONT hfont2 = CreateFontIndirect(plf))
			{
				if (__hfont)
				{
					DeleteObject(__hfont);
				}

				__hfont = hfont2;
			}

			plf->lfUnderline = FALSE;

			if (bSave)
			{
				if (ZRegistry* p = ZGLOBALS::getRegistry())
				{
					p->SetValue(&FontFaceName, REG_SZ, lfFaceName, (DWORD)(wcslen(lfFaceName) + 1) << 1);
					p->SetValue(&FontHeight, REG_DWORD, &lfHeight, sizeof(DWORD));
				}
			}

			if (ZGLOBALS* globals = ZGLOBALS::get())
			{
				if (globals->Font == this)
				{
					PLIST_ENTRY head = &globals->_fontListHead, entry = head;

					while ((entry = entry->Flink) != head)
					{
						static_cast<ZFontNotify*>(entry)->OnNewFont(hfont);
					}
				}
			}

			return TRUE;
		}

		DeleteObject(hfont);
	}

	plf->lfWidth = Width;

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
// ZGLOBALS

_ZGLOBALS::_ZGLOBALS()
{
	InitializeListHead(&_docListHead);
	InitializeListHead(&_fontListHead);
	App = 0, Reg = 0, MainFrame = 0, hwndMain = 0, Font = 0;
}

_ZGLOBALS::~_ZGLOBALS()
{
	if (!IsListEmpty(&_docListHead) || !IsListEmpty(&_fontListHead) || App || Reg || MainFrame || hwndMain || Font)
	{
		DbgBreak();
	}
}

ZAppEx* _ZGLOBALS::getApp()
{
	if (ZGLOBALS* globals = ZGLOBALS::get())
	{
		return globals->AppEx;
	}

	return 0;
}

class ZSDIFrameWnd* _ZGLOBALS::getMainFrame()
{
	if (ZGLOBALS* globals = ZGLOBALS::get())
	{
		return globals->MainFrame;
	}

	return 0;
}

HWND _ZGLOBALS::getMainHWND()
{
	if (ZGLOBALS* globals = ZGLOBALS::get())
	{
		return globals->hwndMain;
	}

	return 0;
}

ZRegistry* _ZGLOBALS::getRegistry()
{
	if (ZGLOBALS* globals = ZGLOBALS::get())
	{
		return globals->Reg;
	}

	return 0;
}

ZFont* _ZGLOBALS::getFont()
{
	if (ZGLOBALS* globals = ZGLOBALS::get())
	{
		return globals->Font;
	}

	return 0;
}

const VS_FIXEDFILEINFO* GetFileVersion(PCWSTR name)
{
	PVOID ImageBase;
	static LPCWSTR a[3] = { RT_VERSION, MAKEINTRESOURCE(1) };
	PIMAGE_RESOURCE_DATA_ENTRY pirde;
	DWORD size, wLength;

	struct VS_VERSIONINFO_HEADER {
		WORD             wLength;
		WORD             wValueLength;
		WORD             wType;
		WCHAR            szKey[];
	} *pv;

	if (
		(ImageBase = GetModuleHandle(name)) &&
		0 <= LdrFindResource_U(ImageBase, a, 3, &pirde) && 
		0 <= LdrAccessResource(ImageBase, pirde, (void**)&pv, &size) && 
		size > sizeof(*pv) &&
		(wLength = pv->wLength) <= size &&
		pv->wValueLength >= sizeof(VS_FIXEDFILEINFO)
		)
	{
		PVOID end = RtlOffsetToPointer(pv, wLength);
		PWSTR sz = pv->szKey;
		do 
		{
			if (!*sz++)
			{
				VS_FIXEDFILEINFO* pffi = (VS_FIXEDFILEINFO*)((3 + (ULONG_PTR)sz) & ~3);
				//RtlOffsetToPointer(pv, (RtlPointerToOffset(pv, sz) + 3) & ~3);
				return RtlPointerToOffset(pffi, end) < wLength && pffi->dwSignature == VS_FFI_SIGNATURE ? pffi : 0;
			}
		} while (sz < end);
	}

	return 0;
}

_NT_END