#include "StdAfx.h"
#include <ws2ipdef.h >

_NT_BEGIN

#define LockSocket(socket) LockHandle((HANDLE&)socket)
#define UnlockSocket() UnlockHandle()

#include "socket.h"

ULONG WSA_ERROR(int i) 
{
	return i ? WSAGetLastError() : NOERROR;
}

ULONG BOOL_TO_ERR(BOOL b)
{
	return b ? NOERROR : WSAGetLastError();
}

SOCKET _WSASocket(
				 _In_ int                af,
				 _In_ int                type,
				 _In_ int                protocol,
				 _In_ LPWSAPROTOCOL_INFO lpProtocolInfo,
				 _In_ GROUP              g,
				 _In_ DWORD              dwFlags
				 )
{
	static char flag_supported = -1;

	if (flag_supported < 0)
	{
		ULONG dwMajorVersion, dwMinorVersion, dwBuildNumber;
		RtlGetNtVersionNumbers(&dwMajorVersion, &dwMinorVersion, &dwBuildNumber);
		flag_supported = (0x06010000|7601) <= ((dwMajorVersion << 24)|(dwMinorVersion << 16)|(dwBuildNumber & 0xffff));
	}

	if (flag_supported)
	{
		return WSASocket(af, type, protocol, lpProtocolInfo, g, dwFlags | WSA_FLAG_NO_HANDLE_INHERIT);
	}

	SOCKET s = WSASocket(af, type, protocol, lpProtocolInfo, g, dwFlags);

	if (s != INVALID_SOCKET)
	{
		SetHandleInformation((HANDLE)s, HANDLE_FLAG_INHERIT, 0);
	}

	return s;
}

//////////////////////////////////////////////////////////////////////////
// CSocketObject

void CSocketObject::CloseObjectHandle(HANDLE hFile)
{
	if (hFile) closesocket((SOCKET)hFile);
}

ULONG CSocketObject::GetPort(PUSHORT pPort)
{
	union {
		sockaddr_in sai;
		sockaddr_in6 sai6;
		sockaddr sa;
	};

	int len = sizeof (sai6);

	ULONG err = ERROR_INVALID_HANDLE;

	SOCKET socket;
	if (LockSocket(socket))
	{
		err = WSA_ERROR(getsockname(socket, &sa, &len));
		UnlockSocket();
	}

	USHORT Port = 0;

	if (!err)
	{
		switch (sa.sa_family)
		{
		case AF_INET:
			Port = sai.sin_port;
			break;
		case AF_INET6:
			Port = sai6.sin6_port;
			break;
		}
	}

	*pPort = Port;

	return err;
}

ULONG CSocketObject::Create(int af, int type, int protocol)
{
	SOCKET socket = _WSASocket(af, type, protocol, 0, 0, WSA_FLAG_OVERLAPPED);

	if (INVALID_SOCKET == socket)
	{
		return WSAGetLastError();
	}

	if (ULONG err = IO_IRP::BindIoCompletion((HANDLE)socket))
	{
		closesocket(socket);
		return err;
	}

	Assign((HANDLE)socket);
	return NOERROR;
}

ULONG CSocketObject::CreateAddress(USHORT port, ULONG ip)
{
	sockaddr_in asi = { AF_INET, port };

	asi.sin_addr.S_un.S_addr = ip;

	return CreateAddress((sockaddr*)&asi, sizeof(asi));
}

ULONG CSocketObject::CreateAddress(_In_reads_bytes_(namelen) const struct sockaddr * name, _In_ int namelen)
{
	ULONG err = Create(name->sa_family, SOCK_STREAM, IPPROTO_TCP);

	if (err == NOERROR)
	{
		err = ERROR_INVALID_HANDLE;

		SOCKET socket;
		if (LockSocket(socket))
		{
			err = WSA_ERROR(bind(socket, name, namelen) || listen(socket, 0));
			UnlockSocket();
		}
	}

	return err;
}
//////////////////////////////////////////////////////////////////////////
// CUdpEndpoint

