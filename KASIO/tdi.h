#pragma once

#include "io.h"

#define IP(a, b, c, d) ((DWORD)(a + (b << 8) + (c << 16) + (d << 24)))

typedef struct TA_INET_ADDRESS {
	LONG TAAddressCount;
	struct {
		USHORT AddressLength;       // length in bytes of this address == 14
		USHORT AddressType;         // this will == TDI_ADDRESS_TYPE_IP
		union {
			UCHAR tp_addr[ISO_MAX_ADDR_LENGTH];
			TDI_ADDRESS_IP Ipv4;
			TDI_ADDRESS_IP6 Ipv6;
		};
	};
} *PTA_INET_ADDRESS;

class __declspec(novtable) CTdiObject : public IO_OBJECT_TIMEOUT
{
	virtual void IOCompletionRoutine(CDataPacket* , ULONG , NTSTATUS , ULONG_PTR , PVOID )
	{
		__debugbreak();
	}

public:
	virtual void OnIp(USHORT /*AddrType*/, PVOID /*Addr*/, USHORT /*AddrLength*/)
	{
	}
	void DnsToIp(_In_ PCSTR Dns, _In_ USHORT QueryType = DNS_RTYPE_A, _In_ LONG QueryOptions = DNS_QUERY_STANDARD);
};

class CTdiAddress : public CTdiObject
{
	friend class CTcpEndpoint;

protected:
public:
	NTSTATUS QueryInfo(LONG QueryType, PVOID buf, ULONG cb, ULONG_PTR& Information);
	NTSTATUS Create(POBJECT_ATTRIBUTES DeviceName, USHORT port, ULONG ip);
	NTSTATUS Create(POBJECT_ATTRIBUTES DeviceName, USHORT AddressType, PVOID Address, USHORT AddressLength, USHORT exv = 0);

	NTSTATUS GetPort(PWORD port);
	NTSTATUS Create(USHORT port, ULONG ip = 0);
};

class CUdpEndpoint : public CTdiAddress
{
	virtual void IOCompletionRoutine(CDataPacket* packet, DWORD Code, NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PVOID Pointer);

	virtual BOOL KeepFileHandle()
	{
		return FALSE;
	}
protected:

	enum {
		recv = 'rrrr', send = 'ssss'
	};

	virtual void OnRecv(PSTR , ULONG , CDataPacket* , TA_INET_ADDRESS*  )
	{
	}

	virtual void OnSend(CDataPacket* )
	{
	}

public:

	NTSTATUS Create(USHORT port, ULONG ip = 0);
	NTSTATUS RecvFrom(CDataPacket* packet);
	NTSTATUS SendTo(USHORT AddressType, PVOID Address, USHORT AddressLength, CDataPacket* packet);	
	NTSTATUS SendTo(ULONG ip, USHORT port, CDataPacket* packet);	
	NTSTATUS SendTo(ULONG ip, USHORT port, const void* lpData, ULONG cbData);	
};

extern OBJECT_ATTRIBUTES oaTcp, oaUdp;

class __declspec(novtable) CTcpEndpoint : public CTdiObject, public TA_INET_ADDRESS
{
	friend class CDnsSocket;
	friend class CTcpEndpointTest;
	friend class CTcpEndpointEvt;

protected:

	static inline TDI_CONNECTION_INFORMATION zRequestConnectionInformation {};

	CDataPacket*		m_packet;
private:
	CTdiAddress*		m_pAddress;
	RundownProtection	m_connectionLock;
protected:
	LONG				m_flags;

	enum {
		flListenActive, flDisconectActive, flMaxFlag,
		cnct = 'cccc', recv = 'rrrr', send = 'ssss', disc = 'dddd'
	};

	CTcpEndpoint(CTdiAddress* pAddress);

	virtual ~CTcpEndpoint();

	/************************************************************************/
	/* implement this ! */

	virtual BOOL OnConnect(NTSTATUS status) = 0;	

	virtual void OnDisconnect() = 0;

	virtual BOOL OnRecv(PSTR Buffer, ULONG cbTransferred) = 0;

	virtual void OnSend(CDataPacket* )
	{
	}

	virtual ULONG GetRecvBuffer(void** ppv);

	virtual void LogError(DWORD /*opCode*/, NTSTATUS /*status*/)
	{
	}
	/************************************************************************/

	/*virtual*/ NTSTATUS Recv(PVOID Buffer, ULONG ReceiveLength);
	NTSTATUS Recv();
public:

	NTSTATUS Create(DWORD BufferSize, POBJECT_ATTRIBUTES poa = &oaTcp);
	NTSTATUS Listen();

	NTSTATUS Connect(ULONG ip, USHORT port);
	NTSTATUS Connect(PIN6_ADDR ip6, USHORT port);
	NTSTATUS Connect(USHORT AddrType, PVOID Addr, USHORT AddrLength);

	NTSTATUS Send(CDataPacket* packet);	
	void Disconnect(NTSTATUS status = STATUS_SUCCESS);
	void Close();
	CDataPacket* getDataPacket() { return m_packet; };

	CTdiAddress* getAddress() { return	m_pAddress; }

private:

	NTSTATUS Connect();
	NTSTATUS Associate(PFILE_OBJECT FileObject, HANDLE AddressHandle);

	_NODISCARD BOOL LockConnection()
	{
		return m_connectionLock.Acquire();
	}

	void UnlockConnection()
	{
		if (m_connectionLock.Release())
		{
			InterlockedBitTestAndResetNoFence(&m_flags, flListenActive);
			OnDisconnect();
		}
	}

	void RunDownConnection_l()
	{
		m_connectionLock.Rundown_l();
	}

	void IOCompletionRoutine(CDataPacket* packet, DWORD Code, NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PVOID Pointer);
};