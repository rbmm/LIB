#pragma once

#include <mswsock.h>
#include <ws2ipdef.h>

#include "io.h"

#define IP(a, b, c, d) ((DWORD)(a + (b << 8) + (c << 16) + (d << 24)))

struct SOCKADDR_IN_EX 
{
	union {
		SOCKADDR_INET addr;
		SOCKADDR saAddress;
	};
	INT dwAddressLength;
};

class CSocketObject : public IO_OBJECT_TIMEOUT
{
	friend class CTcpEndpoint;

	virtual void IOCompletionRoutine(CDataPacket* , DWORD , NTSTATUS , ULONG_PTR , PVOID )
	{
		__debugbreak();
	}
protected:

	virtual void CloseObjectHandle(HANDLE hFile);

public:

	virtual void OnIp(DWORD /*ip*/)
	{
	}

	void DnsToIp(PCSTR Dns);

	ULONG GetLocalAddr(PSOCKET_ADDRESS LocalAddr );
	ULONG GetPort(PUSHORT Port);

	ULONG Create(int af, int type, int protocol);

	ULONG CreateAddress(USHORT port, ULONG ip = 0);
	ULONG CreateAddress(_In_reads_bytes_(namelen) const struct sockaddr * name, _In_ int namelen, _In_ int protocol = IPPROTO_TCP);
};

class CUdpEndpoint : public CSocketObject
{
	virtual void IOCompletionRoutine(CDataPacket* packet, DWORD Code, NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PVOID Pointer);

protected:

	enum {
		recv = 'rrrr', send = 'ssss'
	};

	virtual void OnRecv(PSTR , ULONG , CDataPacket* , SOCKADDR_IN_EX* )
	{
	}

	virtual void OnSend(CDataPacket* )
	{
	}

public:

	ULONG Create(WORD Port, ULONG ip = 0);
	ULONG Create(PSOCKADDR Address, DWORD AddressLength);
	ULONG RecvFrom(CDataPacket* packet);
	ULONG SendTo(PSOCKADDR Address, DWORD AddressLength, CDataPacket* packet);	
	ULONG SendTo(PSOCKADDR Address, DWORD AddressLength, const void* lpData, DWORD cbData);	
	ULONG SendTo(ULONG IpAddr, USHORT Port, CDataPacket* packet);	
	ULONG SendTo(ULONG IpAddr, USHORT Port, const void* lpData, DWORD cbData);	
};

class __declspec(novtable) CTcpEndpoint : public CSocketObject
{
	friend class CDnsSocket;
	friend class TestSocket;

protected:

	CDataPacket*		m_packet;
	CSocketObject*		m_pAddress;
private:
	RundownProtection	m_connectionLock;
protected:
	LONG				m_flags;

	union {
		ULONG			m_dwReceiveDataLength;
		sockaddr_in		m_RemoteSockaddr;
		sockaddr_in6	m_RemoteSockaddr6;
	};

	enum {
		flBind, flListenActive, flDisconectActive, flMaxFlag,
		lstn = 'llll', cnct = 'cccc', recv = 'rrrr', send = 'ssss', disc = 'dddd'
	};

	CTcpEndpoint(CSocketObject*	pAddress = 0);

	virtual ~CTcpEndpoint();

	virtual ULONG vSend(SOCKET socket, WSABUF* lpBuffers, DWORD dwBufferCount, IO_IRP* Irp);
	virtual ULONG vRecv(SOCKET socket, WSABUF* lpBuffers, DWORD dwBufferCount, IO_IRP* Irp);
	virtual CDataPacket* allocPacket(ULONG cb) { return new(cb) CDataPacket; };

	/************************************************************************/
	/* implement this ! */

	virtual BOOL OnConnect(ULONG dwError) = 0;	

	virtual void OnDisconnect() = 0;

	virtual BOOL OnRecv(PSTR Buffer, ULONG cbTransferred) = 0;

	virtual void OnSend(CDataPacket* )
	{
	}

	virtual void OnTransmitFile (ULONG /*dwError*/, PVOID /*Context*/)
	{
	}

	virtual ULONG GetConnectData(void** ppSendBuffer);

	virtual ULONG GetRecvBuffers(WSABUF lpBuffers[2], void** ppv);

	virtual void LogError(DWORD /*opCode*/, DWORD /*dwError*/)
	{
	}

	/************************************************************************/

public:

	ULONG Recv();// usually not call it direct !
	ULONG Recv(WSABUF* lpBuffers, DWORD dwBufferCount, PVOID buf);// usually not call it direct !

	ULONG Create(DWORD BufferSize, int af = AF_INET, int protocol = IPPROTO_TCP);
	ULONG Listen(ULONG dwReceiveDataLength = 0);
	
	ULONG Connect(ULONG IpAddr, USHORT Port);
	ULONG Connect(PSOCKADDR RemoteAddress, DWORD RemoteAddressLength);

	void Disconnect(ULONG dwErrorReason = NOERROR);
	void Close();
	ULONG Send(CDataPacket* packet);	
	ULONG Send(const void* Buffer, ULONG cb);	
	CDataPacket* getDataPacket() { return m_packet; };

	ULONG SendFile(HANDLE hFile,
		DWORD nNumberOfBytesToWrite,
		DWORD nNumberOfBytesPerSend,
		PLARGE_INTEGER ByteOffset,
		PTRANSMIT_FILE_BUFFERS lpTransmitBuffers,
		DWORD dwFlags,
		PVOID Context);

	ULONG getRemoteIP(){
		return m_RemoteSockaddr.sin_family == AF_INET ? m_RemoteSockaddr.sin_addr.S_un.S_addr : 0;
	}

	USHORT getRemotePort(){
		return m_RemoteSockaddr.sin_family == AF_INET ? m_RemoteSockaddr.sin_port : 0;
	}

	ULONG GetConnectionTime(PULONG seconds);

private:

	void IOCompletionRoutine(CDataPacket* packet, DWORD Code, NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PVOID Pointer);

	void GetSockaddrs(PVOID buf);

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

	ULONG Connect_l(SOCKET socket, PSOCKADDR RemoteAddress, DWORD RemoteAddressLength);
};