#pragma once

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

extern NTSTATUS (NTAPI *fnSetIoCompletionCallback)(HANDLE , LPOVERLAPPED_COMPLETION_ROUTINE , ULONG );

NTSTATUS NTAPI BindIoCompletionEx(HANDLE hObject, LPOVERLAPPED_COMPLETION_ROUTINE CompletionRoutine);

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

	virtual void IOCompletionRoutine(CDataPacket* packet, DWORD Code, NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PVOID Pointer) = 0;

	HANDLE				m_hFile;
	RundownProtection	m_HandleLock;
	LONG				m_nRef;

	_NODISCARD BOOL LockHandle()
	{
		return m_HandleLock.Acquire();
	}

protected:

	IO_OBJECT() : m_nRef(1), m_hFile(0)
	{
		ReferenceDll();
	}

	virtual ~IO_OBJECT()
	{
		Close();
		DereferenceDll();
	}

	virtual void CloseObjectHandle(HANDLE hFile)
	{
		if (hFile) NtClose(hFile);
	}

public:

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

	IO_OBJECT* m_pObj;
	CDataPacket* m_packet;
	PVOID Pointer;
	DWORD m_Code;
	PVOID m_buf[];

protected:

	~IO_IRP();

public:
	VOID IOCompletionRoutine(DWORD dwErrorCode, ULONG_PTR dwNumberOfBytesTransfered)
	{
		CPP_FUNCTION;
		BOOL bDelete = Pointer != this;
		m_pObj->IOCompletionRoutine(m_packet, m_Code, dwErrorCode, dwNumberOfBytesTransfered, Pointer);
		if (bDelete) delete this;
	}

	static VOID CALLBACK _IOCompletionRoutine(NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)ASM_FUNCTION;

	PVOID SetPointer()
	{
		return Pointer = m_buf;
	}

	void NotDelete()
	{
		Pointer = this;
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

	DWORD CheckError(BOOL fOk, BOOL bSkippedOnSynchronous = FALSE)
	{
		return CheckErrorCode(fOk ? NOERROR : GetLastError(), bSkippedOnSynchronous);
	}

	DWORD CheckErrorCode(DWORD dwErrorCode, BOOL bSkippedOnSynchronous = FALSE);

	void* operator new(size_t size);

	void* operator new(size_t size, size_t cb);

	void operator delete(PVOID p);

	static ULONG BindIoCompletion(HANDLE hObject)
	{
		return RtlNtStatusToDosError(BindIoCompletionEx(hObject, (LPOVERLAPPED_COMPLETION_ROUTINE)_IOCompletionRoutine));
	}
};

//////////////////////////////////////////////////////////////////////////
// NT_IRP

class NT_IRP : public IO_STATUS_BLOCK 
{
	static BLOCK_HEAP s_bh;

	IO_OBJECT* m_pObj;
	CDataPacket* m_packet;
	PVOID Pointer;
	DWORD m_Code;
	PVOID m_buf[];

	VOID IOCompletionRoutine(NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered)
	{
		CPP_FUNCTION;
		BOOL bDelete = Pointer != this;
		m_pObj->IOCompletionRoutine(m_packet, m_Code, status, dwNumberOfBytesTransfered, Pointer);
		if (bDelete) delete this;
	}

protected:

	~NT_IRP();

public:

	static VOID NTAPI ApcRoutine (
		PVOID /*ApcContext*/,
		PIO_STATUS_BLOCK IoStatusBlock,
		ULONG /*Reserved*/
		)ASM_FUNCTION;

	static VOID CALLBACK _IOCompletionRoutine(NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PVOID ApcContext)ASM_FUNCTION;

	PVOID SetPointer()
	{
		return Pointer = m_buf;
	}

	void NotDelete()
	{
		Pointer = this;
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

	NTSTATUS CheckNtStatus(NTSTATUS status, BOOL bSkippedOnSynchronous = FALSE);

	void* operator new(size_t size, size_t cb);

	void* operator new(size_t size);

	void operator delete(PVOID p);

	static NTSTATUS RtlBindIoCompletion(HANDLE hObject)
	{
		return BindIoCompletionEx(hObject, (LPOVERLAPPED_COMPLETION_ROUTINE)_IOCompletionRoutine);
	}
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
