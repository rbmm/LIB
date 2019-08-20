#include "StdAfx.h"

_NT_BEGIN

#include "tdi.h"

//////////////////////////////////////////////////////////////////////////
// CTdiAddress

STATIC_UNICODE_STRING(Tcp, "\\Device\\Tcp");
STATIC_UNICODE_STRING(Udp, "\\Device\\Udp");

static OBJECT_ATTRIBUTES oaTcp = { 
	sizeof oaTcp, 0, (PUNICODE_STRING)&Tcp, OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE 
};

static OBJECT_ATTRIBUTES oaUdp = { 
	sizeof oaUdp, 0, (PUNICODE_STRING)&Udp, OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE 
};

NTSTATUS CTdiAddress::Create(POBJECT_ATTRIBUTES DeviceName, USHORT port, ULONG ip)
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
	pip->RA->sin_port = port;
	pip->RA->in_addr = ip;

	fei->EaValueLength = sizeof (TA_IP_ADDRESS);

	HANDLE hFile;

	NTSTATUS status = IoCreateFile(&hFile, FILE_READ_EA | FILE_WRITE_EA, 
		DeviceName, &iosb, 0, 0, 0, FILE_CREATE, 0, fei, EaSize, CreateFileTypeNone, NULL, 0);

	if (0 <= status && (0 > (status = Assign(hFile))))
	{
		NtClose(hFile);
	}

	return status;
}

NTSTATUS CTdiAddress::Create(USHORT port, ULONG ip)
{
	return Create(&oaTcp, port, ip);
}

NTSTATUS CTdiAddress::QueryInfo(LONG QueryType, PVOID buf, ULONG cb, ULONG_PTR& Information)
{
	IO_STATUS_BLOCK iosb;

	NTSTATUS status = STATUS_INVALID_HANDLE;

	HANDLE hFile;
	PFILE_OBJECT FileObject;

	if (LockHandle(hFile, FileObject))
	{
		status = STATUS_INSUFFICIENT_RESOURCES;

		PDEVICE_OBJECT DeviceObject = IoGetRelatedDeviceObject(FileObject);

		KEVENT Event;
		KeInitializeEvent(&Event, NotificationEvent, FALSE);

		if (PIRP irp = IoBuildDeviceIoControlRequest(METHOD_OUT_DIRECT, 
			DeviceObject, 0, 0, buf, cb, TRUE, &Event, &iosb))
		{
			PIO_STACK_LOCATION irpSp = IoGetNextIrpStackLocation(irp);

			irpSp->MinorFunction = TDI_QUERY_INFORMATION;
			irpSp->FileObject = FileObject;

			PTDI_REQUEST_KERNEL_QUERY_INFORMATION p = (PTDI_REQUEST_KERNEL_QUERY_INFORMATION)&irpSp->Parameters;

			p->QueryType = QueryType;
			p->RequestConnectionInformation = 0;

			status = IofCallDriver(DeviceObject, irp);

			DbgPrint("QueryInfo(%x)=%x[%x,%p] %x\n", QueryType, status, iosb.Status, iosb.Information, Event.Header.SignalState);

			if (status == STATUS_PENDING)
			{
				KeWaitForSingleObject(&Event, WrExecutive, KernelMode, FALSE, 0);

				status = iosb.Status;

				DbgPrint("QueryInfo2(%x)=[%x,%p]\n", QueryType, iosb.Status, iosb.Information);
			}

			Information = iosb.Information;
		}

		UnlockHandle();
	}

	return status;
}

NTSTATUS CTdiAddress::GetPort( PWORD port )
{
	ULONG_PTR Information;
	PTDI_ADDRESS_INFO ptia = (PTDI_ADDRESS_INFO)alloca(64);

	NTSTATUS status = QueryInfo(TDI_QUERY_ADDRESS_INFO, ptia, 64, Information);

	if (0 <= status)
	{
		if (ptia->Address.TAAddressCount && ptia->Address.Address->AddressType == TDI_ADDRESS_TYPE_IP)
		{
			*port = reinterpret_cast<TDI_ADDRESS_IP*>(ptia->Address.Address->Address)->sin_port;
		}
	}

	return status;
}

