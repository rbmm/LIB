#include "stdafx.h"

_NT_BEGIN

#include "port.h"

PortList::PortList()
{
	InitializeCriticalSection(this);
	InitializeListHead(this);
	_hTimer = 0;
}

PortList::~PortList()
{
	DeleteCriticalSection(this);
	if (_hTimer)
	{
		__debugbreak();
	}
}

ULONG PortList::CreatePort(Port** pPort, WORD port, ULONG ip /*= 0*/)
{
	if (Port* p = new Port(this))
	{
		if (ULONG err = p->Create(port, ip))
		{
			p->Release();
			return err;
		}

		*pPort = p;
		return NOERROR;
	}

	return ERROR_NO_SYSTEM_RESOURCES;
}

void PortList::AddPort(PLIST_ENTRY Entry)
{
	EnterCriticalSection(this);
	InsertTailList(this, Entry);
	LeaveCriticalSection(this);
}

void PortList::RemovePort(PLIST_ENTRY Entry)
{
	EnterCriticalSection(this);
	RemoveEntryList(Entry);
	LeaveCriticalSection(this);
}

void PortList::OnTimer()
{
	ULONG time = GetTimeSinceBoot();

	EnterCriticalSection(this);

	PLIST_ENTRY head = this, entry = head->Flink;

	while (entry != head)
	{
		Port* p = CONTAINING_RECORD(entry, Port, _entry);

		entry = entry->Flink;

		if (p->AddRefEx())
		{
			p->OnTimer(time);
			p->Release();
		}
	}

	LeaveCriticalSection(this);
}

void PortList::Stop()
{
	if (_hTimer)
	{
		if (!DeleteTimerQueueTimer(0, _hTimer, INVALID_HANDLE_VALUE))
		{
			__debugbreak();
		}
		_hTimer = 0;
	}

	EnterCriticalSection(this);

	PLIST_ENTRY head = this, entry = head->Flink;

	while (entry != head)
	{
		Port* p = CONTAINING_RECORD(entry, Port, _entry);

		entry = entry->Flink;

		if (p->AddRefEx())
		{
			p->Stop();
			p->Release();
		}
	}

	LeaveCriticalSection(this);
}

ULONG PortList::Start(ULONG Period)
{
	Period *= 1000;

	return CreateTimerQueueTimer(&_hTimer, 0, WaitOrTimerCallback, this, Period, Period, 
		WT_EXECUTEINTIMERTHREAD) ? NOERROR : GetLastError();
}

//////////////////////////////////////////////////////////////////////////
//

Port::~Port()
{
	_List->RemovePort(&_entry);

	if (_pAddress)
	{
		_pAddress->Release();
	}

	DeleteCriticalSection(this);
}

Port::Port(PortList* List)
{
	InitializeListHead(this);
	_List = List;
	_nListenCount = 0, _nConnectedCount = 0, _nEndpointCount = 0;
	_dwRefCount = 1;
	_bStop = FALSE;
	_pAddress = 0;

	InitializeCriticalSection(this);
	_List->AddPort(&_entry);
}

ULONG Port::Start(LONG nMinListen, LONG nMaxListen, PortContext* pCtx)
{
	if (nMinListen > nMaxListen || nMaxListen < 1)
	{
		return ERROR_INVALID_PARAMETER;
	}

	_pCtx = pCtx;
	_nMaxListen = nMaxListen, _nMinListen = nMinListen;

	if (ULONG n = (_nMaxListen + _nMinListen) >> 1)
	{
		do 
		{
			AddEndpoint();
		} while (--n);
	}

	return _nListenCount;
}

void Port::Remove(ENDPOINT_ENTRY* pEntry)
{
	EnterCriticalSection(this);
	_nEndpointCount--;
	RemoveEntryList(pEntry);
	LeaveCriticalSection(this);

	Release();
}

void Port::AddEndpoint()
{
	if (!_bStop)
	{
		if (ENDPOINT_ENTRY* pEntry = _pCtx->CreateEntry(_pAddress))
		{
			AddRef();
			pEntry->_Port = this;

			EnterCriticalSection(this);
			InsertTailList(this, pEntry);
			_nEndpointCount++;
			LeaveCriticalSection(this);

			CTcpEndpoint* pSocket = _pCtx->getEndpoint(pEntry);

			StartListen(pSocket, _pCtx->GetReceiveDataLength());

			pSocket->Release();
		}
	}
}

