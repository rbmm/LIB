#pragma once

#include "../inc/rtlframe.h"
#include "winZ.h"

extern "C"
{
	WINZ_API PVOID* __fastcall findPVOID(SIZE_T, PVOID*, PVOID);
	WINZ_API DWORD* __fastcall findDWORD(SIZE_T, DWORD*, DWORD);
	WINZ_API WORD* __fastcall findWORD(SIZE_T, WORD*, WORD);

	WINZ_API PWSTR __fastcall wtrnstr(SIZE_T n1, const void* str1, SIZE_T n2, const void* str2);
	WINZ_API PWSTR __fastcall wtrnchr(SIZE_T n1, const void* str1, WCHAR c);
	WINZ_API PSTR __fastcall strnstr(SIZE_T n1, const void* str1, SIZE_T n2, const void* str2);
	WINZ_API PSTR __fastcall strnchr(SIZE_T n1, const void* str1, char c);
}

#define _strnstr(a, b, x) strnstr(RtlPointerToOffset(a, b), a, sizeof(x) - 1, x)
#define _strnstrL(a, b, x) strnstr(RtlPointerToOffset(a, b), a, strlen(x), x)
#define _strnchr(a, b, c) strnchr(RtlPointerToOffset(a, b), a, c)
#define _strnstrS(a, b, s, x) strnstr(RtlPointerToOffset(a, b), a, s, x)

#define LP(str) RTL_NUMBER_OF(str) - 1, str

class WINZ_API __declspec(novtable) ZTranslateMsg : LIST_ENTRY
{
public:
	virtual BOOL PreTranslateMessage(PMSG lpMsg) = 0;
	
	ZTranslateMsg();
	~ZTranslateMsg();

	void Insert();
	void Remove();
};

class WINZ_API __declspec(novtable) ZIdle : LIST_ENTRY
{
public:
	virtual void OnIdle() = 0;

	ZIdle();
	~ZIdle();

	void Insert();
	void Remove();
};

class WINZ_API __declspec(novtable) ZFontNotify : LIST_ENTRY
{
public:
	virtual void OnNewFont(HFONT hFont) = 0;

	ZFontNotify();
	~ZFontNotify();
};

class __declspec(novtable) ZSignalObject
{
public:
	virtual void OnSignal() = 0;
	virtual void OnAbandoned();
};

class WINZ_API ZApp
{
	LIST_ENTRY _headTM, _headIdle;
	
	BOOL PreTranslateMessage(PMSG lpMsg);

protected:

	virtual DWORD GetWaitHandles(HANDLE ** ppHandles);
	virtual DWORD GetTimeout();
	virtual void OnSignalObject(DWORD i);
	virtual void OnAbandoned(DWORD i);
	virtual void OnApcAlert();
	virtual void OnTimeout();
	virtual void OnIdle();
	virtual BOOL IsIdleMessage(UINT uMsg);

public:

	ZApp();
	~ZApp();

	void InsertTM(PLIST_ENTRY entry);
	void InsertIdle(PLIST_ENTRY entry);

	virtual BOOL CanClose(HWND hwnd);

	void Run();
};

class WINZ_API ZAppEx : public ZApp
{
public:
	ZAppEx();
	~ZAppEx();
	BOOL addWaitObject(ZSignalObject* pObject, HANDLE hObject);
	BOOL delWaitObject(ZSignalObject* pObject);
protected:
	virtual DWORD GetWaitHandles(HANDLE ** ppHandles);
	virtual void OnSignalObject(DWORD i);
	virtual void OnAbandoned(DWORD i);
private:

	HANDLE _Handles[MAXIMUM_WAIT_OBJECTS - 1];
	ZSignalObject* _objects[MAXIMUM_WAIT_OBJECTS - 1];
	DWORD _nCount;
};

struct WINZ_API ZRegistry
{
	HANDLE _hKey;

	ZRegistry();

	~ZRegistry();

	NTSTATUS Open(PHANDLE KeyHandle, PCUNICODE_STRING SubKey);
	
	NTSTATUS Create(PHANDLE KeyHandle, PCUNICODE_STRING SubKey);

	NTSTATUS SetValue(PCUNICODE_STRING ValueName, ULONG Type, PVOID Data, ULONG DataSize)
	{
		return ZwSetValueKey(_hKey, ValueName, 0, Type, Data, DataSize);
	}

	NTSTATUS GetValue(PCUNICODE_STRING ValueName, KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass, PVOID KeyValueInformation, ULONG Length, PULONG ResultLength)
	{
		return ZwQueryValueKey(_hKey, ValueName, KeyValueInformationClass, KeyValueInformation, Length, ResultLength);
	}

	NTSTATUS Open(PCWSTR path);

	NTSTATUS Create(PCWSTR path);

	NTSTATUS SaveRect(PCUNICODE_STRING Name, PRECT prc);
	
	NTSTATUS LoadRect(PCUNICODE_STRING Name, PRECT prc);

	void SaveWinPos(PCUNICODE_STRING Name, HWND hwnd);

	void LoadWinPos(PCUNICODE_STRING Name, int& x, int& y, int& width, int& height, DWORD& dwStyle);
};

class WINZ_API ZFont : protected LOGFONT
{
	HFONT _hfont, __hfont, _hStatusFont;

public:

	ZFont(BOOL bMain);
	
	~ZFont();

	BOOL CopyFont(ZFont* font);

	BOOL SetNewFont(PLOGFONT plf, BOOL bSave);

	BOOL Init();

	void getLogfont(PLOGFONT plf)
	{
		memcpy(plf, static_cast<PLOGFONT>(this), sizeof(LOGFONT));
	}

	HFONT getStatusFont();

	HFONT getFont()
	{
		return _hfont;
	}

	HFONT _getFont()
	{
		return __hfont;
	}

	void getSIZE(PSIZE ps)
	{
		ps->cx = lfWidth, ps->cy = abs(lfHeight);
	}
};

struct WINZ_API _ZGLOBALS
{
	LIST_ENTRY _fontListHead, _docListHead;
	union
	{
		ZApp* App;
		ZAppEx* AppEx;
	};
	class ZSDIFrameWnd* MainFrame;
	ZRegistry* Reg;
	ZFont* Font;
	HWND hwndMain;

	_ZGLOBALS();
	~_ZGLOBALS();

	static ZAppEx* getApp();
	static class ZSDIFrameWnd* getMainFrame();
	static HWND getMainHWND();
	static ZFont* getFont();
	static ZRegistry* getRegistry();
};

typedef RTL_FRAME<_ZGLOBALS> ZGLOBALS;

inline BOOL IsRegSz(PKEY_VALUE_PARTIAL_INFORMATION pkvpi)
{
	ULONG DataLength = pkvpi->DataLength;
	return pkvpi->Type == REG_SZ && DataLength && !(DataLength & 1) && 
		!(PWSTR)RtlOffsetToPointer(pkvpi->Data, DataLength)[-1];
}

WINZ_API const VS_FIXEDFILEINFO* GetFileVersion(PCWSTR name);

WINZ_API ULONG GetNONCLIENTMETRICSWSize();