//////////////////////////////////////////////////////////////////////////
// CUdpEndpoint

NTSTATUS CUdpEndpoint::Create(USHORT port, ULONG ip)
{
	return CTdiAddress::Create(&oaUdp, port, ip);
}

NTSTATUS CUdpEndpoint::SendTo(ULONG ip, USHORT port, const void* lpData, DWORD cbData)	
{
	if (CDataPacket* packet = new(cbData) CDataPacket)
	{
		memcpy(packet->getData(), lpData, cbData);
		packet->setDataSize(cbData);
		NTSTATUS status = SendTo(ip, port, packet);
		packet->Release();
		return status;
	}

	return STATUS_INSUFFICIENT_RESOURCES;
}

NTSTATUS CUdpEndpoint::SendTo(ULONG ip, USHORT port, CDataPacket* packet )
{
	ULONG pad = packet->getPad();

	ULONG SendLength = packet->getDataSize() - pad;

	if (!SendLength) return STATUS_INVALID_DEVICE_REQUEST;

	struct CONNECTION_INFO   
	{
		TDI_CONNECTION_INFORMATION info;
		TA_IP_ADDRESS address;
	} *pci ,ci = {
		{  0, 0, 0, 0, sizeof(TA_IP_ADDRESS) },
		{ 1, { TDI_ADDRESS_LENGTH_IP, TDI_ADDRESS_TYPE_IP, { port, ip } } }
	};

	NTSTATUS status = STATUS_INVALID_HANDLE;

	HANDLE hFile;
	PFILE_OBJECT FileObject;

	if (LockHandle(hFile, FileObject))
	{
		status = STATUS_INSUFFICIENT_RESOURCES;

		PDEVICE_OBJECT DeviceObject = IoGetRelatedDeviceObject(FileObject);

		if (PIRP Irp = BuildDeviceIoControlRequest(METHOD_IN_DIRECT, DeviceObject, 
			&ci, sizeof (ci), packet->getData() + pad, SendLength, TRUE, send, packet, 0))
		{
			pci = (CONNECTION_INFO*)Irp->AssociatedIrp.SystemBuffer;

			pci->info.RemoteAddress = &pci->address;

			PIO_STACK_LOCATION irpSp = IoGetNextIrpStackLocation(Irp);

			irpSp->MinorFunction = TDI_SEND_DATAGRAM;

			PTDI_REQUEST_KERNEL_SENDDG p = (PTDI_REQUEST_KERNEL_SENDDG)&irpSp->Parameters;

			p->SendLength = SendLength;
			p->SendDatagramInformation = &pci->info;

			status = SendIrp(DeviceObject, Irp);
		}

		UnlockHandle();
	}

	return status;
}

