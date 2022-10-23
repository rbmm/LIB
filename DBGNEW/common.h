#pragma once
#define USER_SHARED_DATA ((PKUSER_SHARED_DATA)0x7ffe0000)

#define ID_PATH				IDB_BITMAP3
#define ID_KILL				IDB_BITMAP27
#define ID_DETACH			IDB_BITMAP9
#define ID_VMOP				IDB_BITMAP13
#define ID_HANDLES			IDB_BITMAP14
#define ID_MEMMAP			IDB_BITMAP15
#define ID_MODULES			IDB_BITMAP22
#define ID_BREAKPOINTS		IDB_BITMAP10
#define ID_BACK				IDB_BITMAP8
#define ID_DISASM			IDB_BITMAP21
#define ID_DBGTH			IDB_BITMAP23
#define ID_TRACE			IDB_BITMAP6
#define ID_PRFLT			IDB_BITMAP24
#define ID_TOOLS			IDB_BITMAP25
#define ID_DBGFLAGS			IDB_BITMAP4
#define ID_PIPE				IDB_BITMAP5

#define COLOR_ACTIVE_LINK	RGB(210,105,30)
#define COLOR_ACTIVE_LINK2	RGB(120,240,80)
#define COLOR_BP			RGB(190,61,55)
#define COLOR_PCBP			RGB(255,128,0)
#define COLOR_PC			RGB(255,255,0)
#define COLOR_NORMAL		RGB(255,255,255)
#define COLOR_NAME			RGB(0,0,255)
#define COLOR_TEXT			RGB(0,0,0)

#ifdef _WIN64
#define Xsp Rsp
#define Xip Rip
#define Xbp Rbp
#define XFrame Rsp
#else
#define Xsp Esp
#define Xip Eip
#define Xbp Ebp
#define XFrame Ebp
#endif

enum PR {
	prDLL, prThread, prException, prDbgPrint, prGen
};

inline BOOL ifRegSz(PKEY_VALUE_PARTIAL_INFORMATION pkvpi)
{
	return (pkvpi->Type == REG_SZ || pkvpi->Type == REG_EXPAND_SZ) &&
		pkvpi->DataLength >= sizeof WCHAR &&
		!(pkvpi->DataLength & 1) &&
		!*(PWSTR)(pkvpi->Data + pkvpi->DataLength - sizeof(WCHAR));
}

class __declspec(novtable) ZDetachNotify : public LIST_ENTRY
{
public:
	virtual void OnDetach() = 0;
};

#define _UDT_

#include "../winZ/app.h"
#include "../winz/Frame.h"
#include "../winz/mdi.h"
#include "eval64.h"
#include "addressview.h"
#include "DbgDoc.h"
#include "resource.h"
#include "unDName.h"
#include "../tkn/tkn.h"
#include "../inc/idcres.h"
#include "JsScript.h"

#ifdef _WIN64
#define __stosp __stosq
#else
#define __stosp __stosd
#endif

extern volatile const UCHAR guz;
extern OBJECT_ATTRIBUTES zoa;
extern HANDLE g_hDrv;
extern USHORT g_OSversion;
extern UCHAR g_ThreadIndex, g_ProcessIndex, g_FileIndex;
extern LONG g_printMask;
extern HIMAGELIST g_himl16;
extern ZImageList g_IL20;
extern ULONG g_dwGuiThreadId;
//extern HWND g_hwndMain;

PSTR xcscpy(PSTR dst, PCSTR src);
PWSTR xcscpy(PWSTR dst, PCWSTR src);

void CMainDlg_Create(HWND hwndParent);

void ShowProcessMemory(DWORD dwProcessId);
void ShowProcessMemory(HANDLE hProcess, DWORD dwProcessId);
void ShowProcessHandles(HANDLE hProcess, DWORD dwProcessId);
void ShowProcessHandles(DWORD dwProcessId);
void SetAndAnimate(HWND hwnd, PCWSTR sz);
void ShowThreadWindows(DWORD dwThreadId);

