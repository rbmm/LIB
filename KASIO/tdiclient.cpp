#include "StdAfx.h"

_NT_BEGIN

#define DbgPrint /##/

#include "../tdi/TdiClient.h"

#define FLAG(f) (1 << (f))

BOOL FASTCALL RtlLock(PLONG p)
{
	LONG n = *p, _n;
	do 
	{
		if (n <= 0) return FALSE;
		n = InterlockedCompareExchange(p, n + 1, _n = n);
	} while(_n != n);
	return TRUE;
}

LONG g_IoActive, g_nIoThreads;
KQUEUE g_Queue;
LIST_ENTRY g_le;

BOOL LockIo()
{
	return RtlLock(&g_IoActive);
}

void UnlockIo()
{
	if (!InterlockedDecrement(&g_IoActive))
	{
		//DbgPrint("UnlockIo");
		KeInsertQueue(&g_Queue, &g_le);
	}
}

extern PLARGE_INTEGER g_ptimeout;
void OnTimeout();

void TestTimeout()
{
	if (g_ptimeout)
	{
		static LONG _iIdle;

		if (InterlockedExchange(&_iIdle, TRUE) == FALSE)
		{
			static LARGE_INTEGER _lastIdleTime;
			LARGE_INTEGER time;
			KeQuerySystemTime(&time);

			if (time.QuadPart - _lastIdleTime.QuadPart >= -g_ptimeout->QuadPart)
			{
				if (_lastIdleTime.QuadPart)
				{
					OnTimeout();
				}

				KeQuerySystemTime(&_lastIdleTime);
			}

			_iIdle = FALSE;
		}
	}
}

void OnIoStop();

VOID IoThread()
{
	PLARGE_INTEGER Timeout = g_ptimeout;

	for (;;)
	{
		PLIST_ENTRY entry = KeRemoveQueue(&g_Queue, UserMode, Timeout);

		switch((DWORD_PTR)entry)
		{
		case STATUS_TIMEOUT:
			TestTimeout();
			Timeout = 0;
		case STATUS_USER_APC:
		case STATUS_ALERTED:
			continue;
		}

		if (entry == &g_le)
		{
			//DbgPrint("IoThread(%p) end\n", KeGetCurrentThread());//$$$
			
			if (InterlockedDecrement(&g_nIoThreads))
			{
				KeInsertQueue(&g_Queue, &g_le);
			}
			else
			{
				KeRundownQueue(&g_Queue);
				OnIoStop();
			}
			return;
		}

		PIRP irp = CONTAINING_RECORD(entry, IRP, Tail.Overlay.ListEntry);

		ULONG_PTR UserApcContext = (ULONG_PTR)irp->Overlay.AsynchronousParameters.UserApcContext;
		CTdiEndpoint* p = (CTdiEndpoint*)irp->Tail.CompletionKey;
		CDataPacket* packet = (CDataPacket*)(~7 & UserApcContext);
		NTSTATUS status = irp->IoStatus.Status;
		DWORD dwNumberOfBytesTransfered = (DWORD)irp->IoStatus.Information;

		IoFreeIrp(irp);

		p->IOCompletionRoutine(packet, 7 & UserApcContext, status, dwNumberOfBytesTransfered);
		
		TestTimeout();

		Timeout = g_ptimeout;
	}
}

extern "C" void CALLBACK ThreadStartThunk(void* );

BOOL InitIo(PDRIVER_OBJECT DriverObject)
{
	KeInitializeQueue(&g_Queue, 0);
	
	ULONG Count = g_Queue.MaximumCount;

	g_DriverObject = DriverObject;	

	do 
	{
		ObfReferenceObject(DriverObject);
		HANDLE hThread;
		if (0 > PsCreateSystemThread(&hThread, 0, 0, 0, 0, ThreadStartThunk, IoThread)) 
		{
			ObfDereferenceObject(DriverObject);
		}
		else
		{
			g_IoActive = 1;
			g_nIoThreads++;
			ZwClose(hThread);
		}
	} while (--Count);

	return g_nIoThreads;
}

STATIC_UNICODE_STRING(Tcp, "\\Device\\Tcp");
STATIC_UNICODE_STRING(Udp, "\\Device\\Udp");
STATIC_UNICODE_STRING(RawIp, "\\Device\\RawIp\\0");