NTSTATUS CUdpEndpoint::RecvFrom( CDataPacket* packet)
{
	ULONG ReceiveLength = packet->getFreeSize();

	if (ReceiveLength <= sizeof(TA_IP_ADDRESS)) return STATUS_BUFFER_TOO_SMALL;

	TA_IP_ADDRESS* addr = (TA_IP_ADDRESS*)packet->getFreeBuffer();

	PSTR buf = (PSTR)(addr + 1);

	ReceiveLength -= sizeof(TA_IP_ADDRESS);

	static TDI_CONNECTION_INFORMATION ReceiveDatagramInformation;

	TDI_CONNECTION_INFORMATION ReturnConnectionInformation = {
		0, 0, 0, 0, sizeof(TA_IP_ADDRESS), addr
	};

	NTSTATUS status = STATUS_INVALID_HANDLE;

	HANDLE hFile;
	PFILE_OBJECT FileObject;

	if (LockHandle(hFile, FileObject))
	{
		status = STATUS_INSUFFICIENT_RESOURCES;

		PDEVICE_OBJECT DeviceObject = IoGetRelatedDeviceObject(FileObject);

		if (PIRP Irp = BuildDeviceIoControlRequest(METHOD_OUT_DIRECT, DeviceObject, 
			&ReturnConnectionInformation, sizeof ReturnConnectionInformation, 
			buf, ReceiveLength, TRUE, recv, packet, buf))
		{
			PIO_STACK_LOCATION irpSp = IoGetNextIrpStackLocation(Irp);

			irpSp->MinorFunction = TDI_RECEIVE_DATAGRAM;

			PTDI_REQUEST_KERNEL_RECEIVEDG p = (PTDI_REQUEST_KERNEL_RECEIVEDG)&irpSp->Parameters;

			p->ReceiveLength = ReceiveLength;
			p->ReceiveFlags = TDI_RECEIVE_NORMAL;
			p->ReceiveDatagramInformation = &ReceiveDatagramInformation;
			p->ReturnDatagramInformation = (PTDI_CONNECTION_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

			status = SendIrp(DeviceObject, Irp);
		}

		UnlockHandle();
	}

	return status;
}

void CUdpEndpoint::IOCompletionRoutine(CDataPacket* packet, DWORD Code, NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PVOID Pointer)
{
	switch(Code)
	{
	case recv:
		OnRecv(status ? 0 : (PSTR)Pointer, status ? status : (ULONG)dwNumberOfBytesTransfered, packet, (TA_IP_ADDRESS*)Pointer - 1);
		break;

	case send:
		OnSend(packet);
		break;
	default: __debugbreak();
	}
}

//////////////////////////////////////////////////////////////////////////
// CTcpEndpoint

CTcpEndpoint::CTcpEndpoint(CTdiAddress* pAddress)
{
	m_flags = 0;
	m_packet = 0;
	m_pAddress = pAddress;
	pAddress->AddRef();
}

CTcpEndpoint::~CTcpEndpoint()
{
	if (m_packet) m_packet->Release();
	m_pAddress->Release();
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

NTSTATUS CTcpEndpoint::Associate(PFILE_OBJECT FileObject, HANDLE AddressHandle)
{
	IO_STATUS_BLOCK iosb;
	KEVENT Event;
	KeInitializeEvent(&Event, NotificationEvent, FALSE);

	PDEVICE_OBJECT DeviceObject = IoGetRelatedDeviceObject(FileObject);

	NTSTATUS status = STATUS_INSUFFICIENT_RESOURCES;

	if (PIRP irp = IoBuildDeviceIoControlRequest(METHOD_NEITHER, 
		DeviceObject, 0, 0, 0, 0, TRUE, &Event, &iosb))
	{
		PIO_STACK_LOCATION irpSp = IoGetNextIrpStackLocation(irp);

		irpSp->MinorFunction = TDI_ASSOCIATE_ADDRESS;
		irpSp->FileObject = FileObject;

		PTDI_REQUEST_KERNEL_ASSOCIATE p = (PTDI_REQUEST_KERNEL_ASSOCIATE)&irpSp->Parameters;

		p->AddressHandle = AddressHandle;

		status = IofCallDriver(DeviceObject, irp);

		DbgPrint("ASSOCIATE=%x,[%x,%p]\n", status, iosb.Status, iosb.Information);

		if (status == STATUS_PENDING)
		{
			KeWaitForSingleObject(&Event, WrExecutive, KernelMode, FALSE, 0);

			status = iosb.Status;

			DbgPrint("ASSOCIATE2(%x)=[%x,%p]\n", iosb.Status, iosb.Information);
		}
	}

	return status;
}

NTSTATUS CTcpEndpoint::Create(DWORD BufferSize)
{
	if (BufferSize && !(m_packet = new(BufferSize) CDataPacket)) return STATUS_INSUFFICIENT_RESOURCES;

	enum{
		EaSize = sizeof FILE_FULL_EA_INFORMATION + TDI_CONNECTION_CONTEXT_LENGTH + sizeof(PVOID)
	};

	PFILE_FULL_EA_INFORMATION fei = (PFILE_FULL_EA_INFORMATION)alloca(EaSize);

	RtlZeroMemory(fei, EaSize);

	memcpy(fei->EaName, TdiConnectionContext, fei->EaNameLength = TDI_CONNECTION_CONTEXT_LENGTH);

	fei->EaValueLength = sizeof(PVOID);
	PVOID pThis = this;
	memcpy(&fei->EaName[fei->EaNameLength + 1], &pThis, sizeof(PVOID));

	HANDLE hFile, AddressHandle;
	IO_STATUS_BLOCK iosb;

	NTSTATUS status = IoCreateFile(&hFile, FILE_READ_EA | FILE_WRITE_EA, &oaTcp, &iosb, 0, 0, 0, FILE_CREATE, 
		0, fei, EaSize, CreateFileTypeNone, NULL, 0);

	if (0 <= status)
	{
		PFILE_OBJECT FileObject, AddressFileObject;

		if (0 <= (status = ObReferenceObjectByHandle(hFile, 0, *IoFileObjectType, KernelMode, (void**)&FileObject, 0)))
		{
			status = STATUS_INVALID_HANDLE;

			if (m_pAddress->LockHandle(AddressHandle, AddressFileObject))
			{
				status = Associate(FileObject, AddressHandle);

				m_pAddress->UnlockHandle();
			}

			if (0 <= status && (0 <= (status = Assign(hFile, FileObject))))
			{
				return STATUS_SUCCESS;
			}

			ObfDereferenceObject(FileObject);
		}

		NtClose(hFile);
	}

	return status;
}

NTSTATUS CTcpEndpoint::Listen()
{
	if (InterlockedBitTestAndSetNoFence(&m_flags, flListenActive))
	{
		return STATUS_INVALID_PIPE_STATE;
	}

	NTSTATUS status = STATUS_INVALID_HANDLE;

	HANDLE hFile;
	PFILE_OBJECT FileObject;

	if (LockHandle(hFile, FileObject))
	{
		status = STATUS_INSUFFICIENT_RESOURCES;

		PDEVICE_OBJECT DeviceObject = IoGetRelatedDeviceObject(FileObject);

		static TDI_CONNECTION_INFORMATION RequestConnectionInformation;

		TDI_CONNECTION_INFORMATION ReturnConnectionInformation = {
			0, 0, 0, 0, sizeof(TA_IP_ADDRESS), static_cast<TA_IP_ADDRESS*>(this)
		};

		PIRP Irp = BuildDeviceIoControlRequest(METHOD_BUFFERED, DeviceObject, 
			&ReturnConnectionInformation, sizeof(TDI_CONNECTION_INFORMATION), 0, 0, TRUE, cnct, 0, 0);

		if (Irp)
		{
			PIO_STACK_LOCATION irpSp = IoGetNextIrpStackLocation(Irp);

			irpSp->MinorFunction = TDI_LISTEN;

			PTDI_REQUEST_KERNEL p = (PTDI_REQUEST_KERNEL)&irpSp->Parameters;

			p->RequestFlags = 0;
			p->RequestSpecific = 0;
			p->RequestConnectionInformation = &RequestConnectionInformation; 
			p->ReturnConnectionInformation = (PTDI_CONNECTION_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

			status = SendIrp(DeviceObject, Irp);
		}

		UnlockHandle();

		if (Irp)
		{
			return STATUS_SUCCESS;
		}
	}

	// if fail begin IO, direct call with error
	IOCompletionRoutine(m_packet, cnct, status, 0, 0);
	return STATUS_SUCCESS;
}

NTSTATUS CTcpEndpoint::Connect(ULONG ip, USHORT port)
{
	if (InterlockedBitTestAndSetNoFence(&m_flags, flListenActive))
	{
		return STATUS_INVALID_PIPE_STATE;
	}

	NTSTATUS status = STATUS_INVALID_HANDLE;

	HANDLE hFile;
	PFILE_OBJECT FileObject;

	if (LockHandle(hFile, FileObject))
	{
		status = STATUS_INSUFFICIENT_RESOURCES;

		PDEVICE_OBJECT DeviceObject = IoGetRelatedDeviceObject(FileObject);

		TDI_CONNECTION_INFORMATION ConnectionInformation = {
			0, 0, 0, 0, sizeof(TA_IP_ADDRESS), static_cast<TA_IP_ADDRESS*>(this)
		};

		TAAddressCount = 1;
		Address->AddressType = TDI_ADDRESS_TYPE_IP;
		Address->AddressLength = TDI_ADDRESS_LENGTH_IP;
		RA->sin_port = port;
		RA->in_addr = ip;

		PIRP Irp = BuildDeviceIoControlRequest(METHOD_BUFFERED, DeviceObject, 
			&ConnectionInformation, sizeof(TDI_CONNECTION_INFORMATION), 0, 0, TRUE, cnct, 0, 0);

		if (Irp)
		{
			PIO_STACK_LOCATION irpSp = IoGetNextIrpStackLocation(Irp);

			irpSp->MinorFunction = TDI_CONNECT;

			PTDI_REQUEST_KERNEL p = (PTDI_REQUEST_KERNEL)&irpSp->Parameters;

			p->RequestFlags = 0;
			p->RequestSpecific = 0;
			p->RequestConnectionInformation =
				p->ReturnConnectionInformation = (PTDI_CONNECTION_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

			status = SendIrp(DeviceObject, Irp);
		}

		UnlockHandle();

		if (Irp)
		{
			return STATUS_SUCCESS;
		}
	}

	// if fail begin IO, direct call with error
	IOCompletionRoutine(m_packet, cnct, status, 0, 0);
	return STATUS_SUCCESS;
}

NTSTATUS CTcpEndpoint::Send(CDataPacket* packet)
{
	if (!packet) packet = m_packet;

	if (!packet) return STATUS_INVALID_DEVICE_REQUEST;

	ULONG pad = packet->getPad();

	ULONG SendLength = packet->getDataSize() - pad;

	if (!SendLength) return STATUS_INVALID_DEVICE_REQUEST;

	NTSTATUS status = STATUS_INVALID_PIPE_STATE;

	if (LockConnection())
	{
		status = STATUS_INVALID_HANDLE;

		HANDLE hFile;
		PFILE_OBJECT FileObject;

		if (LockHandle(hFile, FileObject))
		{
			status = STATUS_INSUFFICIENT_RESOURCES;

			PDEVICE_OBJECT DeviceObject = IoGetRelatedDeviceObject(FileObject);

			PIRP Irp = BuildDeviceIoControlRequest(METHOD_IN_DIRECT, DeviceObject, 0, 0, 
				packet->getData() + pad, SendLength, TRUE, send, packet, 0);

			if (Irp)
			{
				PIO_STACK_LOCATION irpSp = IoGetNextIrpStackLocation(Irp);

				irpSp->MinorFunction = TDI_SEND;

				PTDI_REQUEST_KERNEL_SEND p = (PTDI_REQUEST_KERNEL_SEND)&irpSp->Parameters;

				p->SendFlags = 0;
				p->SendLength = SendLength;

				status = SendIrp(DeviceObject, Irp);
			}

			UnlockHandle();

			if (Irp)
			{
				return status;
			}
		}

		UnlockConnection();
	}

	return status;
}

ULONG CTcpEndpoint::GetRecvBuffer(void** ppv)
{
	*ppv = m_packet->getFreeBuffer();
	return m_packet->getFreeSize();
}

NTSTATUS CTcpEndpoint::Recv()
{
	PVOID Buffer;
	return Recv(Buffer, GetRecvBuffer(&Buffer));
}

NTSTATUS CTcpEndpoint::Recv(PVOID Buffer, ULONG ReceiveLength)
{
	if (!ReceiveLength)
	{
		return STATUS_BUFFER_TOO_SMALL;
	}

	NTSTATUS status = STATUS_INVALID_PIPE_STATE;

	if (LockConnection())
	{
		status = STATUS_INVALID_HANDLE;

		HANDLE hFile;
		PFILE_OBJECT FileObject;

		if (LockHandle(hFile, FileObject))
		{
			status = STATUS_INSUFFICIENT_RESOURCES;

			PDEVICE_OBJECT DeviceObject = IoGetRelatedDeviceObject(FileObject);

			PIRP Irp = BuildDeviceIoControlRequest(METHOD_OUT_DIRECT, DeviceObject, 0, 0, 
				Buffer, ReceiveLength, TRUE, recv, m_packet, Buffer);

			if (Irp)
			{
				PIO_STACK_LOCATION irpSp = IoGetNextIrpStackLocation(Irp);

				irpSp->MinorFunction = TDI_RECEIVE;

				PTDI_REQUEST_KERNEL_RECEIVE p = (PTDI_REQUEST_KERNEL_RECEIVE)&irpSp->Parameters;

				p->ReceiveFlags = TDI_RECEIVE_NORMAL;
				p->ReceiveLength = ReceiveLength;

				status = SendIrp(DeviceObject, Irp);
			}

			UnlockHandle();

			if (Irp)
			{
				return status;
			}
		}

		UnlockConnection();
	}

	return status;
}

void CTcpEndpoint::Disconnect(NTSTATUS status)
{
	DbgPrint("%s<%p>(%x)\n", __FUNCTION__, this, status);

	if (LockConnection())
	{
		// no more new read/write/disconnect
		RunDownConnection_l();

		switch (status)
		{
		case STATUS_INVALID_HANDLE:
		case STATUS_CONNECTION_INVALID:
		case STATUS_CONNECTION_RESET:
		case STATUS_GRACEFUL_DISCONNECT:
		case STATUS_CONNECTION_ABORTED:
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

		HANDLE hFile;
		PFILE_OBJECT FileObject;

		status = STATUS_INVALID_HANDLE;

		if (LockHandle(hFile, FileObject))
		{
			status = STATUS_INSUFFICIENT_RESOURCES;

			PDEVICE_OBJECT DeviceObject = IoGetRelatedDeviceObject(FileObject);

			PIRP Irp = BuildDeviceIoControlRequest(METHOD_NEITHER, DeviceObject, 0, 0, 0, 0, TRUE, disc, 0, 0);

			if (Irp)
			{
				PIO_STACK_LOCATION irpSp = IoGetNextIrpStackLocation(Irp);

				irpSp->MinorFunction = TDI_DISCONNECT;

				PTDI_REQUEST_KERNEL p = (PTDI_REQUEST_KERNEL)&irpSp->Parameters;

				p->RequestFlags = TDI_DISCONNECT_RELEASE;
				p->RequestSpecific = 0;
				p->RequestConnectionInformation = 0;
				p->ReturnConnectionInformation = 0;

				SendIrp(DeviceObject, Irp);
			}

			UnlockHandle();

			if (Irp)
			{
				return ;
			}
		}

		// if fail begin IO, direct call with error
		IOCompletionRoutine(0, disc, status, 0, 0);
	}
}

void CTcpEndpoint::IOCompletionRoutine(CDataPacket* packet, DWORD Code, NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PVOID Pointer)
{
	BOOL f;

	if (0 > status)
	{
		LogError(Code, status);
	}

	switch(Code) 
	{
	default: __debugbreak();

	case recv:
		dwNumberOfBytesTransfered && (f = OnRecv((PSTR)Pointer, (ULONG)dwNumberOfBytesTransfered))
			? 0 < f && Recv() : (Disconnect(status), 0);
		break;

	case send:
		OnSend(packet);
		if (0 > status)
		{
			Disconnect(status);
		}
		break;

	case disc:
		// if fail disconnect, close handle
		DbgPrint("disc:%x\n", status);
		switch (status)
		{
		case STATUS_SUCCESS:
		case STATUS_INVALID_HANDLE:
		case STATUS_INVALID_CONNECTION:
			break;
		default:
			IO_OBJECT::Close();
		}
		InterlockedBitTestAndResetNoFence(&m_flags, flDisconectActive);
		break;

	case cnct:

		if (0 > status)
		{
			InterlockedBitTestAndResetNoFence(&m_flags, flListenActive);
		}
		else
		{
			m_connectionLock.Init();
		}

		f = OnConnect(status);

		if (0 <= status)
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

			f ? 0 < f && Recv() : (Disconnect(), 0);
		}

		return;
	}

	// DISCONNECT, SEND, RECV
	UnlockConnection();
}

_NT_END