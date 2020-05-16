#pragma once

//#define _PRINT_CPP_NAMES_
#include "../inc/asmfunc.h"

//#define DBG_PRINT

#ifndef DBG_PRINT

#pragma push_macro("DbgPrint")

#ifdef DbgPrint
#undef DbgPrint
#endif

#define DbgPrint /##/

#endif

_NT_END

void* __cdecl operator new(size_t size, NT::POOL_TYPE PoolType);

void* __cdecl operator new[](size_t size, NT::POOL_TYPE PoolType);

void __cdecl operator delete(PVOID pv);

void __cdecl operator delete(PVOID pv, size_t);

void __cdecl operator delete[](PVOID pv);

_NT_BEGIN

#include "..\inc\rundown.h"
#include "Packet.h"

NTSTATUS NTAPI InitIo(PDRIVER_OBJECT DriverObject);

extern RUNDOWN_REF * g_IoRundown;

#define LockDriver() ObfReferenceObject(g_DriverObject)
#define UnlockDriver() ObfDereferenceObject(g_DriverObject)

EXTERN_C_START

NTKERNELAPI VOID FASTCALL ExfAcquirePushLockShared(PEX_PUSH_LOCK PushLock);
NTKERNELAPI VOID FASTCALL ExfAcquirePushLockExclusive(PEX_PUSH_LOCK PushLock);
NTKERNELAPI VOID FASTCALL ExfReleasePushLock(PEX_PUSH_LOCK PushLock);

extern PDRIVER_OBJECT g_DriverObject;

extern LARGE_INTEGER g_timeout;// optional
void NTAPI OnTimeout();// optional

EXTERN_C_END

__forceinline void AcquirePushLockShared(PEX_PUSH_LOCK PushLock )
{
	KeEnterCriticalRegion();
	ExfAcquirePushLockShared(PushLock );
}

__forceinline void AcquirePushLockExclusive(PEX_PUSH_LOCK PushLock )
{
	KeEnterCriticalRegion();
	ExfAcquirePushLockExclusive(PushLock);
}

__forceinline void ReleasePushLock(PEX_PUSH_LOCK PushLock )
{
	ExfReleasePushLock (PushLock);
	KeLeaveCriticalRegion();
}

class __declspec(novtable) IO_OBJECT
{
	friend void IoThread();

	virtual void IOCompletionRoutine(CDataPacket* packet, DWORD Code, NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PVOID Pointer) = 0;

	void OnComplete(PIRP Irp);

	PFILE_OBJECT		m_FileObject;
	HANDLE				m_hFile;
	RundownProtection	m_HandleLock;
	LONG				m_nRef;

	_NODISCARD BOOL LockHandle()
	{
		return m_HandleLock.Acquire();
	}

protected:

	IO_OBJECT();

	virtual ~IO_OBJECT();

	virtual void CloseObjectHandle(HANDLE hFile, PFILE_OBJECT FileObject);

	virtual void OnCleanup()
	{
	}

	NTSTATUS SendIrp(PDEVICE_OBJECT DeviceObject, PIRP Irp);

	static PIRP NTAPI BuildSynchronousFsdRequest(
		ULONG  MajorFunction,
		PDEVICE_OBJECT  DeviceObject,
		PVOID  Buffer,
		ULONG  Length,
		PLARGE_INTEGER  StartingOffset,
		ULONG code, 
		CDataPacket* packet, 
		PVOID Pointer
		);

	static PIRP NTAPI BuildDeviceIoControlRequest(
		ULONG  IoControlCode,
		PDEVICE_OBJECT  DeviceObject,
		PVOID  InputBuffer,
		ULONG  InputBufferLength,
		PVOID  OutputBuffer,
		ULONG  OutputBufferLength,
		BOOLEAN  InternalDeviceIoControl, 
		ULONG code, 
		CDataPacket* packet, 
		PVOID Pointer
		);

public:

	_NODISCARD BOOL LockHandle(HANDLE& hFile, PFILE_OBJECT& FileObject)
	{
		BOOL f = LockHandle();
		hFile = m_hFile, FileObject = m_FileObject;
		return f;
	}

	void UnlockHandle()
	{
		if (m_HandleLock.Release())
		{
			OnCleanup();
			CloseObjectHandle(m_hFile, m_FileObject), m_hFile = 0, m_FileObject = 0;
		}
	}

	HANDLE getHandleNoLock() { return m_hFile; }

	PFILE_OBJECT getFileObjNoLock() { return m_FileObject; }

	void operator delete(void* p);

	void* operator new(size_t cb);

	BOOL AddRefEx()
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

	NTSTATUS Assign(HANDLE hFile);
	NTSTATUS Assign(HANDLE hFile, PFILE_OBJECT FileObject);

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

class __declspec(novtable) IO_OBJECT_TIMEOUT : public IO_OBJECT
{
	class RtlTimer : WORK_QUEUE_ITEM, KDPC, public KTIMER
	{
		IO_OBJECT_TIMEOUT* _pObj;
		LONG _dwRef;

		static VOID NTAPI OnDpc(PKDPC Dpc, PVOID , PVOID , PVOID );

		static VOID NTAPI _OnWorkItem(PVOID This)ASM_FUNCTION;//asm
		static VOID NTAPI OnWorkItem(PVOID This);

		~RtlTimer()
		{
			_pObj->Release();
			DbgPrint("%s<%p>\n", __FUNCTION__, this);
		}

	public:

		void Set(LARGE_INTEGER dueTime)
		{
			AddRef();
			LockDriver();
			KeSetTimer(this, dueTime, this);
		}

		void Cancel();

		void operator delete(void* p)
		{
			ExFreePool(p);
		}

		void* operator new(size_t cb)
		{
			return ExAllocatePool(NonPagedPool, cb);
		}

		RtlTimer(IO_OBJECT_TIMEOUT* pObject) : _pObj(pObject), _dwRef(1)
		{
			pObject->AddRef();

			KeInitializeTimer(this);
			KeInitializeDpc(this, OnDpc, 0);
			ExInitializeWorkItem(this, _OnWorkItem, this);
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
	};

	friend RtlTimer;
	RtlTimer* _pTimer;

	void SetNewTimer(RtlTimer* pTimer);

protected:

	IO_OBJECT_TIMEOUT() : _pTimer(0)
	{
	}

	virtual ~IO_OBJECT_TIMEOUT()
	{
	}

	virtual void OnTimeout()
	{
		Close();
	}

	virtual void OnCleanup()
	{
		StopTimeout();
	}

public:
	BOOL SetTimeout(DWORD dwMilliseconds);
	BOOL SetTimeout(LARGE_INTEGER dueTime);

	void StopTimeout()
	{
		SetNewTimer(0);
	}
};