void CreateMemoryWindow(ZDbgDoc* pDocument);
HWND CreateAsmWindow(ZDbgDoc* pDocument);
UINT_PTR strtoui64(const char * sz, const char ** psz);

NTSTATUS SymReadMemory(ZDbgDoc* pDoc, PVOID BaseAddres, PVOID Buffer, SIZE_T BufferLength, PSIZE_T ReturnLength = 0);
//NTSTATUS DbgReadMemory(HANDLE ProcessHandle, PVOID BaseAddres, PVOID Buffer, SIZE_T BufferLength, PSIZE_T ReturnLength = 0);
void ShowNTStatus(HWND hwnd, NTSTATUS status, PCWSTR caption);
NTSTATUS MyOpenProcess(PHANDLE ProcessHandle, ULONG DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PCLIENT_ID Cid);
NTSTATUS MyOpenThread(PHANDLE ThreadHandle, ULONG DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PCLIENT_ID Cid);
NTSTATUS MySetContextThread ( IN HANDLE ThreadHandle, IN _CONTEXT* Context );
void ShowText(PCWSTR caption, PCWSTR text);
NTSTATUS SetDebugInherit(HANDLE hProcess, BOOLEAN bInherit);
NTSTATUS IsDebugInherit(HANDLE hProcess, BOOLEAN& bInherit);

class ZExceptionFC 
{
	DWORD _nExtra;
	union
	{
		LONG _bits;
		struct  
		{
			LONG _default : 1;
			LONG _SINGLE_STEP : 1;
			LONG _BREAKPOINT : 1;
			LONG _DATATYPE_MISALIGNMENT : 1;
			LONG _GUARD_PAGE_VIOLATION : 1;
			LONG _ACCESS_VIOLATION : 1;
			LONG _INTEGER_DIVIDE_BY_ZERO : 1;
			LONG _INTEGER_OVERFLOW : 1;
			LONG _ILLEGAL_INSTRUCTION : 1;
			LONG _PRIVILEGED_INSTRUCTION : 1;
			LONG _STACK_OVERFLOW : 1;
			LONG _spare : 21;
		};
	};

	DWORD _status[21];
protected:
	ZExceptionFC();
public:
	void Reset();
	void Load();
	LONG get(PDWORD nExtra, DWORD status[]);
	void Save(LONG bits, DWORD nExtra, DWORD status[]);

	DWORD getExtraN() { return _nExtra; }
	NTSTATUS getStatus(DWORD i) { return _status[i]; }

	BOOL StopOnFC(NTSTATUS status);
};

class ZMyApp : public ZAppEx, ZSignalObject, public ZExceptionFC
{
	virtual void OnSignal();
public:
	BOOL Init();
	~ZMyApp();
};

class CDbgPipe;

struct GLOBALS_EX : ZGLOBALS
{
	GLOBALS_EX();
	~GLOBALS_EX();

	PWSTR _NtSymbolPath;
	JsScript* _pScript;
	CDbgPipe* _pipe;

	BOOL Init();
	BOOL SetPath(PCWSTR NtSymbolPath);
	BOOL SetPathNoReg(PCWSTR NtSymbolPath);
	PCWSTR getPath();

	JsScript* getScript();
	static JsScript* _getScript();
};

#ifdef _WIN64
#define uptoul _wcstoui64
#else
#define uptoul wcstoul
#endif

#ifdef _UDT_
void AddWatch(ZDbgDoc* pDoc);
#endif

NTSTATUS DoIoControl(ULONG code);

NTSTATUS LocatePdb(HANDLE hProcess, PVOID ImageBase, PGUID Signature, PULONG Age, PWSTR* ppdbPath);

void StopDebugPipe(CDbgPipe* p);
BOOL StartDebugPipe(CDbgPipe** pp);