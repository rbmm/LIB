#include "stdafx.h"

_NT_BEGIN
//#define _PRINT_CPP_NAMES_

#include "io.h"

IO_RUNDOWN IO_RUNDOWN::g_IoRundown;

//void IO_RUNDOWN::RundownCompleted()
//{
	// x86: ?RundownCompleted@IO_RUNDOWN@NT@@MAEXXZ 
	// x64: ?RundownCompleted@IO_RUNDOWN@NT@@MEAAXXZ
	//__pragma(message("; " __FUNCSIG__ "\r\nextern " __FUNCDNAME__ " : PROC"));
//}

void IO_RUNDOWN::RundownCompletedNop()
{
	// x86: ?RundownCompletedNop@IO_RUNDOWN@NT@@AAEXXZ
	// x64: ?RundownCompletedNop@IO_RUNDOWN@NT@@AEAAXXZ
	//__pragma(message("; " __FUNCSIG__ "\r\nextern " __FUNCDNAME__ " : PROC"));
}

#ifdef _WIN64
#pragma comment(linker, "/alternatename:?RundownCompleted@IO_RUNDOWN@NT@@MEAAXXZ=?RundownCompletedNop@IO_RUNDOWN@NT@@AEAAXXZ")
#else
#pragma comment(linker, "/alternatename:?RundownCompleted@IO_RUNDOWN@NT@@MAEXXZ=?RundownCompletedNop@IO_RUNDOWN@NT@@AAEXXZ")
#endif

NTSTATUS SkipCompletionOnSuccess(HANDLE hObject)
{
	IO_STATUS_BLOCK iosb;
	FILE_IO_COMPLETION_NOTIFICATION_INFORMATION ficni = { FILE_SKIP_COMPLETION_PORT_ON_SUCCESS };
	return NtSetInformationFile(hObject, &iosb, &ficni, sizeof(ficni), FileIoCompletionNotificationInformation);
}

//////////////////////////////////////////////////////////////////////////
// IO_OBJECT

void IO_OBJECT::operator delete(void* p)
{
	::operator delete(p);
	IO_RUNDOWN::g_IoRundown.Release();
}

void* IO_OBJECT::operator new(size_t cb)
{
	if (IO_RUNDOWN::g_IoRundown.Acquire())
	{
		if (PVOID p = ::operator new(cb)) return p;
		IO_RUNDOWN::g_IoRundown.Release();
	}
	return 0;
}

_NODISCARD BOOL IO_OBJECT::LockHandle(HANDLE& hFile)
{
	BOOL f = LockHandle();
	hFile = m_hFile;
	return f;
}

void IO_OBJECT::UnlockHandle()
{
	if (m_HandleLock.Release())
	{
		HANDLE hFile = m_hFile;
		m_hFile = 0;
		CloseObjectHandle(hFile); 
	}
}

void IO_OBJECT::Close()
{
	if (LockHandle())
	{
		Close_l();
		UnlockHandle();
	}
}

NTSTATUS IO_OBJECT::BindIoCompletionCB(HANDLE hFile, PTP_IO_CALLBACK Callback)
{
	TP_CALLBACK_ENVIRON CallbackEnviron, *pCallbackEnviron = 0;
	if (_G_Pool)
	{
		TpInitializeCallbackEnviron(&CallbackEnviron);
		TpSetCallbackThreadpool(&CallbackEnviron, _G_Pool);
		pCallbackEnviron = &CallbackEnviron;
	}
	return TpAllocIoCompletion(&m_Io, hFile, Callback, this, pCallbackEnviron);
}
//////////////////////////////////////////////////////////////////////////
// IO_IRP

BLOCK_HEAP IO_IRP::s_bh;

IO_IRP::IO_IRP(IO_OBJECT* pObj, DWORD Code, CDataPacket* packet, PVOID Ptr)
{
	Internal = STATUS_PENDING;
	InternalHigh = 0;
	Pointer = Ptr;
	hEvent = 0;
	m_Code = Code;
	m_packet = packet;
	if (packet) packet->AddRef();
	pObj->StartIo();
}

