#pragma once

#include "../inc/rundown.h"
#include "packet.h"
#include "blockheap.h"

extern NTSTATUS (NTAPI *fnSetIoCompletionCallback)(HANDLE , LPOVERLAPPED_COMPLETION_ROUTINE , ULONG );

NTSTATUS BindIoCompletionEx(HANDLE hObject, LPOVERLAPPED_COMPLETION_ROUTINE CompletionRoutine);

NTSTATUS SkipCompletionOnSuccess(HANDLE hObject);

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

	virtual void OnCleanup()
	{
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
			OnCleanup();
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

	VOID IOCompletionRoutine(DWORD dwErrorCode, ULONG_PTR dwNumberOfBytesTransfered)
	{
		m_pObj->IOCompletionRoutine(m_packet, m_Code, dwErrorCode, dwNumberOfBytesTransfered, Pointer);
		delete this;
	}

protected:

	~IO_IRP();

public:

	static VOID CALLBACK _IOCompletionRoutine(NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
	{
		static_cast<IO_IRP*>(lpOverlapped)->IOCompletionRoutine(RtlNtStatusToDosError(status), dwNumberOfBytesTransfered);
	}

	PVOID SetPointer()
	{
		return Pointer = m_buf;
	}

	static BOOL _init(DWORD count)
	{
		return s_bh.Create(sizeof(IO_IRP), count);
	}

	IO_IRP(IO_OBJECT* pObj, DWORD Code, CDataPacket* packet, PVOID Ptr = 0);

	DWORD CheckError(BOOL fOk, BOOL bSkippedOnSuccess = FALSE)
	{
		return CheckErrorCode(fOk ? NOERROR : GetLastError(), bSkippedOnSuccess);
	}

	DWORD CheckErrorCode(DWORD dwErrorCode, BOOL bSkippedOnSuccess = FALSE);

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
		m_pObj->IOCompletionRoutine(m_packet, m_Code, status, dwNumberOfBytesTransfered, Pointer);
		delete this;
	}

protected:

	~NT_IRP();

public:

	static VOID CALLBACK _IOCompletionRoutine(NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PIO_STATUS_BLOCK iosb)
	{
		static_cast<NT_IRP*>(iosb)->IOCompletionRoutine(status, dwNumberOfBytesTransfered);
	}

	static VOID NTAPI ApcRoutine (
		PVOID /*ApcContext*/,
		PIO_STATUS_BLOCK IoStatusBlock,
		ULONG /*Reserved*/
		)
	{
		static_cast<NT_IRP*>(IoStatusBlock)->IOCompletionRoutine(IoStatusBlock->Status, IoStatusBlock->Information);
	}

	PVOID SetPointer()
	{
		return Pointer = m_buf;
	}

	static BOOL _init(DWORD count)
	{
		return s_bh.Create(sizeof(NT_IRP), count);
	}

	NT_IRP(IO_OBJECT* pObj, DWORD Code, CDataPacket* packet, PVOID Ptr = 0);

	NTSTATUS CheckNtStatus(NTSTATUS status, BOOL bSkippedOnSuccess = FALSE);

	void* operator new(size_t size, size_t cb);

	void* operator new(size_t size);

	void operator delete(PVOID p);

	static NTSTATUS RtlBindIoCompletion(HANDLE hObject)
	{
		return BindIoCompletionEx(hObject, (LPOVERLAPPED_COMPLETION_ROUTINE)_IOCompletionRoutine);
	}
};

//////////////////////////////////////////////////////////////////////////
// IO_OBJECT_TIMEOUT

class __declspec(novtable) IO_OBJECT_TIMEOUT : public IO_OBJECT
{
	struct RtlTimer 
	{
		IO_OBJECT_TIMEOUT* _pObj;
		HANDLE _hTimer;
		LONG _dwRef;

		RtlTimer(IO_OBJECT_TIMEOUT* pObj) : _hTimer(0), _dwRef(1), _pObj(pObj)
		{
			pObj->AddRef();
		}

		~RtlTimer()
		{
			_pObj->Release();
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

		static VOID CALLBACK TimerCallback(RtlTimer* pTimer, BOOLEAN /*TimerOrWaitFired*/);
	};

	friend RtlTimer;

	RtlTimer* _pTimer;

	void SetNewTimer(RtlTimer* pTimer);

protected:

	virtual void OnCleanup()
	{
		SetNewTimer(0);
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