ULONG CUdpEndpoint::Create(WORD port, ULONG ip)
{
	sockaddr_in asi = { AF_INET, port };

	asi.sin_addr.S_un.S_addr = ip;

	return Create((sockaddr*)&asi, sizeof(asi));
}

ULONG CUdpEndpoint::Create(PSOCKADDR Address, DWORD AddressLength)
{
	ULONG err = CSocketObject::Create(Address->sa_family, SOCK_DGRAM, IPPROTO_UDP);

	if (err == NOERROR)
	{
		err = ERROR_INVALID_HANDLE;

		SOCKET socket;
		if (LockSocket(socket))
		{
			err = WSA_ERROR(bind(socket, Address, AddressLength));
			UnlockSocket();
		}
	}
	return err;
}

ULONG CUdpEndpoint::SendTo(ULONG IpAddr, USHORT Port, const void* lpData, DWORD cbData)
{
	if (CDataPacket* packet = new(cbData) CDataPacket)
	{
		memcpy(packet->getData(), lpData, cbData);
		packet->setDataSize(cbData);
		ULONG err = SendTo(IpAddr, Port, packet);
		packet->Release();
		return err;
	}

	return ERROR_NO_SYSTEM_RESOURCES;
}

ULONG CUdpEndpoint::SendTo(PSOCKADDR Address, DWORD AddressLength, const void* lpData, DWORD cbData)
{
	if (CDataPacket* packet = new(cbData) CDataPacket)
	{
		memcpy(packet->getData(), lpData, cbData);
		packet->setDataSize(cbData);
		ULONG err = SendTo(Address, AddressLength, packet);
		packet->Release();
		return err;
	}

	return ERROR_NO_SYSTEM_RESOURCES;
}

ULONG CUdpEndpoint::SendTo(ULONG IpAddr, USHORT Port, CDataPacket* packet )
{
	sockaddr_in saddr = { AF_INET, Port };
	saddr.sin_addr.s_addr = IpAddr;

	return SendTo((sockaddr*)&saddr, sizeof saddr, packet);
}

ULONG CUdpEndpoint::SendTo(PSOCKADDR Address, DWORD AddressLength, CDataPacket* packet)
{
	ULONG pad = packet->getPad();

	WSABUF wb = { packet->getDataSize() - pad, packet->getData() + pad };

	if (IO_IRP* Irp = new IO_IRP(this, send, packet))
	{
		DWORD n;
		ULONG err = ERROR_INVALID_HANDLE;

		SOCKET socket;
		if (LockSocket(socket))
		{
			err = WSA_ERROR(WSASendTo(socket, &wb, 1, &n, 0, Address, AddressLength, Irp, 0));
			UnlockSocket();
		}
		return Irp->CheckErrorCode(err);
	}

	return ERROR_NO_SYSTEM_RESOURCES;
}

ULONG CUdpEndpoint::RecvFrom(CDataPacket* packet)
{
	WSABUF wb = { packet->getFreeSize(), packet->getFreeBuffer() };

	if (wb.len <= sizeof(SOCKADDR_IN_EX))
	{
		return ERROR_INSUFFICIENT_BUFFER;
	}

	SOCKADDR_IN_EX* addr = (SOCKADDR_IN_EX*)wb.buf;

	wb.buf += sizeof(SOCKADDR_IN_EX), wb.len -= sizeof(SOCKADDR_IN_EX);

	if (IO_IRP* Irp = new IO_IRP(this, recv, packet, wb.buf))
	{
		DWORD n;
		addr->Fromlen = sizeof (SOCKADDR_IN);
		DWORD Flags = 0;
		ULONG err = ERROR_INVALID_HANDLE;

		SOCKET socket;
		if (LockSocket(socket))
		{
			err = WSA_ERROR(WSARecvFrom(socket, &wb, 1, &n, &Flags, (sockaddr*)addr, &addr->Fromlen, Irp, 0));

			UnlockSocket();
		}
		return Irp->CheckErrorCode(err);
	}

	return ERROR_NO_SYSTEM_RESOURCES;
}

