#pragma once

#include "io.h"

#define IP(a, b, c, d) ((DWORD)(a + (b << 8) + (c << 16) + (d << 24)))
#define RA Address->Address

class __declspec(novtable) CTdiObject : public IO_OBJECT_TIMEOUT
{
	virtual void IOCompletionRoutine(CDataPacket* , ULONG , NTSTATUS , ULONG_PTR , PVOID )
	{
		__debugbreak();
	}

public:
	virtual void OnIp(DWORD /*ip*/)
	{
	}
	void DnsToIp(PCSTR Dns);
};

class CTdiAddress : public CTdiObject
{
	friend class CTcpEndpoint;

	NTSTATUS QueryInfo(LONG QueryType, PVOID buf, ULONG cb, ULONG_PTR& Information);
protected:
	NTSTATUS Create(POBJECT_ATTRIBUTES DeviceName, USHORT port, ULONG ip);

public:

	NTSTATUS GetPort(PWORD port);
	NTSTATUS Create(USHORT port, ULONG ip = 0);
};

class CUdpEndpoint : public CTdiAddress
{
	virtual void IOCompletionRoutine(CDataPacket* packet, DWORD Code, NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PVOID Pointer);

protected:

	enum {
		recv = 'rrrr', send = 'ssss'
	};

	virtual void OnRecv(PSTR , ULONG , CDataPacket* , TA_IP_ADDRESS*  )
	{
	}

	virtual void OnSend(CDataPacket* )
	{
	}

public:

	NTSTATUS Create(USHORT port, ULONG ip = 0);
	NTSTATUS RecvFrom(CDataPacket* packet);
	NTSTATUS SendTo(ULONG ip, USHORT port, CDataPacket* packet);	
	NTSTATUS SendTo(ULONG ip, USHORT port, const void* lpData, DWORD cbData);	
};

class __declspec(novtable) CTcpEndpoint : public CTdiObject, public TA_IP_ADDRESS
{
	friend class CDnsSocket;
	friend class CTcpEndpointTest;

protected:

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

public:

	NTSTATUS Create(DWORD BufferSize);
	NTSTATUS Listen();
	NTSTATUS Connect(ULONG ip, USHORT port);
	NTSTATUS Recv();
	NTSTATUS Recv(PVOID Buffer, ULONG ReceiveLength);
	NTSTATUS Send(CDataPacket* packet);	
	void Disconnect(NTSTATUS status = STATUS_SUCCESS);
	void Close();
	CDataPacket* getDataPacket() { return m_packet; };

	CTdiAddress* getAddress() { return	m_pAddress; }

private:

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