static OBJECT_ATTRIBUTES oaTcp = { 
	sizeof oaTcp, 0, (PUNICODE_STRING)&Tcp, OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE 
};

static OBJECT_ATTRIBUTES oaUdp = { 
	sizeof oaUdp, 0, (PUNICODE_STRING)&Udp, OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE 
};

static OBJECT_ATTRIBUTES oaRawIp = { 
	sizeof oaRawIp, 0, (PUNICODE_STRING)&RawIp, OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE 
};

NTSTATUS CreateTdiAdress(PHANDLE phFile, PFILE_OBJECT* pFileObject, POBJECT_ATTRIBUTES DeviceName, ULONG IpAddr, USHORT Port)
{
	IO_STATUS_BLOCK iosb;

	enum{
		EaSize = sizeof FILE_FULL_EA_INFORMATION + TDI_TRANSPORT_ADDRESS_LENGTH + sizeof TA_IP_ADDRESS
	};
	PFILE_FULL_EA_INFORMATION fei = (PFILE_FULL_EA_INFORMATION)alloca(EaSize);

	RtlZeroMemory(fei, EaSize);

	memcpy(fei->EaName, TdiTransportAddress, fei->EaNameLength = TDI_TRANSPORT_ADDRESS_LENGTH);

	PTA_IP_ADDRESS pip = (PTA_IP_ADDRESS)&fei->EaName[TDI_TRANSPORT_ADDRESS_LENGTH + 1];

	pip->TAAddressCount = 1;
	pip->Address->AddressType = TDI_ADDRESS_TYPE_IP;
	pip->Address->AddressLength = TDI_ADDRESS_LENGTH_IP;
	pip->RA->sin_port = Port;
	pip->RA->in_addr = IpAddr;

	fei->EaValueLength = sizeof TA_IP_ADDRESS;

	NTSTATUS status = ZwCreateFile(phFile, FILE_READ_EA | FILE_WRITE_EA, 
		DeviceName, &iosb, 0, 0, 0, FILE_CREATE, 0, fei, EaSize);

	if (0 <= status)
	{
		status = ObReferenceObjectByHandle(*phFile, 0, 0, 0, (void**)pFileObject, 0);		
	}

	return status;
}

NTSTATUS CTdiAddress::Create(ULONG IpAddr, USHORT Port)
{
	return CreateTdiAdress(&m_hFile, &m_FileObject, &oaTcp, IpAddr, Port);
}

NTSTATUS CTdiAddress::QueryInfo(LONG QueryType, LPVOID buf, ULONG cb)
{
	IO_STATUS_BLOCK iosb;

	PDEVICE_OBJECT DeviceObject = m_FileObject->DeviceObject;

	PIRP irp = IoBuildDeviceIoControlRequest(METHOD_OUT_DIRECT, 
		DeviceObject, 0, 0, buf, cb, TRUE, 0, &iosb);

	if (!irp) return STATUS_INSUFFICIENT_RESOURCES;

	PIO_STACK_LOCATION irpSp = IoGetNextIrpStackLocation(irp);

	irpSp->MinorFunction = TDI_QUERY_INFORMATION;
	irpSp->FileObject = m_FileObject;

	PTDI_REQUEST_KERNEL_QUERY_INFORMATION p = (PTDI_REQUEST_KERNEL_QUERY_INFORMATION)&irpSp->Parameters;

	p->QueryType = QueryType;
	p->RequestConnectionInformation = 0;

	return IofCallDriver(DeviceObject, irp);
}

NTSTATUS CTdiAddress::GetPort( WORD& Port )
{
	PTDI_ADDRESS_INFO ptia = (PTDI_ADDRESS_INFO)alloca(64);

	NTSTATUS status = QueryInfo(TDI_QUERY_ADDRESS_INFO, ptia, 64);

	if (0 <= status)
	{
		PTA_IP_ADDRESS p = (PTA_IP_ADDRESS)&ptia->Address;
		Port = p->RA->sin_port;
	}

	return status;
}