void CUdpEndpoint::IOCompletionRoutine(CDataPacket* packet, DWORD Code, NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PVOID Pointer)
{
	switch(Code)
	{
	case recv:
		OnRecv(status ? 0 : (PSTR)Pointer, status ? status : (ULONG)dwNumberOfBytesTransfered, packet, (SOCKADDR_IN_EX*)Pointer - 1);
		break;

	case send:
		OnSend(packet);
		break;
	default: __debugbreak();
	}
}

//////////////////////////////////////////////////////////////////////////
// CTcpEndpoint

static LPFN_DISCONNECTEX lpfnDisconnectEx;

CTcpEndpoint::CTcpEndpoint(CSocketObject* pAddress) : m_pAddress(pAddress), m_packet(0), m_flags(0)
{
	if (pAddress) pAddress->AddRef();
}

CTcpEndpoint::~CTcpEndpoint()
{
	if (m_packet) m_packet->Release();
	if (m_pAddress) m_pAddress->Release();
}

ULONG CTcpEndpoint::GetConnectionTime(PULONG seconds)
{
	int bytes = sizeof(ULONG);

	ULONG err = ERROR_INVALID_HANDLE;
	SOCKET socket;
	if (LockSocket(socket))
	{
		err = WSA_ERROR(getsockopt( socket, SOL_SOCKET, SO_CONNECT_TIME, (char *)seconds, &bytes ));
		UnlockSocket();
	}

	return err;
}

void CTcpEndpoint::Close()
{
	IO_OBJECT::Close();

	if (LockConnection())
	{
		RunDownConnection_l();
		UnlockConnection();
	}
}

void CTcpEndpoint::Disconnect(DWORD dwErrorReason)
{
	if (LockConnection())
	{
		// no more new read/write/disconnect
		RunDownConnection_l();

		switch (dwErrorReason)
		{
		case ERROR_INVALID_HANDLE:
		case WSAENOTCONN:
		case WSAECONNABORTED:
		case WSAECONNRESET:
		case WSAEDISCON:
		case ERROR_CONNECTION_ABORTED:
			// we already disconnected
			UnlockConnection();
			return ;
		}

		// only once
		if (InterlockedBitTestAndSetNoFence(&m_flags, flDisconectActive))
		{
			UnlockConnection();
			return ;
		}

		if (IO_IRP* Irp = new IO_IRP(this, disc, 0))
		{
			ULONG err = ERROR_INVALID_HANDLE;
			SOCKET socket;
			if (LockSocket(socket))
			{
				err = BOOL_TO_ERR(lpfnDisconnectEx(socket, Irp, TF_REUSE_SOCKET, 0));
				UnlockSocket();
			}
			Irp->CheckErrorCode(err);
			return;
		}

		// if fail begin IO, direct call with error
		IOCompletionRoutine(0, disc, ERROR_NO_SYSTEM_RESOURCES, 0, 0);
	}
}

ULONG CTcpEndpoint::Create(DWORD BufferSize, int af)
{
	if (BufferSize && !(m_packet = new(BufferSize) CDataPacket)) return ERROR_NO_SYSTEM_RESOURCES;

	if (ULONG err = CSocketObject::Create(af, SOCK_STREAM, IPPROTO_TCP))
	{
		return err;
	}

	if (!lpfnDisconnectEx)
	{
		DWORD dwBytes;

		static GUID guid = WSAID_DISCONNECTEX;

		LPFN_DISCONNECTEX _lpfnDisconnectEx = 0;

		ULONG err = ERROR_INVALID_HANDLE;

		SOCKET socket;
		if (LockSocket(socket))
		{
			err = BOOL_TO_ERR(WSAIoctl(socket, 
				SIO_GET_EXTENSION_FUNCTION_POINTER, 
				&guid, 
				sizeof(guid),
				&_lpfnDisconnectEx, 
				sizeof(_lpfnDisconnectEx), 
				&dwBytes, 
				NULL, 
				NULL));

			UnlockSocket();
		}

		if (err) 
		{
			return err;
		}

		lpfnDisconnectEx = _lpfnDisconnectEx;
	}

	return NOERROR;
}

