#pragma once

#include "../winz/app.h"
#include "memorycache.h"
#include "AsmView.h"
#include "regview.h"
#include "Dll.h"
#include "zdlgs.h"
#include "ZDbgThread.h"
#include "SrcFile.h"
#include "kdbg.h"

#define BYTE_UPDATED 0
#define ALL_UPDATED 1
#define DLL_UNLOADED 2

void ShowDumpProcesses(ULONG id, PVOID UdtCtx, IMemoryDump* pDump);

struct CV_DebugSFile;

class ZDbgDoc;

struct ZBreakPoint : LIST_ENTRY
{
	PVOID _Va;
	PWSTR _expression;
	DWORD _dllId, _rva;
	DWORD _HitCount;
	DWORD _SuspendCount;
	BYTE _opcode;
	BOOLEAN _isActive;
	BOOLEAN _isUsed;
	BOOLEAN _isSuspended;

	ZBreakPoint()
	{
		_SuspendCount = 0, _HitCount = 0, _isSuspended = FALSE, _expression = 0;
	}

	~ZBreakPoint()
	{
		if (_expression)
		{
			delete [] _expression;
		}
	}
};

struct BOL : LIST_ENTRY 
{
	WCHAR _name[];

	BOL()
	{

	}

	BOL(PCWSTR name)
	{
		wcscpy(_name, name);
	}

	void* operator new (size_t cb, SIZE_T ex)
	{
		return ::operator new(cb + ex);
	}

	void* operator new(size_t , PCWSTR name)
	{
		return new((wcslen(name) + 1) << 1) BOL(name);
	}

	void operator delete(void* p)
	{
		return ::operator delete(p);
	}
};

class ZAsmView;
class ZRegView;
class ZTabFrame;
class ZDbgThread;
class ZTraceView;
class ZExceptionFC;

