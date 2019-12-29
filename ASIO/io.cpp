#include "stdafx.h"

_NT_BEGIN
//#define _PRINT_CPP_NAMES_

#include "io.h"

#ifdef _WINDLL
void ReferenceDll()ASM_FUNCTION;
void DereferenceDll()ASM_FUNCTION;
#define IOCompletionRoutine(status, dwNumberOfBytesTransfered) IOCompletionRoutine((status), (dwNumberOfBytesTransfered));DereferenceDll()
#else
#define ReferenceDll()
#define DereferenceDll()
#endif

NTSTATUS (NTAPI *fnSetIoCompletionCallback)(HANDLE , LPOVERLAPPED_COMPLETION_ROUTINE , ULONG ) = 
	(NTSTATUS (NTAPI *)(HANDLE , LPOVERLAPPED_COMPLETION_ROUTINE , ULONG ))RtlSetIoCompletionCallback;

NTSTATUS BindIoCompletionEx(HANDLE hObject, LPOVERLAPPED_COMPLETION_ROUTINE CompletionRoutine)
{
	return fnSetIoCompletionCallback(hObject, CompletionRoutine, 0);
}

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
	g_IoRundown->Release();
}

void* IO_OBJECT::operator new(size_t cb)
{
	if (g_IoRundown->Acquire())
	{
		if (PVOID p = ::operator new(cb)) return p;
		g_IoRundown->Release();
	}
	return 0;
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
	m_pObj = pObj;
	pObj->AddRef();
	if (packet) packet->AddRef();
	ReferenceDll();
}

IO_IRP::~IO_IRP()
{
	if (m_packet) m_packet->Release();
	m_pObj->Release();
}

void* IO_IRP::operator new(size_t size, size_t cb)
{
	if (g_IoRundown->Acquire())
	{
		if (PVOID p = ::operator new(size + cb)) 
		{
			return p;
		}
		g_IoRundown->Release();
	}

	return 0;
}

void* IO_IRP::operator new(size_t size)
{
	if (g_IoRundown->Acquire())
	{
		PVOID p = size == sizeof(IO_IRP) ? s_bh.alloc() : 0;
		if (!p && !(p = ::operator new(size)))
		{
			g_IoRundown->Release();
		}
		return p;
	}

	return 0;
}

void IO_IRP::operator delete(PVOID p)
{
	s_bh.IsBlock(p) ? s_bh.free(p) : ::operator delete(p);
	g_IoRundown->Release();
}

DWORD IO_IRP::CheckErrorCode(DWORD dwErrorCode, BOOL bSkippedOnSynchronous)
{
	switch (dwErrorCode)
	{
	case NOERROR:
		if (!bSkippedOnSynchronous)
		{
	case ERROR_IO_PENDING:
			return NOERROR;
		}
	}
	IOCompletionRoutine(dwErrorCode, InternalHigh);

	// handle possible error in IOCompletionRoutine
	return NOERROR;
}

//////////////////////////////////////////////////////////////////////////
// NT_IRP

BLOCK_HEAP NT_IRP::s_bh;

NTSTATUS NT_IRP::CheckNtStatus(NTSTATUS status, BOOL bSkippedOnSynchronous)
{
	if (status == STATUS_PENDING)
	{
		return STATUS_PENDING;
	}

	if (NT_ERROR(status) || bSkippedOnSynchronous)
	{
		IOCompletionRoutine(status, Information);
	}

	// handle possible error in IOCompletionRoutine
	return 0;
}

void* NT_IRP::operator new(size_t size, size_t cb)
{
	if (g_IoRundown->Acquire())
	{
		if (PVOID p = ::operator new(size + cb)) 
		{
			return p;
		}
		g_IoRundown->Release();
	}

	return 0;
}

void* NT_IRP::operator new(size_t size)
{
	if (g_IoRundown->Acquire())
	{
		PVOID p = size == sizeof(NT_IRP) ? s_bh.alloc() : 0;
		if (!p && !(p = ::operator new(size)))
		{
			g_IoRundown->Release();
		}
		
		return p;
	}

	return 0;
}

void NT_IRP::operator delete(PVOID p)
{
	s_bh.IsBlock(p) ? s_bh.free(p) : ::operator delete(p);
	g_IoRundown->Release();
}

NT_IRP::NT_IRP(IO_OBJECT* pObj, DWORD Code, CDataPacket* packet, PVOID Ptr)
{
	Status = STATUS_PENDING;
	Information = 0;
	Pointer = Ptr;
	m_Code = Code;
	m_packet = packet;
	m_pObj = pObj;
	pObj->AddRef();
	if (packet) packet->AddRef();
	ReferenceDll();
}

NT_IRP::~NT_IRP()
{
	if (m_packet) m_packet->Release();
	m_pObj->Release();
}

//////////////////////////////////////////////////////////////////////////
//
VOID RtlTimer::TimerCallback()
{
	Stop();
	Release();
}

BOOL RtlTimer::Set(DWORD dwMilliseconds)
{
	AddRef();
	ReferenceDll();

	if (CreateTimerQueueTimer(&_hTimer, 0, _TimerCallback, this, dwMilliseconds, 0, WT_EXECUTEINTIMERTHREAD))
	{
		return TRUE;
	}

	DereferenceDll();
	Release();
	return FALSE;
}

void RtlTimer::Stop()
{
	if (HANDLE hTimer = InterlockedExchangePointerNoFence(&_hTimer, 0))
	{
		if (DeleteTimerQueueTimer(0, hTimer, 0))
		{
			// will be no callback, release it reference
			DereferenceDll();
			Release();
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

VOID IO_OBJECT_TIMEOUT::IoTimer::TimerCallback()
{
	IO_OBJECT_TIMEOUT* pObj = _pObj;
	pObj->SetNewTimer(0);
	pObj->OnTimeout();
	RtlTimer::TimerCallback();
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