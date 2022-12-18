#include "stdafx.h"

_NT_BEGIN
#include "tdievent.h"
#undef DbgPrint

void DumpBytes(const UCHAR* pb, ULONG cb)
{
	if (cb)
	{
		do 
		{
			ULONG m = min(16, cb);
			cb -= m;
			char buf[128], *sz = buf;
			do 
			{
				sz += sprintf(sz, "%02x ", *pb++);
			} while (--m);
			*sz++ = '\n', *sz++ = 0;
			DbgPrint(buf);
		} while (cb);
	}
}

void pat(PCSTR msg)
{
	PVOID pv[32];
	if (ULONG n = RtlWalkFrameChain(pv, _countof(pv), 0))
	{
		DbgPrint("******* %s ******\n", msg);

		do 
		{
			DbgPrint(">> %p\n", pv[--n]);
		} while (n);

		DbgPrint("*************\n");
	}
}

//////////////////////////////////////////////////////////////////////////
NTSTATUS CTdiAddressEvt::ClearEvents()
{
	SetEventHandler(TDI_EVENT_ERROR, 0);
	SetEventHandler(TDI_EVENT_RECEIVE, 0);
	SetEventHandler(TDI_EVENT_DISCONNECT, 0);
	return SetEventHandler(TDI_EVENT_CONNECT, 0);
}

NTSTATUS CTdiAddressEvt::InitEvents()
{
	NTSTATUS status;
	//0 <= (status = SetEventHandler(TDI_EVENT_ERROR, ClientEventError)) &&
		//0 <= (status = SetEventHandler(TDI_EVENT_RECEIVE, ClientEventReceive)) &&
		//0 <= (status = SetEventHandler(TDI_EVENT_DISCONNECT, ClientEventDisconnect)) &&
		0 <= (status = SetEventHandler(TDI_EVENT_CONNECT, ClientEventConnect));
	return status;
}

NTSTATUS CTdiAddressEvt::SetEventHandler(LONG EventType, PVOID EventHandler)
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
			DeviceObject, 0, 0, 0, 0, TRUE, &Event, &iosb))
		{
			PIO_STACK_LOCATION irpSp = IoGetNextIrpStackLocation(irp);

			irpSp->MinorFunction = TDI_SET_EVENT_HANDLER;
			irpSp->FileObject = FileObject;

			PTDI_REQUEST_KERNEL_SET_EVENT p = (PTDI_REQUEST_KERNEL_SET_EVENT)&irpSp->Parameters;

			p->EventContext = EventHandler ? this : 0;
			p->EventHandler = EventHandler;
			p->EventType = EventType;

			status = IofCallDriver(DeviceObject, irp);

			DbgPrint("SetEventHandler(%x)=%x[%x,%p]\n", EventType, status, iosb.Status, iosb.Information);

			if (status == STATUS_PENDING)
			{
				KeWaitForSingleObject(&Event, WrExecutive, KernelMode, FALSE, 0);

				status = iosb.Status;

				DbgPrint("SetEventHandler2(%x)=[%x,%p]\n", EventType, status, iosb.Information);
			}
		}

		UnlockHandle();
	}

	return status;
}

BOOLEAN MoveList(_Inout_ PLIST_ENTRY FromListHead, _Inout_ PLIST_ENTRY ToListHead)
{
	PLIST_ENTRY Blink = FromListHead->Blink, Flink = FromListHead->Flink;

	if (Flink == FromListHead)
	{
		ToListHead->Blink = ToListHead, ToListHead->Flink = ToListHead;
		return FALSE;
	}

	ToListHead->Blink = Blink, Blink->Flink = ToListHead;
	ToListHead->Flink = Flink, Flink->Blink = ToListHead;
	FromListHead->Blink = FromListHead, FromListHead->Flink = FromListHead;
	return TRUE;
}

void CTdiAddressEvt::StopListen(PLIST_ENTRY head)
{
	LIST_ENTRY ListHead;

	KIRQL irq;
	KeAcquireSpinLock(&_lock, &irq);
	BOOLEAN bNotEmpty = MoveList(head, &ListHead);
	KeReleaseSpinLock(&_lock, irq);

	if (bNotEmpty)
	{
		PLIST_ENTRY entry = ListHead.Flink;

		while (entry != &ListHead)
		{
			PIRP Irp = CONTAINING_RECORD(entry, IRP, Tail.Overlay.ListEntry);

			entry = entry->Flink;

			DbgPrint("StopListen: IRP<%p> removed from list\n", Irp);

			Irp->IoStatus.Status = STATUS_CANCELLED;
			Irp->IoStatus.Information = 0;
			IofCompleteRequest(Irp, IO_NO_INCREMENT);
		}
	}
}

void CTdiAddressEvt::StopListen()
{
	StopListen(&_readHead);
	StopListen(this);
}