class __declspec(uuid("AD893927-328E-4d71-B19C-6E90A88E4CBC")) ZDbgDoc : 
	public ZDocument, 
	public ZMemoryCache, 
	ZSignalObject 
{
	friend class ZBPDlg;

	struct RemoteData 
	{
		PVOID KernBase, PsLoadedModuleList;
	};

	LIST_ENTRY _dllListHead, _threadListHead, _bpListHead, _notifyListHead, _bolListHead, _srcListHead;
	HANDLE _hKey;

	union {
		PVOID _PebBaseAddress;
		PWSTR _NtSymbolPath;
	};
#ifdef _WIN64
	PVOID _wowPeb;
#endif
	RemoteData* _pRemoteData;
	ZAsmView* _pAsm;
	ZRegView* _pReg;
	ZTabFrame* _pDbgTH;
	ZDbgThread* _pThread;
	union {
		ZTraceView* _pTraceView;
		PVOID _pUdtCtx;// for dump|remote debugger 
	};
	ZExceptionFC* _pFC;
	HWND _hwndLog, _hwndDLLs, _hwndBPs;
	DWORD _nDllCount, _nLoadIndex, _dwProcessId, _SuspendCount, _KernelStackOffset;

	union {
		LONG _Flags;
		struct 
		{
			ULONG _IsInserted : 1;
			ULONG _IsWow64Process : 1;
			ULONG _IsDebugger : 1;
			ULONG _IsTerminated : 1;
			ULONG _IsWaitContinue : 1;
			ULONG _IsInTrace : 1;
			ULONG _IsDump : 1;
			ULONG _IsDetachCalled : 1;
			ULONG _IsDbgInherit : 1;
			ULONG _IsRemoteDebugger : 1;
			ULONG _IsLocalMemory : 1;
			ULONG _IsUdtTry : 1;
			ULONG _IsAttached : 1;
			ULONG _SpareBits : 19;
		};
	};

	virtual void OnSignal();
	virtual LRESULT OnCmdMsg(WPARAM wParam, LPARAM lParam);
	virtual BOOL IsCmdEnabled(WORD cmd);
	virtual void OnActivate(BOOL bActivate);

	BOOL Create();

	~ZDbgDoc();

	BOOL _DbgContinue(CONTEXT& ctx, int key, INT_PTR Va = 0);

	BOOL _DbgContinue(CONTEXT& ctx, ULONG_PTR Va, int len, BOOL bStepOver);

	void Detach();

	void PrintException(DWORD dwThreadId, NTSTATUS ExceptionCode, PVOID ExceptionAddress, ULONG NumberParameters, PULONG_PTR ExceptionInformation, PCSTR Chance);

	void SuspendOrResumeAllThreads(BOOL bSuspend, ZDbgThread* pCurrentThread);

	BOOL SuspendBp(ZBreakPoint* pbp);

	BOOL ResumeBp(ZBreakPoint* pbp);

	void DestroyDbgTH();

	void SetDbgFlags();

	void SetDbgFlags(ZSDIFrameWnd* pFrame);

public:

	BOOL InTrace()
	{
		return _IsInTrace;
	}

	ULONG GetKernelStackOffset()
	{
		return _KernelStackOffset;
	}

	ULONG SetKernelStackOffset(LONG KernelStackOffset)
	{
		_KernelStackOffset = KernelStackOffset;
		return KernelStackOffset;
	}

	BOOL Is64BitProcess()
	{
#ifdef _WIN64
		return !_IsWow64Process;
#else
		return FALSE;
#endif
	}

#ifdef  _WIN64
	BOOL IsWow64Process()
	{
		return _IsWow64Process;
	}
#endif

	void ShowProcessList()
	{
		ShowDumpProcesses(_dwProcessId, _pUdtCtx, _pDump);
	}

	ULONG64 get_DumpBlobSize()
	{
		return _pDump->get_DumpBlobSize();
	}

	void ShowFrameContext(ULONG i);

	PVOID getUdtCtxEx();

	PVOID getUdtCtx()
	{
		return _IsLocalMemory ? 0 : _pUdtCtx;
	}

	PCWSTR NtSymbolPath()
	{
		return _IsDump ? _NtSymbolPath : 0;
	}
	
	BOOL GetValidRange(INT_PTR Address, INT_PTR& rLo, INT_PTR& rHi);

	NTSTATUS Read(PVOID RemoteAddress, PVOID buf, DWORD cb, PSIZE_T pcb = 0);

	NTSTATUS Write(PVOID RemoteAddress, UCHAR c);

	NTSTATUS OpenDumpComplete();

	NTSTATUS OpenDump(PCWSTR FileName, PWSTR NtSymbolPath);

	virtual HRESULT QI(REFIID riid, void **ppvObject);

	void Rundown();

	void UpdateThreads(PSYSTEM_PROCESS_INFORMATION pspi);

	NTSTATUS Attach(DWORD dwProcessId);

	BOOL OnRemoteStart(CDbgPipe* pipe, _DBGKD_GET_VERSION* GetVersion);
	
	NTSTATUS Create(PCWSTR lpApplicationName, PWSTR lpCommandLine);

	ZDbgDoc(BOOL IsDebugger);

	DWORD getId() { return _dwProcessId; }
	
	DWORD getThreadId();

	HANDLE getProcess() { return _hProcess; }

	BOOLEAN IsWaitContinue() { return _IsWaitContinue; }

	BOOLEAN IsCurrentThread(ZDbgThread* pThread) { return _IsWaitContinue && _pThread == pThread; }

	BOOLEAN IsDebugger() { return _IsDebugger; }

	BOOLEAN IsAttached() { return _IsAttached; }

	BOOLEAN IsLocalMemory() { return _IsLocalMemory; }
	
	BOOLEAN IsRemoteDebugger() { return _IsRemoteDebugger; }

	BOOLEAN IsDump() { return _IsDump; }

	static ZDbgDoc* find(DWORD dwProcessId);

	void SetAsm(ZAsmView* pAsm);

	void SetModDlg(HWND hwndDLLs);
	
	void SetBPDlg(HWND hwndBPs);

	BOOL IsRegVisible();

	BOOL IsDbgWndVisible();
	
	BOOL IsDbgWndExist()
	{
		return _pDbgTH != 0;
	}
	
	void ShowReg();

	void ShowDbgWnd();

	BOOL GoTo(PVOID Va, BOOL bGotoSrc = TRUE);

	INT_PTR getPC();

	void setPC(INT_PTR Va);

	void setAsmPC(INT_PTR Va) { _pAsm->setPC(Va); }

	PCONTEXT getContext() { return _IsWaitContinue ? _pReg : 0; }

	void DbgContinue(int key, INT_PTR Va = 0);

	void DbgContinue(ULONG_PTR Va, int len, BOOL bStepOver);

	//////////////////////////////////////////////////////////////////////////
	// ZSrcFile

	ZSrcFile* findSrc(ZView* pView);

	ZSrcFile* findSrc(CV_DebugSFile* fileId);

	ZSrcFile* openSrc(CV_DebugSFile* fileId);

	void RemoveSrcPC();

	//////////////////////////////////////////////////////////////////////////
	// BOL

	BOOL isBOL(PCWSTR name);

	void SetBOLText(HWND hwnd, UINT id);

	void CreateBOL(PWSTR names);

	void addBOL(PCWSTR name);

	void LoadBOL();

	void SaveBOL();

	void deleteBOL();

	//////////////////////////////////////////////////////////////////////////
	// DEBUG_EVENT

	NTSTATUS OnException(DWORD dwThreadId, PEXCEPTION_RECORD ExceptionRecord, BOOL dwFirstChance, CONTEXT* ctx);

	void OnDebugEvent(DBGUI_WAIT_STATE_CHANGE& StateChange);
	
	void OnDbgPrint(SIZE_T cch, PVOID pv, BOOL bWideChar);

	BOOL Load(PDBGKM_LOAD_DLL LoadDll, BOOL bExe);

	void OnUnloadDll(PVOID lpBaseOfDll);

	NTSTATUS OnLoadDll(DWORD dwThreadId, PDBGKM_LOAD_DLL LoadDll, BOOL bExe, CONTEXT* ctx);

	void OnCreateThread(DWORD dwThreadId, PDBGUI_CREATE_THREAD CreateThreadInfo);

	void OnExitThread(DWORD dwThreadId, DWORD dwExitCode);

	BOOL OnCreateProcess(DWORD dwProcessId, DWORD dwThreadId, PDBGUI_CREATE_PROCESS CreateProcessInfo);

	void OnExitProcess(DWORD dwExitCode);

	//////////////////////////////////////////////////////////////////////////
	//

	NTSTATUS RemoteGetContext(WORD Processor, CONTEXT* ctx);
	NTSTATUS OnRemoteLoadUnload(DBGKD_WAIT_STATE_CHANGE* pwsc, CONTEXT& ctx);
	NTSTATUS OnRemoteException(ZDbgThread* pThread, DBGKD_WAIT_STATE_CHANGE* pwsc, CONTEXT& ctx);
	NTSTATUS OnWaitStateChange(DBGKD_WAIT_STATE_CHANGE* pwsc);

	BOOL IsRemoteWait();

	void OnRemoteEnd();

	void Break();
	//////////////////////////////////////////////////////////////////////////
	// log
	
	void cprintf(PR pr, PCWSTR buf);

	void vprintf(PR pr, PCWSTR format, va_list args);
	
	void printf(PR pr, PCWSTR format, ...){ return vprintf(pr, format, (va_list)(&format + 1)); }

	//////////////////////////////////////////////////////////////////////////
	// notify

	void AddNotify(ZDetachNotify* p);

	static void RemoveNotify(ZDetachNotify* p);

	void FireNotify();

	//////////////////////////////////////////////////////////////////////////
	// Dlls

	ZDll* getDllByBaseNoRefNoParse(PVOID lpBaseOfDll);

	ZDll* getDllByVaNoRef(PVOID Va);

	ZDll* getDllByPathNoRef(PCWSTR path);

	PVOID getVaByName(PCSTR name);

	BOOL getDllByVa(ZDll** ppDll, PVOID Va);

	PCSTR getNameByVa(PVOID Va);
	
	PCWSTR getNameByID(DWORD id);

	//////////////////////////////////////////////////////////////////////////
	// ZDbgThread

	ZDbgThread* getThreadById(DWORD dwThreadId);

	void ScrollAsmUp(){ _pAsm->OnScroll(_pAsm->getHWND(), SB_VERT, SB_LINEUP); }

	void StopTrace();

	//////////////////////////////////////////////////////////////////////////
	// bp

	ZBreakPoint* getBpByVa(PVOID Va);

	void DeleteAllBps();

	void EnableAllBps(BOOL bEnable);

	BOOL EnableBp(ZBreakPoint* pbp, BOOL bEnable, BOOL bSendNotify);

	BOOL EnableBp(PVOID Va, BOOL bEnable);

	BOOL ToggleBp(PVOID Va);

	BOOL AddBp(PVOID Va);

	BOOL DelBp(PVOID Va);

	BOOL DelBp(ZBreakPoint* pbp);

	BOOL SetBpCondition(ZBreakPoint* pbp, PCWSTR exp, ULONG Len);

	BOOL SetBpCondition(PVOID Va, PCWSTR exp, ULONG Len);

	void OnLoadUnload(PVOID DllBase, DWORD id, DWORD size, BOOL bLoad);

	void SaveBps();
	
	void LoadBps();

	void SuspendAllBps(BOOL bSuspend);

	void InvalidateVa(INT_PTR Va, ZAsmView::ACTION action);

	//////////////////////////////////////////////////////////////////////////
	// Unwind

	BOOL DoUnwind(CONTEXT& ctx);

	//////////////////////////////////////////////////////////////////////////
	// reg

	NTSTATUS SetValue(PCUNICODE_STRING ValueName, ULONG Type, PVOID Data, ULONG DataSize)
	{
		return ZwSetValueKey(_hKey, ValueName, 0, Type, Data, DataSize);
	}

	NTSTATUS GetValue(PCUNICODE_STRING ValueName, KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass, PVOID KeyValueInformation, ULONG Length, PULONG ResultLength)
	{
		return ZwQueryValueKey(_hKey, ValueName, KeyValueInformationClass, KeyValueInformation, Length, ResultLength);
	}

	//////////////////////////////////////////////////////////////////////////
	// Dump

	void LoadKernelModules();
	void BuildProcessList(ULONG SizeEProcess);
#ifdef _WIN64
	void LoadKernelModulesWow();
	void BuildProcessListWow(ULONG SizeEProcess);
#endif
};

#define GetDbgDoc() ((ZDbgDoc*)_pDocument)

BOOL CreatePrivateUDTContext(ZDbgDoc* pDoc, PCWSTR NtSymbolPath, PVOID KernelBase, void** ppv);
void DeletePrivateUDTContext(PVOID pv);