void CTcpEndpoint::GetSockaddrs(PVOID buf)
{
	PSOCKADDR LocalSockaddr, RemoteSockaddr;
	INT LocalSockaddrLength, RemoteSockaddrLength;

	GetAcceptExSockaddrs(buf, m_dwReceiveDataLength, sizeof(sockaddr_in6) + 16, sizeof(sockaddr_in6) + 16, 
		&LocalSockaddr, &LocalSockaddrLength, &RemoteSockaddr, &RemoteSockaddrLength);

	m_RemoteSockaddr.sin_family = AF_UNSPEC;

	if (RemoteSockaddrLength <= sizeof(m_RemoteSockaddr6))
	{
		memcpy(&m_RemoteSockaddr6, RemoteSockaddr, RemoteSockaddrLength);
	}
}

ULONG CTcpEndpoint::Listen(ULONG dwReceiveDataLength)
{
	ULONG cb = m_packet->getBufferSize(), cbNeed = 2*(sizeof(sockaddr_in6) + 16) + dwReceiveDataLength;

	if (cb < cbNeed || cbNeed < dwReceiveDataLength)
	{
		return ERROR_INVALID_PARAMETER;
	}

	if (InterlockedBitTestAndSetNoFence(&m_flags, flListenActive))
	{
		return ERROR_BAD_PIPE;
	}

	PVOID buf = m_packet->getData();

	if (IO_IRP* Irp = new IO_IRP(this, lstn, m_packet, buf))
	{
		DWORD dwBytes;
		m_dwReceiveDataLength = dwReceiveDataLength;

		ULONG err = ERROR_INVALID_HANDLE;

		SOCKET socket, address;

		if (LockSocket(socket))
		{
			if (m_pAddress->LockSocket(address))
			{
				err = BOOL_TO_ERR(AcceptEx(address, socket, 
					buf, dwReceiveDataLength, sizeof(sockaddr_in6) + 16, sizeof(sockaddr_in6) + 16, &dwBytes, Irp));

				m_pAddress->UnlockSocket();
			}

			UnlockSocket();
		}

		return Irp->CheckErrorCode(err);
	}

	// if fail begin IO, direct call with error
	IOCompletionRoutine(m_packet, lstn, ERROR_NO_SYSTEM_RESOURCES, 0, 0);
	return NOERROR;
}

LPFN_CONNECTEX g_lpfnConnectEx;
GUID guid_connectex = WSAID_CONNECTEX;

ULONG CTcpEndpoint::GetConnectData(void** ppSendBuffer)
{
	ULONG dwSendDataLength = 0;
	PVOID pSendBuffer = 0;

	if (CDataPacket* packet = m_packet)
	{
		ULONG pad = packet->getPad();

		if (dwSendDataLength = packet->getDataSize() - pad)
		{
			pSendBuffer = packet->getData() + pad;
		}
	}

	*ppSendBuffer = pSendBuffer;
	return dwSendDataLength;
}

ULONG CTcpEndpoint::Connect(ULONG IpAddr, USHORT Port)
{
	sockaddr_in addr = { AF_INET };
	addr.sin_addr.s_addr = IpAddr;
	addr.sin_port = Port;

	return Connect((PSOCKADDR)&addr, sizeof(addr));
}

ULONG CTcpEndpoint::Connect(PSOCKADDR RemoteAddress, DWORD RemoteAddressLength)
{
	ULONG err = ERROR_INVALID_HANDLE;

	SOCKET socket;
	if (LockSocket(socket))
	{
		err = Connect_l(socket, RemoteAddress, RemoteAddressLength);
		UnlockSocket();
	}
	return err;
}