void CTdiAddressEvt::QueueIrp(ULONG Type, PIRP Irp, CTcpEndpointEvt* Endpoint)
{
	PLIST_ENTRY head;

	switch (Type)
	{
	case TDI_EVENT_CONNECT: head = this;
		break;
	case TDI_EVENT_RECEIVE: head = &_readHead;
		break;
	default: __assume(false);
	}
	Endpoint->PrepareIrp(Irp);
	Irp->PendingReturned = TRUE;
	KIRQL irq;
	KeAcquireSpinLock(&_lock, &irq);
	InsertHeadList(head, &Irp->Tail.Overlay.ListEntry);
	KeReleaseSpinLock(&_lock, irq);
	DbgPrint("QueueIrp<%p> <<<< %p\n", Endpoint, Irp);
}

NTSTATUS NTAPI CTdiAddressEvt::ClientEventError(
								IN PVOID  TdiEventContext,
								IN NTSTATUS  Status
								)
{
	DbgPrint("ClientEventError<%p>(%x)\n", TdiEventContext, Status);
	pat("ClientEventError");
	return 0;
}

NTSTATUS NTAPI CTdiAddressEvt::ClientEventDisconnect(
	IN PVOID  TdiEventContext,
	IN CONNECTION_CONTEXT  ConnectionContext,
	IN LONG  DisconnectDataLength,
	IN PVOID  /*DisconnectData*/,
	IN LONG  DisconnectInformationLength,
	IN PVOID  /*DisconnectInformation*/,
	IN ULONG  DisconnectFlags
	)
{
	DbgPrint("ClientEventDisconnect<%p>(%p %x %x %x)\n", TdiEventContext, ConnectionContext, 
		DisconnectDataLength, DisconnectInformationLength, DisconnectFlags);
	return 0;
}

