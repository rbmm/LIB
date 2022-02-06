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

ULONG PortList::CreatePort(_Out_ Port** pPort, _In_ WORD port, _In_opt_ ULONG ip /*= 0*/)
{
	sockaddr_in asi = { AF_INET, port };

	asi.sin_addr.S_un.S_addr = ip;

	return CreatePort(pPort, (sockaddr*)&asi, sizeof(asi));
}

ULONG PortList::CreatePort(_Out_ Port** pPort, _In_ const sockaddr * name, _In_ int namelen, _In_opt_ int protocol /*= IPPROTO_TCP*/)
{
	if (Port* p = new Port(this))
	{
		if (ULONG dwError = p->Create(name, namelen, protocol))
		{
			p->Release();
			return dwError;
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
	PortList* List = _List;
	List->RemovePort(&_entry);
	List->Release();

	if (CSocketObject* pAddress = _pAddress)
	{
		pAddress->Release();
	}

	DeleteCriticalSection(this);
}

Port::Port(PortList* List) : _List(List)
{
	InitializeListHead(this);
	InitializeCriticalSection(this);
	List->AddRef();
	List->AddPort(&_entry);
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
		InterlockedIncrementNoFence(&_nConnectedCount);
	}

	if (InterlockedDecrementNoFence(&_nListenCount) < _nMinListen)
	{
		AddEndpoint();
	}
}

void Port::StartListen(CTcpEndpoint* pSocket, ULONG dwReceiveDataLength )
{
	InterlockedIncrementNoFence(&_nListenCount);

	if (pSocket->Listen(dwReceiveDataLength))
	{
		InterlockedDecrementNoFence(&_nListenCount);
	}
}

void Port::OnDisconnect(ENDPOINT_ENTRY* Entry)
{
	InterlockedDecrementNoFence(&_nConnectedCount);

	if (!_bStop && _nListenCount < _nMaxListen)
	{
		if (InterlockedExchange(&Entry->_DisconnectTime, MAXULONG))
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

ULONG Port::Create(_In_reads_bytes_(namelen) const sockaddr * name, _In_ int namelen, _In_ int protocol)
{
	if (CSocketObject* pAddress = new CSocketObject)
	{
		if (ULONG dwError = pAddress->CreateAddress(name, namelen, protocol))
		{
			return dwError;
		}
		_pAddress = pAddress;
		return NOERROR;
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
			ULONG Value = InterlockedCompareExchange(&_DisconnectTime, 0, DisconnectTime);

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