VOID IO_IRP::OnIoComplete(PVOID Context, PIO_STATUS_BLOCK IoSB)
{
	CPP_FUNCTION;
	BOOL bDelete = Pointer != this;
	CDataPacket* packet = m_packet;
	m_packet = 0;
	reinterpret_cast<IO_OBJECT*>(Context)->IOCompletionRoutine(packet, m_Code, IoSB->Status, IoSB->Information, Pointer);
	reinterpret_cast<IO_OBJECT*>(Context)->Release();
	if (packet) packet->Release(); 
	if (bDelete) delete this;
}

void* IO_IRP::operator new(size_t size, size_t cb)
{
	if (IO_RUNDOWN::g_IoRundown.Acquire())
	{
		if (PVOID p = ::operator new(size + cb)) 
		{
			return p;
		}
		IO_RUNDOWN::g_IoRundown.Release();
	}

	return 0;
}

void* IO_IRP::operator new(size_t size)
{
	if (IO_RUNDOWN::g_IoRundown.Acquire())
	{
		PVOID p = size == sizeof(IO_IRP) ? s_bh.alloc() : 0;
		if (!p && !(p = ::operator new(size)))
		{
			IO_RUNDOWN::g_IoRundown.Release();
		}
		return p;
	}

	return 0;
}

void IO_IRP::operator delete(PVOID p)
{
	s_bh.IsBlock(p) ? s_bh.free(p) : ::operator delete(p);
	IO_RUNDOWN::g_IoRundown.Release();
}

void IO_IRP::CheckErrorCode(IO_OBJECT* pObj, DWORD dwErrorCode, BOOL bSkippedOnSynchronous)
{
	switch (dwErrorCode)
	{
	case NOERROR:
		if (!bSkippedOnSynchronous)
		{
	case ERROR_IO_PENDING:
			return ;
		}
	}

	TpCancelAsyncIoOperation(pObj->m_Io);
	Internal = dwErrorCode;
	OnIoComplete(pObj, reinterpret_cast<PIO_STATUS_BLOCK>(this));
}

//////////////////////////////////////////////////////////////////////////
// NT_IRP

BLOCK_HEAP NT_IRP::s_bh;

void NT_IRP::CheckNtStatus(IO_OBJECT* pObj, NTSTATUS status, BOOL bSkippedOnSynchronous)
{
	if (status == STATUS_PENDING)
	{
		return ;
	}

	if (NT_ERROR(status) || bSkippedOnSynchronous)
	{
		TpCancelAsyncIoOperation(pObj->m_Io);
		Status = status;
		OnIoComplete(pObj, this);
	}
}

void* NT_IRP::operator new(size_t size, size_t cb)
{
	if (IO_RUNDOWN::g_IoRundown.Acquire())
	{
		if (PVOID p = ::operator new(size + cb)) 
		{
			return p;
		}
		IO_RUNDOWN::g_IoRundown.Release();
	}

	return 0;
}

void* NT_IRP::operator new(size_t size)
{
	if (IO_RUNDOWN::g_IoRundown.Acquire())
	{
		PVOID p = size == sizeof(NT_IRP) ? s_bh.alloc() : 0;
		if (!p && !(p = ::operator new(size)))
		{
			IO_RUNDOWN::g_IoRundown.Release();
		}
		
		return p;
	}

	return 0;
}

void NT_IRP::operator delete(PVOID p)
{
	s_bh.IsBlock(p) ? s_bh.free(p) : ::operator delete(p);
	IO_RUNDOWN::g_IoRundown.Release();
}

NT_IRP::NT_IRP(IO_OBJECT* pObj, DWORD Code, CDataPacket* packet, PVOID Ptr)
{
	Status = STATUS_PENDING;
	Information = 0;
	Pointer = Ptr;
	m_Code = Code;
	m_packet = packet;
	if (packet) packet->AddRef();
	pObj->StartIo();
}

VOID NT_IRP::OnIoComplete(
				  _Inout_opt_ PVOID Context, 
				  _In_ PIO_STATUS_BLOCK IoSB)
{
	CPP_FUNCTION;
	BOOL bDelete = Pointer != this;
	CDataPacket* packet = m_packet;
	m_packet = 0;
	reinterpret_cast<IO_OBJECT*>(Context)->IOCompletionRoutine(packet, m_Code, IoSB->Status, IoSB->Information, Pointer);
	reinterpret_cast<IO_OBJECT*>(Context)->Release();
	if (packet) packet->Release();
	if (bDelete) delete this;
}