ULONG CTcpEndpoint::Connect_l(SOCKET socket, PSOCKADDR RemoteAddress, DWORD RemoteAddressLength)
{
	ULONG err;

	if (!g_lpfnConnectEx)
	{
		DWORD dwBytes;

		LPFN_CONNECTEX lpfnConnectEx;

		if (WSAIoctl(socket, 
			SIO_GET_EXTENSION_FUNCTION_POINTER, 
			&guid_connectex, 
			sizeof(guid_connectex),
			&lpfnConnectEx, 
			sizeof(lpfnConnectEx), 
			&dwBytes, 
			NULL, 
			NULL)) return WSAGetLastError();

		if (!g_lpfnConnectEx)
		{
			g_lpfnConnectEx = lpfnConnectEx;
		}
	}

	if (!InterlockedBitTestAndSetNoFence(&m_flags, flBind))
	{
		PSOCKADDR LocalAddress = (PSOCKADDR)alloca(RemoteAddressLength);
		RtlZeroMemory(LocalAddress, RemoteAddressLength);
		LocalAddress->sa_family = RemoteAddress->sa_family;

		err = WSA_ERROR(bind(socket, LocalAddress, RemoteAddressLength));
		if (err)
		{
			Close();
			return err;
		}
	}

	if (InterlockedBitTestAndSetNoFence(&m_flags, flListenActive))
	{
		return ERROR_BAD_PIPE;
	}

	PVOID lpSendBuffer;
	ULONG dwSendDataLength = GetConnectData(&lpSendBuffer);

	if (IO_IRP* Irp = new IO_IRP(this, cnct, 0))
	{
		DWORD dwBytes;

		err = BOOL_TO_ERR(g_lpfnConnectEx(socket, RemoteAddress, RemoteAddressLength, 
			lpSendBuffer, dwSendDataLength, &dwBytes, Irp));

		return Irp->CheckErrorCode(err);
	}

	// if fail begin IO, direct call with error
	IOCompletionRoutine(m_packet, cnct, ERROR_NO_SYSTEM_RESOURCES, 0, 0);
	return NOERROR;
}

ULONG CTcpEndpoint::SendFile(HANDLE hFile,
			   DWORD nNumberOfBytesToWrite,
			   DWORD nNumberOfBytesPerSend,
			   PLARGE_INTEGER ByteOffset,
			   LPTRANSMIT_FILE_BUFFERS lpTransmitBuffers,
			   DWORD dwFlags,
			   PVOID Context)
{
	ULONG err = ERROR_BAD_PIPE;

	if (LockConnection())
	{
		err = ERROR_NO_SYSTEM_RESOURCES;

		if (IO_IRP* Irp = new IO_IRP(this, send, 0, Context))
		{
			if (ByteOffset)
			{
				Irp->Offset = ByteOffset->LowPart;
				Irp->OffsetHigh = ByteOffset->HighPart;
			}
			else
			{
				Irp->Offset = 0;
				Irp->OffsetHigh = 0;
			}

			err = ERROR_INVALID_HANDLE;

			SOCKET socket;
			if (LockSocket(socket))
			{
				err = BOOL_TO_ERR(TransmitFile(
					socket, hFile, nNumberOfBytesToWrite, nNumberOfBytesPerSend, 
					Irp, lpTransmitBuffers, dwFlags));

				UnlockSocket();
			}

			return Irp->CheckErrorCode(err);
		}

		UnlockConnection();
	}

	return err;
}

ULONG CTcpEndpoint::Send(const void* Buffer, ULONG cb)
{
	ULONG dwError = ERROR_NO_SYSTEM_RESOURCES;

	if (CDataPacket* packet = new(cb) CDataPacket)
	{
		memcpy(packet->getData(), Buffer, cb);
		packet->setDataSize(cb);
		dwError = Send(packet);
		packet->Release();
	}

	return dwError;
}