void Port::OnConnect(ULONG dwError)
{
	if (!dwError)
	{
		_InterlockedIncrement(&_nConnectedCount);
	}

	if (_InterlockedDecrement(&_nListenCount) < _nMinListen)
	{
		AddEndpoint();
	}
}

void Port::StartListen(CTcpEndpoint* pSocket, ULONG dwReceiveDataLength )
{
	_InterlockedIncrement(&_nListenCount);

	if (pSocket->Listen(dwReceiveDataLength))
	{
		_InterlockedDecrement(&_nListenCount);
	}
}

void Port::OnDisconnect(ENDPOINT_ENTRY* Entry)
{
	_InterlockedDecrement(&_nConnectedCount);

	if (!_bStop && _nListenCount < _nMaxListen)
	{
		if (_InterlockedExchange(&Entry->_DisconnectTime, MAXULONG))
		{
			StartListen(_pCtx->getEndpoint(Entry), _pCtx->GetReceiveDataLength());
		}
	}
}

void Port::OnTimer(ULONG time)
{
	PortContext* pCtx = _pCtx;

	EnterCriticalSection(this);

	PLIST_ENTRY head = this, entry = head->Flink;

	while (entry != head)
	{
		ENDPOINT_ENTRY* pEe = static_cast<ENDPOINT_ENTRY*>(entry);

		entry = entry->Flink;

		pEe->CheckTimeout(pCtx->getEndpoint(pEe), time);
	}

	LeaveCriticalSection(this);
}

void Port::Enum(PVOID ctx, BOOL (WINAPI* cb)(PVOID , ENDPOINT_ENTRY* ))
{
	EnterCriticalSection(this);

	PLIST_ENTRY head = this, entry = head->Flink;

	while (entry != head)
	{
		ENDPOINT_ENTRY* pEe = static_cast<ENDPOINT_ENTRY*>(entry);

		entry = entry->Flink;

		if (!cb(ctx, pEe))
		{
			break;
		}
	}

	LeaveCriticalSection(this);
}

void Port::Stop()
{
	_bStop = TRUE;

	if (_pAddress)
	{
		_pAddress->Close();
	}

	PortContext* pCtx = _pCtx;

	EnterCriticalSection(this);

	PLIST_ENTRY head = this, entry = head;

	while ((entry = entry->Flink) != head)
	{
		pCtx->getEndpoint(static_cast<ENDPOINT_ENTRY*>(entry))->Close();// Close can be called even if refcount == 0
	}

	LeaveCriticalSection(this);
}

ULONG Port::Create(WORD Port, ULONG ip /*= 0*/)
{
	if (_pAddress = new CSocketObject)
	{
		return _pAddress->CreateAddress(Port, ip);
	}

	return ERROR_NO_SYSTEM_RESOURCES;
}

//////////////////////////////////////////////////////////////////////////
//

ENDPOINT_ENTRY::~ENDPOINT_ENTRY()
{
	_Port->Remove(this);
}

void ENDPOINT_ENTRY::CheckTimeout(CTcpEndpoint* pSocket, ULONG time)
{
	if (pSocket->AddRefEx())
	{
		ULONG DisconnectTime = _DisconnectTime;

__0:
		if (DisconnectTime && time > DisconnectTime)
		{		
			ULONG Value = _InterlockedCompareExchange(&_DisconnectTime, 0, DisconnectTime);

			if (Value != DisconnectTime)
			{
				DisconnectTime = Value;
				goto __0;
			}

			//DbgPrint("%u:%p>Timeout(%x)\r\n", GetTickCount(), this, DisconnectTime);

			pSocket->Close();
		}

		pSocket->Release();
	}
}

void ENDPOINT_ENTRY::SetDisconnectTime(ULONG SecDelay)
{
	_InterlockedExchange(&_DisconnectTime, GetTimeSinceBoot() + SecDelay);
}

_NT_END
