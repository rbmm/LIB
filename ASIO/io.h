#pragma once

EXTERN_C_START
#include "../inc/nttp.h"
EXTERN_C_END
#include "../inc/rundown.h"
#include "../inc/asmfunc.h"
#include "packet.h"
#include "blockheap.h"

void ReferenceDll()ASM_FUNCTION;
void DereferenceDll()ASM_FUNCTION;

#ifdef _X86_
#define VSIFN "YGXXZ"
#else
#define VSIFN "YAXXZ"
#endif

__pragma(comment(linker, "/alternatename:@?FastReferenceDll=@?FastReferenceDllNopa" ))
__pragma(comment(linker, "/alternatename:?ReferenceDll@NT@@" VSIFN "=@?FastReferenceDllNopa" ))
__pragma(comment(linker, "/alternatename:?DereferenceDll@NT@@" VSIFN "=@?FastReferenceDllNopa" ))

NTSTATUS NTAPI SkipCompletionOnSuccess(HANDLE hObject);

struct IO_RUNDOWN : public RUNDOWN_REF 
{
	static IO_RUNDOWN g_IoRundown;
protected:
	virtual void RundownCompleted();
private:
	void RundownCompletedNop();
};

class __declspec(novtable) IO_OBJECT 
{
	friend class IO_IRP;
	friend class NT_IRP;

	NTSTATUS BindIoCompletionCB(HANDLE hFile, PTP_IO_CALLBACK Callback);	

	virtual void IOCompletionRoutine(CDataPacket* packet, DWORD Code, NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PVOID Pointer) = 0;

	HANDLE				m_hFile = 0;
	PTP_IO				m_Io = 0;
	RundownProtection	m_HandleLock;
	LONG				m_nRef = 1;

	_NODISCARD BOOL LockHandle()
	{
		return m_HandleLock.Acquire();
	}

	void StartIo()
	{
		AddRef();
		if (m_Io) TpStartAsyncIoOperation(m_Io);
	}

protected:

	IO_OBJECT()
	{
		ReferenceDll();
	}

	virtual ~IO_OBJECT()
	{
		if (m_Io)
		{
			TpReleaseIoCompletion(m_Io);
		}
		Close();
		DereferenceDll();
	}

	virtual void CloseObjectHandle(HANDLE hFile)
	{
		if (hFile) NtClose(hFile);
	}

public:

	static inline PTP_POOL _G_Pool = 0;

	_NODISCARD BOOL LockHandle(HANDLE& hFile);

	void UnlockHandle();

	HANDLE getHandleNoLock() { return m_hFile; }

	void operator delete(void* p);

	void operator delete[](void* p)
	{
		delete p;
	}

	void* operator new(size_t cb);

	_NODISCARD BOOL AddRefEx()
	{
		return ObpLock(&m_nRef);
	}

	void AddRef()
	{
		InterlockedIncrementNoFence(&m_nRef);
	}

	void Release()
	{
		if (!InterlockedDecrement(&m_nRef)) delete this;
	}

	void Assign(HANDLE hFile)
	{
		m_HandleLock.Init();
		m_hFile = hFile;
	}

	// inside LockHandle() / UnlockHandle();
	void Close_l()
	{
		m_HandleLock.Rundown_l();
	}

	void Close();
};

//////////////////////////////////////////////////////////////////////////
// IO_IRP

class IO_IRP : public OVERLAPPED 
{
	static BLOCK_HEAP s_bh;

	CDataPacket* m_packet;
	PVOID Pointer;
	DWORD m_Code;
	PVOID m_buf[];

public:

	static ULONG BindIoCompletion( IO_OBJECT* pObj, HANDLE hFile)
	{
		NTSTATUS status = pObj->BindIoCompletionCB(hFile, S_OnIoComplete);
		return 0 > status ? RtlNtStatusToDosError(status) : NOERROR;
	}

	VOID OnIoComplete(PVOID Context, PIO_STATUS_BLOCK IoSB);

	static VOID NTAPI S_OnIoComplete(
		_Inout_ PTP_CALLBACK_INSTANCE Instance,
		_Inout_opt_ PVOID Context,
		_In_ PVOID ApcContext,
		_In_ PIO_STATUS_BLOCK IoSB,
		_In_ PTP_IO Io
		)ASM_FUNCTION;

	PVOID SetPointer()
	{
		return Pointer = m_buf;
	}

	PVOID NotDelete()
	{
		Pointer = this;
		return m_buf;
	}

	void Delete()
	{
		if (Pointer == this)
		{
			delete this;
		}
	}

	static BOOL _init(DWORD count)
	{
		return s_bh.Create(sizeof(IO_IRP), count);
	}

	IO_IRP(IO_OBJECT* pObj, DWORD Code, CDataPacket* packet, PVOID Ptr = 0);

	void CheckError(IO_OBJECT* pObj, BOOL fOk, BOOL bSkippedOnSynchronous = FALSE)
	{
		CheckErrorCode(pObj, fOk ? NOERROR : GetLastError(), bSkippedOnSynchronous);
	}

	void CheckErrorCode(IO_OBJECT* pObj, DWORD dwErrorCode, BOOL bSkippedOnSynchronous = FALSE);

	void* operator new(size_t size);