NTSTATUS NTAPI CTdiAddressEvt::ClientEventConnect(
	IN PVOID  TdiEventContext,
	IN LONG  RemoteAddressLength,
	IN PVOID  RemoteAddress,
	IN LONG  UserDataLength,
	IN PVOID  /*UserData*/,
	IN LONG  OptionsLength,
	IN PVOID  /*Options*/,
	OUT CONNECTION_CONTEXT  *ConnectionContext,
	OUT PIRP  *AcceptIrp
	)
{
	pat("ClientEventConnect"); 
	if (!RemoteAddressLength || RemoteAddressLength > sizeof(TA_INET_ADDRESS))
	{
		DbgPrint("!!! RemoteAddressLength=%x\n", RemoteAddressLength);
		*ConnectionContext = 0;
		*AcceptIrp = 0;
		return STATUS_CONNECTION_REFUSED;
	}

	KIRQL irq;
	KeAcquireSpinLock(&reinterpret_cast<CTdiAddressEvt*>(TdiEventContext)->_lock, &irq);
	PLIST_ENTRY Entry = RemoveHeadList(reinterpret_cast<CTdiAddressEvt*>(TdiEventContext));
	KeReleaseSpinLock(&reinterpret_cast<CTdiAddressEvt*>(TdiEventContext)->_lock, irq);
	DbgPrint("ClientEventConnect<%p>(%x %x %x)\n", TdiEventContext, RemoteAddressLength, UserDataLength, OptionsLength);
	DumpBytes((UCHAR*)RemoteAddress, RemoteAddressLength);

	if (Entry == reinterpret_cast<CTdiAddressEvt*>(TdiEventContext))
	{
		DbgPrint("CONNECTION_REFUSED\n");
		*ConnectionContext = 0;
		*AcceptIrp = 0;
		return STATUS_CONNECTION_REFUSED;
	}

	PIRP Irp = CONTAINING_RECORD(Entry, IRP, Tail.Overlay.ListEntry);

	PIO_STACK_LOCATION irpSp = IoGetNextIrpStackLocation(Irp);

	if (irpSp->MinorFunction != TDI_ACCEPT)__debugbreak();

	PTDI_REQUEST_KERNEL_ACCEPT p = (PTDI_REQUEST_KERNEL_ACCEPT)&irpSp->Parameters;
	if (RemoteAddressLength <= sizeof(TA_INET_ADDRESS))
	{
		memcpy(p->ReturnConnectionInformation->RemoteAddress, RemoteAddress, RemoteAddressLength);
	}

	*AcceptIrp = Irp;
	*ConnectionContext = Irp->Tail.Overlay.OriginalFileObject->CompletionContext->Key;
	DbgPrint("Accept<%p-%p> >>>> %p\n", TdiEventContext, *ConnectionContext, Irp);

	return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS NTAPI CTdiAddressEvt::ClientEventReceive(
	IN PVOID  TdiEventContext,
	IN CONNECTION_CONTEXT ConnectionContext,
	IN ULONG  ReceiveFlags,
	IN ULONG  BytesIndicated,
	IN ULONG  BytesAvailable,
	OUT ULONG  *BytesTaken,
	IN PVOID  Tsdu,
	OUT PIRP  *IoRequestPacket
	)
{
	pat("ClientEventReceive"); 
	DbgPrint("EventReceive<%p, %p>(%x/%x %x)\n", 
		TdiEventContext, ConnectionContext, BytesIndicated, BytesAvailable, ReceiveFlags);

	*IoRequestPacket = 0;

	PIRP Irp = 0;
	KIRQL irq;
	
	KeAcquireSpinLock(&reinterpret_cast<CTdiAddressEvt*>(TdiEventContext)->_lock, &irq);
	
	PLIST_ENTRY Head = &reinterpret_cast<CTdiAddressEvt*>(TdiEventContext)->_readHead, Entry = Head;

	while ((Entry = Entry->Blink) != Head)
	{
		Irp = CONTAINING_RECORD(Entry, IRP, Tail.Overlay.ListEntry);

		if (Irp->Tail.Overlay.OriginalFileObject->CompletionContext->Key == ConnectionContext)
		{
			RemoveEntryList(Entry);
			break;
		}

		Irp = 0;
	}
	
	KeReleaseSpinLock(&reinterpret_cast<CTdiAddressEvt*>(TdiEventContext)->_lock, irq);

	if (Irp)
	{
		DbgPrint("dequeue recv IRP %p\n", Irp);

		PIO_STACK_LOCATION irpSp = IoGetNextIrpStackLocation(Irp);

		if (irpSp->MinorFunction != TDI_RECEIVE)__debugbreak();

		PTDI_REQUEST_KERNEL_RECEIVE p = (PTDI_REQUEST_KERNEL_RECEIVE)&irpSp->Parameters;

		ULONG ReceiveLength = p->ReceiveLength;

		if (ReceiveLength < BytesIndicated)
		{
			BytesIndicated = ReceiveLength;
		}

		DbgPrint("copy Tsdu to %p\n", Irp->UserBuffer);

		memcpy(Irp->UserBuffer, Tsdu, BytesIndicated);

		*BytesTaken = BytesIndicated;

		if (BytesAvailable -= BytesIndicated)
		{
		}
		DbgPrint("BytesAvailable=%x\n", BytesAvailable);

		Irp->IoStatus.Status = STATUS_SUCCESS;
		Irp->IoStatus.Information = BytesIndicated;
		IofCompleteRequest(Irp, IO_NO_INCREMENT);

		return STATUS_SUCCESS;
	}

	return STATUS_DATA_NOT_ACCEPTED;
}

NTSTATUS CTcpEndpointEvt::Listen()
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

		RtlZeroMemory(static_cast<TA_INET_ADDRESS*>(this), sizeof(TA_INET_ADDRESS));

		TDI_CONNECTION_INFORMATION ReturnConnectionInformation = {
			0, 0, 0, 0, sizeof(TA_INET_ADDRESS), static_cast<TA_INET_ADDRESS*>(this)
		};

		PIRP Irp = BuildDeviceIoControlRequest(METHOD_BUFFERED, IoGetRelatedDeviceObject(FileObject), 
			&ReturnConnectionInformation, sizeof(TDI_CONNECTION_INFORMATION), 0, 0, TRUE, cnct, 0, 0);

		if (Irp)
		{
			PIO_STACK_LOCATION irpSp = IoGetNextIrpStackLocation(Irp);

			irpSp->MinorFunction = TDI_ACCEPT;

			PTDI_REQUEST_KERNEL_ACCEPT p = (PTDI_REQUEST_KERNEL_ACCEPT)&irpSp->Parameters;

			p->RequestConnectionInformation = &zRequestConnectionInformation; 
			p->ReturnConnectionInformation = (PTDI_CONNECTION_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

			QueueIrp(TDI_EVENT_CONNECT, Irp);
		}

		UnlockHandle();

		if (Irp)
		{
			return STATUS_SUCCESS;
		}
	}

	// if fail begin IO, direct call with error
	IOCompletionRoutine(0, cnct, status, 0, 0);
	return STATUS_SUCCESS;
}

NTSTATUS CTcpEndpointEvt::Recve(PVOID Buffer, ULONG ReceiveLength)
{
	DbgPrint("%s<%p>(%p, %x)\n", __FUNCTION__, this, Buffer, ReceiveLength);

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

			PIRP Irp = BuildDeviceIoControlRequest(METHOD_NEITHER, DeviceObject, 0, 0, 
				Buffer, ReceiveLength, TRUE, recv, m_packet, Buffer);

			if (Irp)
			{
				PIO_STACK_LOCATION irpSp = IoGetNextIrpStackLocation(Irp);

				irpSp->MinorFunction = TDI_RECEIVE;

				PTDI_REQUEST_KERNEL_RECEIVE p = (PTDI_REQUEST_KERNEL_RECEIVE)&irpSp->Parameters;

				p->ReceiveFlags = TDI_RECEIVE_NORMAL;
				p->ReceiveLength = ReceiveLength;

				QueueIrp(TDI_EVENT_RECEIVE, Irp);
			}

			UnlockHandle();

			if (Irp)
			{
				return STATUS_SUCCESS;
			}
		}

		UnlockConnection();
	}

	return status;
}

_NT_END