#pragma once

#include "../inc/rtlframe.h"
#include "winZ.h"
#include "str.h"

class WINZ_API __declspec(novtable) ZTranslateMsg : public LIST_ENTRY
{
public:
	virtual BOOL PreTranslateMessage(PMSG lpMsg) = 0;
	
	ZTranslateMsg();
	~ZTranslateMsg();

	void Insert();
	void Remove();
};

class WINZ_API __declspec(novtable) ZIdle : public LIST_ENTRY
{
public:
	virtual void OnIdle() = 0;

	ZIdle();
	~ZIdle();

	void Insert();
	void Remove();
};

class WINZ_API __declspec(novtable) ZFontNotify : public LIST_ENTRY
{
public:
	virtual void OnNewFont(HFONT hFont) = 0;

	ZFontNotify();
	~ZFontNotify();
};

class __declspec(novtable) ZSignalObject
{
public:
	virtual ~ZSignalObject() = default;
	virtual void OnSignal() = 0;
	virtual void OnAbandoned();
	virtual void OnStop();
};

class WINZ_API ZApp
{
	LIST_ENTRY _headTM, _headIdle;
	
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
	BOOL PreTranslateMessage(PMSG lpMsg);

	virtual BOOL CanClose(HWND hwnd);

	WPARAM Run();
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

	BOOL SaveWinPos(PCUNICODE_STRING Name, HWND hwnd);

	BOOL LoadWinPos(PCUNICODE_STRING Name, int& x, int& y, int& width, int& height, DWORD& dwStyle);
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

extern ULONG gWinVersion;