	void* operator new(size_t size, size_t cb);

	void operator delete(PVOID p);
};

//////////////////////////////////////////////////////////////////////////
// NT_IRP

class NT_IRP : public IO_STATUS_BLOCK 
{
	static BLOCK_HEAP s_bh;

	CDataPacket* m_packet;
	PVOID Pointer;
	DWORD m_Code;
	PVOID m_buf[];

	VOID OnIoComplete(PVOID Context, PIO_STATUS_BLOCK IoSB);

public:

	static NTSTATUS BindIoCompletion( IO_OBJECT* pObj, HANDLE hFile)
	{
		return pObj->BindIoCompletionCB(hFile, S_OnIoComplete);
	}

	static VOID NTAPI ApcRoutine (
		PVOID ApcContext,
		PIO_STATUS_BLOCK IoSB,
		ULONG /*Reserved*/
		)ASM_FUNCTION;

	static VOID NTAPI S_OnIoComplete(
		_Inout_ PTP_CALLBACK_INSTANCE Instance,
		_Inout_opt_ PVOID Context,
		_In_ PVOID ApcContext,
		_In_ PIO_STATUS_BLOCK IoSB,
		_In_ PTP_IO Io
		)ASM_FUNCTION;

	PVOID SetPointer()
	{
		return Pointer = m_buf;
	}

	PVOID NotDelete()
	{
		Pointer = this;
		return m_buf;
	}

	PVOID GetBuf()
	{
		return m_buf;
	}

	void Delete()
	{
		if (Pointer == this)
		{
			delete this;
		}
	}

	static BOOL _init(DWORD count)
	{
		return s_bh.Create(sizeof(NT_IRP), count);
	}

	NT_IRP(IO_OBJECT* pObj, DWORD Code, CDataPacket* packet, PVOID Ptr = 0);

	void CheckNtStatus(IO_OBJECT* pObj, NTSTATUS status, BOOL bSkippedOnSynchronous = FALSE);

	void* operator new(size_t size, size_t cb);

	void* operator new(size_t size);

	void operator delete(PVOID p);
};

//////////////////////////////////////////////////////////////////////////
// RtlWait

class __declspec(novtable) RtlWait 
{
	HANDLE _hWait = 0;
	HANDLE _hObject = 0;
	BOOL _cbExecuted = FALSE;
	LONG _dwRef = 1;

	static VOID CALLBACK _WaitCallback(PVOID pTimer, BOOLEAN Timeout)ASM_FUNCTION;

	VOID FASTCALL WaitCallback(BOOLEAN Timeout);

protected:

	virtual VOID OnSignal(HANDLE hObject, NTSTATUS status) = 0;

	virtual ~RtlWait()
	{
		DereferenceDll();
	}

public:

	RtlWait()
	{
		ReferenceDll();
	}

	void AddRef()
	{
		InterlockedIncrementNoFence(&_dwRef);
	}

	void Release()
	{
		if (!InterlockedDecrement(&_dwRef))
		{
			delete this;
		}
	}

	void Unregister();

	BOOL RegisterWait(HANDLE hObject, ULONG dwMilliseconds);
};

//////////////////////////////////////////////////////////////////////////
// RtlTimer

class __declspec(novtable) RtlTimer 
{
	HANDLE _hTimer = 0;
	BOOL _cbExecuted = FALSE;
	LONG _dwRef = 1;

	static VOID CALLBACK _TimerCallback(PVOID pTimer, BOOLEAN /*TimerOrWaitFired*/)ASM_FUNCTION;
	
	VOID TimerCallback();

protected:

	virtual VOID OnTimer(BOOL bCanceled) = 0;

	virtual ~RtlTimer()
	{
		DereferenceDll();
	}

public:

	RtlTimer()
	{
		ReferenceDll();
	}

	void AddRef()
	{
		InterlockedIncrementNoFence(&_dwRef);
	}

	void Release()
	{
		if (!InterlockedDecrement(&_dwRef))
		{
			delete this;
		}
	}

	void Stop();

	BOOL Set(DWORD dwMilliseconds);

	BOOL IsSet()
	{
		return _hTimer != 0;
	}
};

//////////////////////////////////////////////////////////////////////////
// IO_OBJECT_TIMEOUT

class __declspec(novtable) IO_OBJECT_TIMEOUT : public IO_OBJECT
{
	class IoTimer : public RtlTimer
	{
		IO_OBJECT_TIMEOUT* _pObj;

		virtual VOID OnTimer(BOOL /*bCanceled*/);

		~IoTimer()
		{
			_pObj->Release();
		}

	public:

		IoTimer(IO_OBJECT_TIMEOUT* pObj) : _pObj(pObj)
		{
			pObj->AddRef();
		}
	};

	IoTimer* _pTimer;

	void SetNewTimer(IoTimer* pTimer);

protected:

	virtual void CloseObjectHandle(HANDLE hFile)
	{
		SetNewTimer(0);
		if (hFile) NtClose(hFile);
	}

	virtual void OnTimeout()
	{
		Close();
	}

	IO_OBJECT_TIMEOUT() : _pTimer(0)
	{
	}

public:

	void StopTimeout()
	{
		SetNewTimer(0);
	}

	BOOL SetTimeout(DWORD dwMilliseconds);
};
