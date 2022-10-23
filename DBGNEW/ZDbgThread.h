#pragma once

#include "../winZ/ctrl.h"
#include "../winZ/Frame.h"
#include "../winZ/subclass.h"
#include "common.h"

class ZDbgDoc;

class ZDbgThread : public LIST_ENTRY
{
	friend ZDbgDoc;

	enum STATE : USHORT {
		stF4, stF5, stF11, 
	};

	enum {
		stSuspended = 16, stBP
	};

	HANDLE _hThread;
	PVOID _lpThreadLocalBase;
	ULONG_PTR _Dr[4];
	PVOID _Ctx[4];
	PVOID _pbpVa;
	ULONG_PTR _Va;
	DWORD _dwThreadId;
	union {
		STATE _state; // [0..15]
		LONG _flags; // [16..31]
	};
	DWORD _len;
	BOOL _bStepOver;

	ZDbgThread(DWORD dwThreadId, HANDLE hThread, PVOID lpThreadLocalBase);
	~ZDbgThread();
public:
	PVOID GetThreadLocalBase()
	{
		return _lpThreadLocalBase;
	}

	NTSTATUS GetContext(PCONTEXT ctx)
	{
		return ZwGetContextThread(_hThread, ctx);
	}

#ifdef _WIN64
	NTSTATUS GetWowCtx(PWOW64_CONTEXT x86_ctx)
	{
		return ZwQueryInformationThread(_hThread, ThreadWow64Context, x86_ctx, sizeof(WOW64_CONTEXT), 0);
	}
#endif
	ULONG getID() { return _dwThreadId; }
};

class ZTabFrame : public ZFrameMultiWnd, ZTabBar
{
	HWND _hwndTH, _hwndST, _hwndCur;
	ZDbgDoc* _pDoc;
	UINT_PTR _Stack;
	BOOL _bTempHide;

	virtual LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	virtual BOOL CreateClient(HWND hWndParent, int nWidth, int nHeight, PVOID /*lpCreateParams*/);

	virtual PCUNICODE_STRING getPosName();

	void ShowStackTrace(ZDbgThread* pThread, bool bKernel);

public:
	void AddThread(DWORD dwThreadId, PVOID lpThreadLocalBase, PVOID lpStartAddress);

	void DelThread(DWORD dwThreadId);

	void ShowStackTrace(CONTEXT& ctx);

	void TempHide(BOOL bShow);

	ZTabFrame(ZDbgDoc* pDoc)
	{
		_pDoc = pDoc;
		_bTempHide = FALSE;
	}
};

BOOL CreateDbgTH(ZTabFrame** pp, ZDbgDoc* pDoc);

class ZTraceView : public ZSubClass, ZDetachNotify
{
	HTREEITEM		_Nodes[64];
	ULONG_PTR		_HighLevelStack[64];	
	ULONG_PTR		_LastStack;
	ULONG_PTR		_LastPC;
	PVOID			_ExceptionAddress;
	ZDbgDoc*		_pDoc;
	int				_Level;
	DWORD			_dwThreadId;
	DWORD			_id;
	BOOLEAN			_bStop;

	virtual LRESULT WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	virtual void OnDetach();

	void AddReport(ULONG_PTR Pc, ULONG_PTR From, DWORD dw = MAXDWORD);

	~ZTraceView();

public:

	void AddFirstReport();
	NTSTATUS OnException(CONTEXT* ctx);

	ZTraceView(ZDbgDoc* pDoc, DWORD dwThreadId, CONTEXT* ctx);

	void SetExeceptionAddress(PVOID ExceptionAddress) { _ExceptionAddress = ExceptionAddress; }

	DWORD getID() { return _dwThreadId; }
};
