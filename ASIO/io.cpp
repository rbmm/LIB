#include "stdafx.h"

_NT_BEGIN

#include "io.h"

ULONG IsXPOr2003()
{
	static ULONG MajorOsVersion;

	if (!MajorOsVersion)
	{
		RtlGetNtVersionNumbers(&MajorOsVersion, 0, 0);
	}

	return MajorOsVersion < 6;
}

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
	Internal = MAXULONG_PTR;
	InternalHigh = MAXULONG_PTR;
	Pointer = Ptr;
	hEvent = 0;
	m_Code = Code;
	m_packet = packet;
	m_pObj = pObj;
	pObj->AddRef();
	if (packet) packet->AddRef();
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
		if (PVOID p = ::operator new(size + cb)) return p;
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

DWORD IO_IRP::CheckErrorCode(DWORD dwErrorCode, BOOL bSkippedOnSuccess)
{
	switch (dwErrorCode)
	{
	case NOERROR:
		if (bSkippedOnSuccess)
		{
			IOCompletionRoutine(NOERROR, InternalHigh);
		}
		// read comment in CheckNtStatus
	case ERROR_IO_PENDING:
		return NOERROR;
	}

	IOCompletionRoutine(dwErrorCode, 0);
	return NOERROR;
}

//////////////////////////////////////////////////////////////////////////
// NT_IRP

BLOCK_HEAP NT_IRP::s_bh;

NTSTATUS NT_IRP::CheckNtStatus(NTSTATUS status, BOOL bSkippedOnSuccess)
{
	if (status == STATUS_PENDING)
	{
		return STATUS_PENDING;
	}

	if (bSkippedOnSuccess)
	{
		IOCompletionRoutine(status, NT_ERROR(status) ? 0 : Information);
	}
	else
	{
		if (NT_ERROR(status))
		{
			IOCompletionRoutine(status, 0);
		}
		// if NT_WARNING(status) (status in range [0x80000000, 0xc0000000) )
		// 1) if status from io-manager (say STATUS_DATATYPE_MISALIGNMENT) :
		// will be NO completion and iosb.Status not modified
		// 2) if status from driver (say STATUS_NO_MORE_FILES) : 
		// will be completion and iosb.Status == status
		// check iosb.Status ? but irp (iosb) can be already free and modified (in case 2.))
		// add ref semantic to irp for xp support and rare case
		// however error from io-manager only in case we do wrong api call (aligment)
		// assume than no this case will be and will be completion
	}

	return 0;
}

void* NT_IRP::operator new(size_t size, size_t cb)
{
	if (g_IoRundown->Acquire())
	{
		if (PVOID p = ::operator new(size + cb)) return p;
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
	Status = -1;
	Information = MAXULONG_PTR;
	Pointer = Ptr;
	m_Code = Code;
	m_packet = packet;
	m_pObj = pObj;
	pObj->AddRef();
	if (packet) packet->AddRef();
}

NT_IRP::~NT_IRP()
{
	if (m_packet) m_packet->Release();
	m_pObj->Release();
}

//////////////////////////////////////////////////////////////////////////
// IO_OBJECT_TIMEOUT

void IO_OBJECT_TIMEOUT::RtlTimer::Stop()
{
	if (HANDLE hTimer = InterlockedExchangePointerNoFence(&_hTimer, 0))
	{
		if (DeleteTimerQueueTimer(0, hTimer, 0))
		{
			// will be no callback, release it reference
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

BOOL IO_OBJECT_TIMEOUT::RtlTimer::Set(DWORD dwMilliseconds)
{
	AddRef();
	if (CreateTimerQueueTimer(&_hTimer, 0, (WAITORTIMERCALLBACK)TimerCallback, this, dwMilliseconds, 0, WT_EXECUTEDEFAULT))
	{
		return TRUE;
	}

	Release();
	return FALSE;
}

VOID CALLBACK IO_OBJECT_TIMEOUT::RtlTimer::TimerCallback(RtlTimer* pTimer, BOOLEAN /*TimerOrWaitFired*/)
{
	pTimer->Stop();
	IO_OBJECT_TIMEOUT* pObj = pTimer->_pObj;
	pObj->SetNewTimer(0);
	pObj->OnTimeout();
	pTimer->Release();
}

void IO_OBJECT_TIMEOUT::SetNewTimer(RtlTimer* pTimer)
{
	if (pTimer = (RtlTimer*)InterlockedExchangePointer((void**)&_pTimer, pTimer))
	{
		pTimer->Stop();
		pTimer->Release();
	}
}

BOOL IO_OBJECT_TIMEOUT::SetTimeout(DWORD dwMilliseconds)
{
	BOOL fOk = FALSE;

	if (RtlTimer* pTimer = new RtlTimer(this))
	{
		if (pTimer->Set(dwMilliseconds))
		{
			fOk = TRUE;

			// _pTimer hold additional reference
			pTimer->AddRef();
			SetNewTimer(pTimer);

			if (!pTimer->_hTimer)
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