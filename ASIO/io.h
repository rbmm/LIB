#pragma once

#include "../inc/rundown.h"
#include "../inc/asmfunc.h"
#include "packet.h"
#include "blockheap.h"

extern NTSTATUS (NTAPI *fnSetIoCompletionCallback)(HANDLE , LPOVERLAPPED_COMPLETION_ROUTINE , ULONG );

NTSTATUS NTAPI BindIoCompletionEx(HANDLE hObject, LPOVERLAPPED_COMPLETION_ROUTINE CompletionRoutine);

NTSTATUS NTAPI SkipCompletionOnSuccess(HANDLE hObject);

extern RUNDOWN_REF * g_IoRundown;

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
	}

	virtual ~IO_OBJECT()
	{
		Close();
	}

	virtual void CloseObjectHandle(HANDLE hFile)
	{
		if (hFile) NtClose(hFile);
	}

public:

	_NODISCARD BOOL LockHandle(HANDLE& hFile)
	{
		BOOL f = LockHandle();
		hFile = m_hFile;
		return f;
	}

	void UnlockHandle()
	{
		if (m_HandleLock.Release())
		{
			CloseObjectHandle(m_hFile), m_hFile = 0;
		}
	}

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

	void Close()
	{
		if (LockHandle())
		{
			Close_l();
			UnlockHandle();
		}
	}
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
		m_pObj->IOCompletionRoutine(m_packet, m_Code, dwErrorCode, dwNumberOfBytesTransfered, Pointer);
		delete this;
	}

#ifdef _WINDLL
	static VOID CALLBACK _IOCompletionRoutine(NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)ASM_FUNCTION;
#else
	static VOID CALLBACK _IOCompletionRoutine(NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
	{
		// we must pass IO_IRP pointer in place lpOverlapped in I/O call
		static_cast<IO_IRP*>(lpOverlapped)->IOCompletionRoutine(RtlNtStatusToDosError(status), dwNumberOfBytesTransfered);
	}
#endif

	PVOID SetPointer()
	{
		return Pointer = m_buf;
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
		m_pObj->IOCompletionRoutine(m_packet, m_Code, status, dwNumberOfBytesTransfered, Pointer);
		delete this;
	}

protected:

	~NT_IRP();

public:

#ifdef _WINDLL
	static VOID NTAPI ApcRoutine (
		PVOID /*ApcContext*/,
		PIO_STATUS_BLOCK IoStatusBlock,
		ULONG /*Reserved*/
		)ASM_FUNCTION;

	static VOID CALLBACK _IOCompletionRoutine(NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PVOID ApcContext)ASM_FUNCTION;
#else
	static VOID CALLBACK _IOCompletionRoutine(NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PVOID ApcContext)
	{
		// we must pass NT_IRP pointer in place ApcContext in I/O call
		reinterpret_cast<NT_IRP*>(ApcContext)->IOCompletionRoutine(status, dwNumberOfBytesTransfered);
	}

	static VOID NTAPI ApcRoutine (
		PVOID /*ApcContext*/,
		PIO_STATUS_BLOCK IoStatusBlock,
		ULONG /*Reserved*/
		)
	{
		static_cast<NT_IRP*>(IoStatusBlock)->IOCompletionRoutine(IoStatusBlock->Status, IoStatusBlock->Information);
	}
#endif

	PVOID SetPointer()
	{
		return Pointer = m_buf;
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
// RtlTimer

class RtlTimer 
{
	HANDLE _hTimer;
	LONG _dwRef;

#ifdef _WINDLL
	static VOID CALLBACK _TimerCallback(PVOID pTimer, BOOLEAN /*TimerOrWaitFired*/)ASM_FUNCTION;
#else
	static VOID CALLBACK _TimerCallback(PVOID pTimer, BOOLEAN /*TimerOrWaitFired*/)
	{
		static_cast<RtlTimer*>(pTimer)->TimerCallback();
	}
#endif

protected:

	virtual VOID TimerCallback();

	virtual ~RtlTimer()
	{
	}

public:

	RtlTimer() : _hTimer(0), _dwRef(1)
	{
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

		virtual VOID TimerCallback();

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