NTSTATUS CTdiEndpoint::CallDriver(PDEVICE_OBJECT DeviceObject, PIRP Irp , DWORD code, CDataPacket* packet)
{
	AddRef();
	if (packet) packet->AddRef();

	Irp->Overlay.AsynchronousParameters.UserApcContext = (PVOID)((DWORD_PTR)packet|code);
	Irp->UserIosb = &Irp->IoStatus;
	Irp->Tail.Overlay.OriginalFileObject = m_FileObject;
	ObfReferenceObject(m_FileObject);

	NTSTATUS status = IofCallDriver(DeviceObject, Irp);

	//DbgPrint("CallDriver(%p,%p,%d)=%X", Irp, packet, code, status);

	if (NT_ERROR(status))
	{
		IOCompletionRoutine(packet, code, status, 0);
	}
	return status;
}

CTcpEndpoint::CTcpEndpoint()
{
	m_flags = 0;
	m_packet = 0;
	m_pAddress = 0;
	m_nLock = 0;
	m_error = 0;
	m_lastIOtime.QuadPart = 0;
	m_nSendCount = 0;
}

CTcpEndpoint::~CTcpEndpoint()
{
	if (m_packet) m_packet->Release();
	if (m_pAddress) m_pAddress->Release();
}

NTSTATUS CTcpEndpoint::Create(DWORD BufferSize)
{
	if (BufferSize && !(m_packet = new(BufferSize) CDataPacket)) return STATUS_INSUFFICIENT_RESOURCES;

	enum{
		EaSize = sizeof FILE_FULL_EA_INFORMATION + TDI_CONNECTION_CONTEXT_LENGTH + sizeof LPVOID
	};

	PFILE_FULL_EA_INFORMATION fei = (PFILE_FULL_EA_INFORMATION)alloca(EaSize);

	RtlZeroMemory(fei, EaSize);

	memcpy(fei->EaName,TdiConnectionContext, fei->EaNameLength = TDI_CONNECTION_CONTEXT_LENGTH);

	fei->EaValueLength = sizeof(LPVOID);
	PVOID pThis = this;
	memcpy(&fei->EaName[fei->EaNameLength + 1], &pThis, sizeof(PVOID));

	IO_STATUS_BLOCK iosb;

	NTSTATUS status = ZwCreateFile(&m_hFile, FILE_READ_EA | FILE_WRITE_EA, &oaTcp, &iosb, 0, 0, 0, FILE_CREATE, 0, fei, EaSize);

	if (0 <= status)
	{
		if (0 <= (status = ObReferenceObjectByHandle(m_hFile, 0, 0, 0, (void**)&m_FileObject, 0)))
		{
			m_FileObject->CompletionContext = this;
			Port = &g_Queue;
			Key = static_cast<CTdiEndpoint*>(this);
		}
	}

	return status;
}

NTSTATUS CTcpEndpoint::Associate(CTdiAddress* pAddress)
{
	IO_STATUS_BLOCK iosb;

	PDEVICE_OBJECT DeviceObject = m_FileObject->DeviceObject;

	PIRP irp = IoBuildDeviceIoControlRequest(METHOD_NEITHER, 
		DeviceObject, 0, 0, 0, 0, TRUE, 0, &iosb);

	if (!irp) return STATUS_INSUFFICIENT_RESOURCES;

	PIO_STACK_LOCATION irpSp = IoGetNextIrpStackLocation(irp);

	irpSp->MinorFunction = TDI_ASSOCIATE_ADDRESS;
	irpSp->FileObject = m_FileObject;

	PTDI_REQUEST_KERNEL_ASSOCIATE p = (PTDI_REQUEST_KERNEL_ASSOCIATE)&irpSp->Parameters;

	p->AddressHandle = pAddress->m_hFile;

	NTSTATUS status = IofCallDriver(DeviceObject, irp);

	if (0 > status) return status;

	pAddress->AddRef();

	m_pAddress = pAddress;

	return 0;
}

NTSTATUS CTcpEndpoint::DiAssociate()
{
	CTdiAddress* pAddress = (CTdiAddress*)InterlockedExchangePointer((void**)&m_pAddress, 0);

	if (!pAddress) return 0;

	IO_STATUS_BLOCK iosb;

	PDEVICE_OBJECT DeviceObject = m_FileObject->DeviceObject;

	NTSTATUS status = STATUS_INSUFFICIENT_RESOURCES;

	if (PIRP irp = IoBuildDeviceIoControlRequest(METHOD_NEITHER, DeviceObject, 0, 0, 0, 0, TRUE, 0, &iosb))
	{
		PIO_STACK_LOCATION irpSp = IoGetNextIrpStackLocation(irp);

		irpSp->MinorFunction = TDI_DISASSOCIATE_ADDRESS;
		irpSp->FileObject = m_FileObject;

		status = IofCallDriver(DeviceObject, irp);
	}

	pAddress->Release();

	return status;
}