ULONG CTcpEndpoint::Send(CDataPacket* packet)
{
	ULONG pad = packet->getPad();

	WSABUF wb = { packet->getDataSize() - pad, packet->getData() + pad };

	ULONG err = ERROR_BAD_PIPE;

	if (LockConnection())
	{
		err = ERROR_NO_SYSTEM_RESOURCES;

		if (IO_IRP* Irp = new IO_IRP(this, send, packet))
		{
			DWORD dwBytes;

			err = ERROR_INVALID_HANDLE;

			SOCKET socket;
			if (LockSocket(socket))
			{
				err = WSA_ERROR(WSASend(socket, &wb, 1, &dwBytes, 0, Irp, 0));

				UnlockSocket();
			}

			return Irp->CheckErrorCode(err);
		}

		UnlockConnection();
	}

	return err;
}

ULONG CTcpEndpoint::GetRecvBuffers(WSABUF lpBuffers[2], void** ppv)
{
	lpBuffers->buf = m_packet->getFreeBuffer(), lpBuffers->len = m_packet->getFreeSize();
	*ppv = lpBuffers->buf;
	return lpBuffers->len ? 1 : 0;
}

ULONG CTcpEndpoint::Recv()
{
	PVOID buf;
	WSABUF wb[2];
	DWORD dwBufferCount = GetRecvBuffers(wb, &buf);
	return Recv(wb, dwBufferCount, buf);
}

ULONG CTcpEndpoint::Recv(WSABUF* lpBuffers, DWORD dwBufferCount, PVOID buf)
{
	if (!dwBufferCount)
	{
		return ERROR_INSUFFICIENT_BUFFER;
	}

	ULONG err = ERROR_BAD_PIPE;

	if (LockConnection())
	{
		err = ERROR_NO_SYSTEM_RESOURCES;

		if (IO_IRP* Irp = new IO_IRP(this, recv, m_packet, buf))
		{
			DWORD Flags = 0;
			DWORD dwBytes;

			err = ERROR_INVALID_HANDLE;

			SOCKET socket;
			if (LockSocket(socket))
			{
				err = WSA_ERROR(WSARecv(socket, lpBuffers, dwBufferCount, &dwBytes, &Flags, Irp, 0));

				UnlockSocket();
			}

			return Irp->CheckErrorCode(err);
		}

		UnlockConnection();
	}

	return err;
}

void CTcpEndpoint::IOCompletionRoutine(CDataPacket* packet, DWORD Code, NTSTATUS dwError, ULONG_PTR dwNumberOfBytesTransfered, PVOID Pointer)
{
	BOOL f;

	if (dwError)
	{
		LogError(Code, dwError);
	}

	switch(Code) 
	{
	default: __debugbreak();
	case recv:
		dwNumberOfBytesTransfered && (f = OnRecv((PSTR)Pointer, (ULONG)dwNumberOfBytesTransfered))
			? 0 < f && Recv() : (Disconnect(dwError),0);
		break;

	case send:
		Pointer ? OnTransmitFile(dwError, Pointer) : OnSend(packet);
		if (dwError)
		{
			Disconnect(dwError);
		}
		break;

	case disc:
		// if fail disconnect, close handle
		switch (dwError)
		{
		case NOERROR:
		case ERROR_INVALID_HANDLE:
		case ERROR_CONNECTION_ABORTED:
		case WSAENOTCONN:
			break;
		default:
			IO_OBJECT::Close();
		}
		InterlockedBitTestAndResetNoFence(&m_flags, flDisconectActive);
		break;

	case lstn:
		if (!dwError) GetSockaddrs(Pointer);
	case cnct:
		if (dwError)
		{
			InterlockedBitTestAndResetNoFence(&m_flags, flListenActive);
		}
		else
		{
			m_connectionLock.Init();
		}

		f = OnConnect(dwError);

		if (!dwError)
		{
			if (dwNumberOfBytesTransfered)
			{
				if (cnct == Code)
				{
					OnSend(packet);
				}
				else
				{
					f = OnRecv((PSTR)Pointer, (ULONG)dwNumberOfBytesTransfered);
				}
			}

			f ? 0 < f && Recv() : (Disconnect(),0);
		}

		return;
	}

	// DISCONNECT, SEND, RECV
	UnlockConnection();
}

_NT_END