//////////////////////////////////////////////////////////////////////////
// RtlWait

BOOL RtlWait::RegisterWait(HANDLE hObject, ULONG dwMilliseconds)
{
	AddRef();

	_hObject = hObject;
	if (RegisterWaitForSingleObject(&_hWait, hObject, _WaitCallback, this, dwMilliseconds, WT_EXECUTEINTIMERTHREAD|WT_EXECUTEONLYONCE))
	{
		return TRUE;
	}

	_hObject = 0;
	Release();
	return FALSE;
}

VOID RtlWait::WaitCallback(BOOLEAN Timeout)
{
	CPP_FUNCTION;

	_cbExecuted = TRUE;
	Unregister();
	OnSignal(_hObject, Timeout ? STATUS_TIMEOUT : STATUS_WAIT_0);
	Release();
}

void RtlWait::Unregister()
{
	if (HANDLE hWait = InterlockedExchangePointerNoFence(&_hWait, 0))
	{
		if (UnregisterWaitEx(hWait, 0))
		{
			// WaitOrTimerCallback or not called or already returned
			if (_cbExecuted)
			{
				// WaitOrTimerCallbackalready returned
				// it called Release() by self
			}
			else
			{
				// WaitOrTimerCallback never called
				OnSignal(_hObject, STATUS_CANCELLED);
				Release();
			}
		}
		else
		{
			if (GetLastError() != ERROR_IO_PENDING)
			{
				__debugbreak();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// RtlTimer

VOID RtlTimer::TimerCallback()
{
	CPP_FUNCTION;

	_cbExecuted = TRUE;
	Stop();
	OnTimer(FALSE);
	Release();
}

BOOL RtlTimer::Set(DWORD dwMilliseconds)
{
	AddRef();

	_cbExecuted = FALSE;

	if (CreateTimerQueueTimer(&_hTimer, 0, _TimerCallback, this, dwMilliseconds, 0, WT_EXECUTEINTIMERTHREAD|WT_EXECUTEONLYONCE))
	{
		return TRUE;
	}

	Release();
	return FALSE;
}

void RtlTimer::Stop()
{
	if (HANDLE hTimer = InterlockedExchangePointerNoFence(&_hTimer, 0))
	{
		if (DeleteTimerQueueTimer(0, hTimer, 0))
		{
			// WaitOrTimerCallback or not called or already returned
			if (_cbExecuted)
			{
				// WaitOrTimerCallbackalready returned
				// it called Release() by self
			}
			else
			{
				// WaitOrTimerCallback never called
				OnTimer(TRUE);
				Release();
			}
		}
		else
		{
			if (GetLastError() != ERROR_IO_PENDING)
			{
				__debugbreak();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// IO_OBJECT_TIMEOUT

VOID IO_OBJECT_TIMEOUT::IoTimer::OnTimer(BOOL /*bCanceled*/)
{
	IO_OBJECT_TIMEOUT* pObj = _pObj;
	pObj->SetNewTimer(0);
	pObj->OnTimeout();
}

void IO_OBJECT_TIMEOUT::SetNewTimer(IoTimer* pTimer)
{
	if (pTimer = (IoTimer*)InterlockedExchangePointer((void**)&_pTimer, pTimer))
	{
		pTimer->Stop();
		pTimer->Release();
	}
}

BOOL IO_OBJECT_TIMEOUT::SetTimeout(DWORD dwMilliseconds)
{
	BOOL fOk = FALSE;

	if (IoTimer* pTimer = new IoTimer(this))
	{
		if (pTimer->Set(dwMilliseconds))
		{
			fOk = TRUE;

			// _pTimer hold additional reference
			pTimer->AddRef();
			SetNewTimer(pTimer);

			if (!pTimer->IsSet())
			{
				// already fired
				SetNewTimer(0);
			}
		}

		pTimer->Release();
	}

	return fOk;
}

_NT_END