NTSTATUS CTcpEndpoint::Listen()
{
	if (!BeginConnect())
	{
		return STATUS_INVALID_DEVICE_STATE;
	}

	IO_STATUS_BLOCK iosb;

	PDEVICE_OBJECT DeviceObject = m_FileObject->DeviceObject;

	static TDI_CONNECTION_INFORMATION RequestConnectionInformation;

	TDI_CONNECTION_INFORMATION ReturnConnectionInformation = {
		0, 0, 0, 0, sizeof(TA_IP_ADDRESS), static_cast<TA_IP_ADDRESS*>(this)
	};

	PIRP irp = IoBuildDeviceIoControlRequest(METHOD_BUFFERED, DeviceObject, 
		&ReturnConnectionInformation, sizeof(TDI_CONNECTION_INFORMATION), 
		0, 0, TRUE, 0, &iosb);

	if (!irp) {
		_interlockedbittestandreset(&m_flags, flConnecting);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	PIO_STACK_LOCATION irpSp = IoGetNextIrpStackLocation(irp);

	irpSp->MinorFunction = TDI_LISTEN;
	irpSp->FileObject = m_FileObject;

	PTDI_REQUEST_KERNEL p = (PTDI_REQUEST_KERNEL)&irpSp->Parameters;

	p->RequestFlags = 0;
	p->RequestSpecific = 0;
	p->RequestConnectionInformation = &RequestConnectionInformation; 
	p->ReturnConnectionInformation = (PTDI_CONNECTION_INFORMATION)irp->AssociatedIrp.SystemBuffer;

	m_lastIOtime.QuadPart = 0;

	return CallDriver(DeviceObject, irp, connect, 0);
}

NTSTATUS CTcpEndpoint::Connect( ULONG IpAddr, USHORT port )
{
	if (!BeginConnect())
	{
		return STATUS_INVALID_DEVICE_STATE;
	}

	IO_STATUS_BLOCK iosb;

	PDEVICE_OBJECT DeviceObject = m_FileObject->DeviceObject;

	TDI_CONNECTION_INFORMATION ConnectionInformation = {
		0, 0, 0, 0, sizeof(TA_IP_ADDRESS), static_cast<TA_IP_ADDRESS*>(this)
	};

	TAAddressCount = 1;
	Address->AddressType = TDI_ADDRESS_TYPE_IP;
	Address->AddressLength = TDI_ADDRESS_LENGTH_IP;
	RA->sin_port = port;
	RA->in_addr = IpAddr;

	PIRP irp = IoBuildDeviceIoControlRequest(METHOD_BUFFERED, DeviceObject, 
		&ConnectionInformation, sizeof(TDI_CONNECTION_INFORMATION), 
		0, 0, TRUE, 0, &iosb);

	if (!irp) {
		_interlockedbittestandreset(&m_flags, flConnecting);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	PIO_STACK_LOCATION irpSp = IoGetNextIrpStackLocation(irp);

	irpSp->MinorFunction = TDI_CONNECT;
	irpSp->FileObject = m_FileObject;

	PTDI_REQUEST_KERNEL p = (PTDI_REQUEST_KERNEL)&irpSp->Parameters;

	p->RequestFlags = 0;
	p->RequestSpecific = 0;
	p->RequestConnectionInformation =
	p->ReturnConnectionInformation = (PTDI_CONNECTION_INFORMATION)irp->AssociatedIrp.SystemBuffer;

	return CallDriver(DeviceObject, irp, connect, 0);
}

NTSTATUS CTcpEndpoint::RealDisconnect()
{
	IO_STATUS_BLOCK iosb;

	PDEVICE_OBJECT DeviceObject = m_FileObject->DeviceObject;

	PIRP irp = IoBuildDeviceIoControlRequest(METHOD_NEITHER, DeviceObject, 0, 0, 0, 0, TRUE, 0, &iosb);

	if (!irp) {
		_interlockedbittestandreset(&m_flags, flDisconnecting);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	PIO_STACK_LOCATION irpSp = IoGetNextIrpStackLocation(irp);

	irpSp->MinorFunction = TDI_DISCONNECT;
	irpSp->FileObject = m_FileObject;

	PTDI_REQUEST_KERNEL p = (PTDI_REQUEST_KERNEL)&irpSp->Parameters;

	p->RequestFlags = TDI_DISCONNECT_RELEASE;
	p->RequestSpecific = 0;
	p->RequestConnectionInformation = 0;
	p->ReturnConnectionInformation = 0;

	return CallDriver(DeviceObject, irp, disconnect, 0);
}

NTSTATUS CTcpEndpoint::Recv()
{
	if (!m_packet) return STATUS_INVALID_DEVICE_REQUEST;

	ULONG ReceiveLength = m_packet->getFreeSize();
	
	if (_bittest(&m_flags, flNotRecv) || !ReceiveLength || !Lock()) return STATUS_INVALID_DEVICE_STATE;

	if (_interlockedbittestandset(&m_flags, flRecvActive))
	{
		Unlock(STATUS_INVALID_DEVICE_STATE);
		return STATUS_INVALID_DEVICE_STATE;
	}

	IO_STATUS_BLOCK iosb;

	PDEVICE_OBJECT DeviceObject = m_FileObject->DeviceObject;

	PIRP irp = IoBuildDeviceIoControlRequest(METHOD_OUT_DIRECT, DeviceObject, 
		0, 0, m_packet->getFreeBuffer(), ReceiveLength, TRUE, 0, &iosb);

	if (!irp) {
		_interlockedbittestandreset(&m_flags, flRecvActive);
		Unlock(m_error);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	PIO_STACK_LOCATION irpSp = IoGetNextIrpStackLocation(irp);

	irpSp->MinorFunction = TDI_RECEIVE;
	irpSp->FileObject = m_FileObject;

	PTDI_REQUEST_KERNEL_RECEIVE p = (PTDI_REQUEST_KERNEL_RECEIVE)&irpSp->Parameters;

	p->ReceiveFlags = TDI_RECEIVE_NORMAL;
	p->ReceiveLength = ReceiveLength;

	return CallDriver(DeviceObject, irp, recv, m_packet);
}

NTSTATUS CTcpEndpoint::Send(CDataPacket* packet)
{
	if (!packet) packet = m_packet;

	if (!packet) return STATUS_INVALID_DEVICE_REQUEST;

	ULONG SendLength = packet->getDataSize();

	if (!SendLength) return STATUS_INVALID_DEVICE_REQUEST;

	if (!Lock()) 
	{
		return STATUS_INVALID_DEVICE_STATE;
	}

	if (!LockDisconect())
	{
		Unlock(STATUS_INVALID_DEVICE_STATE);
		return STATUS_INVALID_DEVICE_STATE;
	}

	IO_STATUS_BLOCK iosb;

	PDEVICE_OBJECT DeviceObject = m_FileObject->DeviceObject;

	PIRP irp = IoBuildDeviceIoControlRequest(METHOD_OUT_DIRECT, DeviceObject, 0, 0, packet->getData(), SendLength, TRUE, 0, &iosb);

	if (!irp) {
		UnlockDisconect();
		Unlock(STATUS_INSUFFICIENT_RESOURCES);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	PIO_STACK_LOCATION irpSp = IoGetNextIrpStackLocation(irp);

	irpSp->MinorFunction = TDI_SEND;
	irpSp->FileObject = m_FileObject;

	PTDI_REQUEST_KERNEL_SEND p = (PTDI_REQUEST_KERNEL_SEND)&irpSp->Parameters;

	p->SendFlags = 0;
	p->SendLength = SendLength;

	return CallDriver(DeviceObject, irp, send, packet);
}

NTSTATUS CTcpEndpoint::SetErrorStatus(NTSTATUS status)
{
	InterlockedCompareExchange(&m_error, status, 0);
	return m_error;
}

BOOL CTcpEndpoint::Lock()
{
	return RtlLock(&m_nLock);
}

void CTcpEndpoint::Unlock(NTSTATUS status)
{
	if (!InterlockedDecrement(&m_nLock)) 
	{
		m_lastIOtime.QuadPart = 0;
		_interlockedbittestandreset(&m_flags, flConnected);
		_interlockedbittestandreset(&m_flags, flDisconnecting);
		OnDisconnect(status);
	}
}

NTSTATUS CTcpEndpoint::Disconnect()
{
	return BeginDisconnect() ? UnlockDisconect() : STATUS_INVALID_DEVICE_STATE;
}

NTSTATUS CTcpEndpoint::UnlockDisconect()
{
	return InterlockedDecrement(&m_nSendCount) ? 0 : RealDisconnect();
}

inline BOOL CTcpEndpoint::LockDisconect()
{
	return RtlLock(&m_nSendCount);
}

BOOL CTcpEndpoint::BeginDisconnect()
{
	LONG flags = m_flags, _flags;

	do 
	{
		if ((flags & ( FLAG(flConnecting)|FLAG(flConnected)|FLAG(flDisconnecting) )) != FLAG(flConnected) )
		{
			return FALSE;
		}

		flags = InterlockedCompareExchange(&m_flags, flags|FLAG(flDisconnecting), _flags = flags);

	} while (flags != _flags);

	return TRUE;
}

BOOL CTcpEndpoint::BeginConnect()
{
	LONG flags = m_flags, _flags;

	do 
	{

		if (flags & ( FLAG(flConnecting)|FLAG(flConnected)|FLAG(flDisconnecting) ))
		{
			return FALSE;
		}

		flags = InterlockedCompareExchange(&m_flags, flags|FLAG(flConnecting), _flags = flags);

	} while (flags != _flags);

	return TRUE;
}

VOID CTcpEndpoint::IOCompletionRoutine( CDataPacket* packet, DWORD Code, NTSTATUS status, DWORD dwNumberOfBytesTransfered )
{
	//DbgPrint("%p:IOCompletionRoutine(%u)=%X\n", KeGetCurrentThread(), Code, status);//$$$

	if (Code != disconnect) KeQuerySystemTime(&m_lastIOtime);

	switch(Code)
	{
	case connect:

		if (0 <= status) 
		{
			m_nLock = 1, m_error = 0, m_nSendCount = 1;
			_interlockedbittestandset(&m_flags, flConnected);
		}
		_interlockedbittestandreset(&m_flags, flConnecting);

		if (OnConnect(status) && (0 <= status)) Recv();

		break;

	case disconnect:
		Unlock(SetErrorStatus(status));
		break;

	case recv:

		_interlockedbittestandreset(&m_flags, flRecvActive);
		if (!dwNumberOfBytesTransfered) 
		{
			if (status != STATUS_CANCELLED) Disconnect();
		}
		else
		{
			if (OnRecv(packet->getFreeBuffer(), dwNumberOfBytesTransfered)) 
			{
				Recv();	
			}
			else 
			{
				Disconnect();
			}
		}
		Unlock(SetErrorStatus(status));
		break;

	case send:
		OnSend(packet);
		UnlockDisconect();
		if ((0 > status) && (status != STATUS_CANCELLED)) Disconnect();
		Unlock(SetErrorStatus(status));
		break;
	default: __debugbreak();
	}

	if (packet) packet->Release();
	Release();
}

NTSTATUS CUdpEndpoint::CommonCreate(WORD sinPort, POBJECT_ATTRIBUTES poa)
{
	NTSTATUS status = CreateTdiAdress(&m_hFile, &m_FileObject, poa, 0, sinPort);
	if (0 <= status)
	{
		m_FileObject->CompletionContext = this;
		Port = &g_Queue;
		Key = static_cast<CTdiEndpoint*>(this);
	}
	return status;
}

NTSTATUS CUdpEndpoint::Create(WORD port)
{
	return CommonCreate(port, &oaUdp);
}

NTSTATUS CUdpEndpoint::CreateRaw()
{
	NTSTATUS status = CommonCreate(0, &oaRawIp);
	//DbgPrint("CreateRaw()=%x", status);
	if (0 <= status)
	{

#define IOCTL_TCP_SET_INFORMATION_EX 0x120028

		static DWORD req[] = { 0x400, 0, 0x200, 0x200, 0xc, 0x4, 0x1 };
		IO_STATUS_BLOCK iosb;
		status = ZwDeviceIoControlFile(m_hFile, 0, 0, 0, &iosb, IOCTL_TCP_SET_INFORMATION_EX, req, sizeof(req), 0, 0);

		//DbgPrint("--IOCTL_TCP_SET_INFORMATION_EX=%x", status);
	}
	return status;
}

NTSTATUS CUdpEndpoint::SendTo( ULONG IpAddr, USHORT sin_port, CDataPacket* packet )
{
	ULONG SendLength = packet->getDataSize();

	if (!SendLength) return STATUS_INVALID_DEVICE_REQUEST;

	IO_STATUS_BLOCK iosb;

	PDEVICE_OBJECT DeviceObject = m_FileObject->DeviceObject;

	struct CONNECTION_INFO   
	{
		TDI_CONNECTION_INFORMATION info;
		TA_IP_ADDRESS address;
	} *pci ,ci = 
	{
		{ 
			0, 0, 0, 0, sizeof(TA_IP_ADDRESS) 
		},
		{ 
			1, { TDI_ADDRESS_LENGTH_IP, TDI_ADDRESS_TYPE_IP, { sin_port, IpAddr } }
		}
	};

	PIRP irp = IoBuildDeviceIoControlRequest(METHOD_OUT_DIRECT, DeviceObject, 
		&ci, sizeof ci, packet->getData(), SendLength, TRUE, 0, &iosb);

	if (!irp) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	pci = (CONNECTION_INFO*)irp->AssociatedIrp.SystemBuffer;

	pci->info.RemoteAddress = &pci->address;

	PIO_STACK_LOCATION irpSp = IoGetNextIrpStackLocation(irp);

	irpSp->MinorFunction = TDI_SEND_DATAGRAM;
	irpSp->FileObject = m_FileObject;

	PTDI_REQUEST_KERNEL_SENDDG p = (PTDI_REQUEST_KERNEL_SENDDG)&irpSp->Parameters;

	p->SendLength = SendLength;
	p->SendDatagramInformation = &pci->info;

	return CallDriver(DeviceObject, irp, send, packet);
}

NTSTATUS CUdpEndpoint::RecvFrom( CDataPacket* packet, TA_IP_ADDRESS* addr )
{
	ULONG ReceiveLength = packet->getFreeSize();

	if (!ReceiveLength) return STATUS_INSUFFICIENT_RESOURCES;

	IO_STATUS_BLOCK iosb;

	PDEVICE_OBJECT DeviceObject = m_FileObject->DeviceObject;

	static TDI_CONNECTION_INFORMATION ReceiveDatagramInformation;

	TDI_CONNECTION_INFORMATION ReturnConnectionInformation = {
		0, 0, 0, 0, sizeof(TA_IP_ADDRESS), addr
	};

	PIRP irp = IoBuildDeviceIoControlRequest(METHOD_OUT_DIRECT, DeviceObject, 
		&ReturnConnectionInformation, sizeof ReturnConnectionInformation, 
		packet->getFreeBuffer(), ReceiveLength, TRUE, 0, &iosb);

	if (!irp) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	PIO_STACK_LOCATION irpSp = IoGetNextIrpStackLocation(irp);

	irpSp->MinorFunction = TDI_RECEIVE_DATAGRAM;
	irpSp->FileObject = m_FileObject;

	PTDI_REQUEST_KERNEL_RECEIVEDG p = (PTDI_REQUEST_KERNEL_RECEIVEDG)&irpSp->Parameters;

	p->ReceiveLength = ReceiveLength;
	p->ReceiveFlags = TDI_RECEIVE_NORMAL;
	p->ReceiveDatagramInformation = &ReceiveDatagramInformation;
	p->ReturnDatagramInformation = (PTDI_CONNECTION_INFORMATION)irp->AssociatedIrp.SystemBuffer;

	return CallDriver(DeviceObject, irp, recv, packet);
}

VOID CUdpEndpoint::IOCompletionRoutine( CDataPacket* packet, DWORD Code, NTSTATUS status, DWORD dwNumberOfBytesTransfered )
{
	switch(Code)
	{
	case recv:
		OnRecv(0 > status ? 0 : packet->getFreeBuffer(), 0 > status ? status : dwNumberOfBytesTransfered, packet);
		break;

	case send:
		OnSend(packet);
		break;
	default: __debugbreak();
	}

	if (packet) packet->Release();
	Release();
}

CTdiIo::CTdiIo()
{
	KeInitializeTimer(this);
	KeInitializeDpc(this, OnDpc, 0);
	ExInitializeWorkItem(this, (PWORKER_THREAD_ROUTINE)OnWorkItem, this);
}

void CTdiIo::Start(LARGE_INTEGER dueTime)
{
	AddRef();
	KeSetTimer(this, dueTime, this);
}

void CTdiIo::_Stop()
{
	Close();
	OnStop();
	Release();
}

void CTdiIo::Stop()
{
	if (KeCancelTimer(this)) _Stop();
}

VOID CTdiIo::OnDpc(PKDPC Dpc, PVOID , PVOID , PVOID )
{
	//DbgPrint("OnDpc(%p)\n", Dpc);
	ExQueueWorkItem(static_cast<CTdiIo*>(Dpc), DelayedWorkQueue);
}

VOID CTdiIo::OnWorkItem(CTdiIo* This)
{
	//DbgPrint("OnWorkItem(%p)\n", This);
	This->_Stop();
}

//////////////////////////////////////////////////////////////////////////

class CDnsSocket : public CUdpEndpoint
{
	CTdiFile* _rslv;
	DWORD _ip;

	virtual void OnStop()
	{
		_rslv->OnIp(_ip);
	}

	virtual void OnRecv(LPSTR Buffer, ULONG cbTransferred, CDataPacket* )
	{
		if (Buffer) OnRecv(Buffer, cbTransferred);
		Stop();
	}

	virtual void OnRecv(PSTR Buffer, ULONG cbTransferred)
	{
		if (cbTransferred < 13) return ;

		Buffer += 12, cbTransferred -= 12;

		UCHAR c;

		while (c = *Buffer++)
		{
			if (cbTransferred < (DWORD)(2 + c)) return;
			cbTransferred -= 1 + c;
			Buffer += c;
		}
		Buffer += 4;
		if (cbTransferred < 4) return ;
		cbTransferred -= 4;

		struct DNS_RR
		{
			WORD name, type, cls, ttl1, ttl2, len;
		} x;

		for(;;) 
		{
			if (cbTransferred < sizeof DNS_RR) return;
			memcpy(&x, Buffer, sizeof x);
			cbTransferred -= sizeof DNS_RR, Buffer+= sizeof DNS_RR;
			x.len = _byteswap_ushort(x.len);
			if (cbTransferred<x.len) return;
			cbTransferred-=x.len;
			if (x.type==0x100 && x.cls == 0x100 && x.len == sizeof DWORD)
			{
				memcpy(&_ip, Buffer, sizeof DWORD);
				return;
			}
			Buffer+=x.len;
		}
	}

public:

	void Resolve(PCSTR Dns, DWORD ip)
	{
		if (CDataPacket* packet = new(1024) CDataPacket)
		{
			LPSTR __lpsz = packet->getData(), _lpsz, lpsz = __lpsz;
			char c, i;
			static WORD bb1[6]={ 0x1111, 1, 0x0100 };
			static WORD bb2[2]={ 0x0100, 0x0100 };
			memcpy(lpsz, bb1, sizeof bb1);
			lpsz += sizeof bb1;

			do 
			{
				_lpsz = lpsz++, i = 0;
mm:
				switch (c = *Dns++)
				{
				case '.':
				case 0:
					break;
				default:*lpsz++ = c, ++i;
					goto mm;
				}
				*_lpsz = i;
			} while (c);

			*lpsz++ = 0;

			memcpy(lpsz, bb2, sizeof bb2);

			packet->setDataSize(RtlPointerToOffset(__lpsz, lpsz) + sizeof(bb2));
			SendTo(ip, 0x3500, packet);

			packet->Release();
		}
	}

	CDnsSocket(CTdiFile* rslv)
	{
		_ip = 0;
		_rslv = rslv;
		rslv->AddRef();
	}

	~CDnsSocket()
	{
		_rslv->Release();
	}
};

void CTdiFile::DnsToIp(PCSTR Dns)
{
	if (CDnsSocket* p = new CDnsSocket(this))
	{
		if (0 <= p->Create(0))
		{
			if (CDataPacket* packet = new(1024) CDataPacket)
			{
				packet->setDataSize(sizeof(TA_IP_ADDRESS));
				NTSTATUS status = p->RecvFrom(packet, (TA_IP_ADDRESS*)packet->getData());
				packet->Release();

				if (status == STATUS_PENDING)
				{
					LARGE_INTEGER li;
					li.QuadPart = -50000000;

					p->Start(li);
					p->Resolve(Dns, IP(8,8,8,8));
				}
			}
		}
		p->Release();
	}
}

_NT_END
