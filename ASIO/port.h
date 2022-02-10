#pragma once

#include "../asio/socket.h"

class Port;
class PortList;

__forceinline ULONG GetTimeSinceBoot()
{
	return (ULONG)(GetTickCount64() / 1000);
}

class ENDPOINT_ENTRY : LIST_ENTRY 
{
	friend Port;

protected:
	Port* _Port;
	LONG _DisconnectTime;

	void CheckTimeout(CTcpEndpoint* pSocket, ULONG time);

	void SetDisconnectTime(ULONG SecDelay = 0);

	ENDPOINT_ENTRY()
	{
		_DisconnectTime = MAXULONG;
	}

	~ENDPOINT_ENTRY();
};

struct __declspec(novtable) PortContext
{
	virtual CTcpEndpoint* getEndpoint(ENDPOINT_ENTRY*) = 0;
	virtual ENDPOINT_ENTRY* CreateEntry(CSocketObject* pAddress) = 0;
	virtual ULONG GetReceiveDataLength() = 0;
};

class Port : LIST_ENTRY, CRITICAL_SECTION
{
	friend PortList;
	friend ENDPOINT_ENTRY;

	LIST_ENTRY _entry;
	PortList* _List;
	PortContext* _pCtx;
	CSocketObject* _pAddress = 0;
	LONG _nListenCount = 0, _nConnectedCount = 0, _nEndpointCount = 0, _nMinListen, _nMaxListen;
	LONG _dwRefCount = 1;
	BOOLEAN _bStop = FALSE;

	void StartListen(CTcpEndpoint* pSocket, ULONG dwReceiveDataLength);

	void AddEndpoint();

	void Remove(ENDPOINT_ENTRY* Entry);

	void OnTimer(ULONG time);

	void Stop();

	ULONG Create(_In_reads_bytes_(namelen) const sockaddr * name, _In_ int namelen, _In_ int protocol = IPPROTO_TCP);

	Port(PortList* List);

	~Port();

public:

	ULONG GetPort(PUSHORT Port)
	{
		return _pAddress->GetPort(Port);
	}

	void OnConnect(ULONG dwError);

	void OnDisconnect(ENDPOINT_ENTRY* Entry);

	BOOL AddRefEx()
	{
		return ObpLock(&_dwRefCount);
	}

	void AddRef()
	{
		InterlockedIncrementNoFence(&_dwRefCount);
	}

	void Release()
	{
		if (!InterlockedDecrement(&_dwRefCount))
		{
			delete this;
		}
	}

	ULONG Start(LONG nMinListen, LONG nMaxListen, PortContext* pCtx);

	void get_stat(LONG& nListenCount, LONG& nConnectedCount, LONG& nEndpointCount)
	{
		nListenCount = _nListenCount, nConnectedCount = _nConnectedCount, nEndpointCount = _nEndpointCount;
	}

	void Enum(PVOID, BOOL (WINAPI* )(PVOID , ENDPOINT_ENTRY* ));
};

class PortList : LIST_ENTRY, CRITICAL_SECTION
{
	friend Port;
	HANDLE _hTimer = 0;
	LONG _dwRefCount = 1;

	static VOID CALLBACK WaitOrTimerCallback(PVOID lpParameter, BOOLEAN /*TimerOrWaitFired*/)
	{
		reinterpret_cast<PortList*>(lpParameter)->OnTimer();
	}

	void AddPort(PLIST_ENTRY Entry);
	void RemovePort(PLIST_ENTRY Entry);
	~PortList();
public:

	PortList();

	void AddRef()
	{
		InterlockedIncrementNoFence(&_dwRefCount);
	}
	
	void Release()
	{
		if (!InterlockedDecrement(&_dwRefCount))
		{
			delete this;
		}
	}

	ULONG CreatePort(_Out_ Port** pPort, _In_ WORD port, _In_opt_ ULONG ip = 0);
	ULONG CreatePort(_Out_ Port** pPort, _In_ const sockaddr * name, _In_ int namelen, _In_opt_ int protocol = IPPROTO_TCP);

	void OnTimer();
	void Stop();
	ULONG Start(ULONG Period);
};
