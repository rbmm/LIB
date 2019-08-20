#include "StdAfx.h"

_NT_BEGIN

#include "pipe.h"

NTSTATUS CPipeEnd::SetBuffer(ULONG InBufferSize)
{
	return (m_packet = new(InBufferSize) CDataPacket) ? 0 : STATUS_INSUFFICIENT_RESOURCES;
}

NTSTATUS CPipeEnd::Assign(HANDLE hFile)
{
	NTSTATUS status = IO_OBJECT::Assign(hFile);

	if (0 <= status)
	{
		if (IsServer())
		{
			return Listen();
		}

		IOCompletionRoutine(0, op_listen, 0, 0, 0);
		return 0;
	}

	return status;
}

NTSTATUS CPipeEnd::Create(POBJECT_ATTRIBUTES poa, ULONG InBufferSize/* = 0*/, DWORD nMaxInstances/* = PIPE_UNLIMITED_INSTANCES*/)
{
	HANDLE hFile;
	IO_STATUS_BLOCK iosb;

	NTSTATUS status;

	if (IsServer())
	{
		NAMED_PIPE_CREATE_PARAMETERS namedPipeCreateParameters = {
			FILE_PIPE_BYTE_STREAM_TYPE, 
			FILE_PIPE_BYTE_STREAM_MODE,
			FILE_PIPE_QUEUE_OPERATION, 
			nMaxInstances, 
			InBufferSize, 
			InBufferSize, 
			{ 0, MINLONG }, 
			TRUE
		};

		status = IoCreateFile( &hFile,
			FILE_READ_ATTRIBUTES|FILE_READ_DATA|
			FILE_WRITE_ATTRIBUTES|FILE_WRITE_DATA|
			FILE_CREATE_PIPE_INSTANCE,
			poa,
			&iosb,
			NULL,
			0,
			FILE_SHARE_READ|FILE_SHARE_WRITE,
			FILE_OPEN_IF,
			0,
			NULL,
			0,
			CreateFileTypeNamedPipe,
			&namedPipeCreateParameters,
			0 );
	}
	else
	{
		status = IoCreateFile( &hFile,
			FILE_READ_ATTRIBUTES|FILE_READ_DATA|
			FILE_WRITE_ATTRIBUTES|FILE_WRITE_DATA,
			poa,
			&iosb,
			NULL,
			0,
			FILE_SHARE_READ|FILE_SHARE_WRITE,
			FILE_OPEN,
			0,
			NULL,
			0,
			CreateFileTypeNone,
			NULL,
			0 );
	}

	return 0 > status ? status : Assign(hFile);
}

static LARGE_INTEGER NpStartingOffset;

ULONG CPipeEnd::GetReadBuffer(void** ppv)
{
	*ppv = m_packet->getFreeBuffer();
	return m_packet->getFreeSize();
}

NTSTATUS CPipeEnd::Read()
{
	PVOID buf;
	ULONG cb = GetReadBuffer(&buf);

	return Read(buf, cb);
}

NTSTATUS CPipeEnd::Read(PVOID pv, ULONG cb)
{
	NTSTATUS status = STATUS_INVALID_PIPE_STATE;

	if (LockConnection())
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		HANDLE hPipe;
		PFILE_OBJECT FileObject;

		if (LockHandle(hPipe, FileObject))
		{
			PDEVICE_OBJECT DeviceObject = IoGetRelatedDeviceObject(FileObject);

			PIRP Irp = BuildSynchronousFsdRequest(IRP_MJ_READ, DeviceObject, 
				pv, cb, &NpStartingOffset, op_read, m_packet, pv);

			if (Irp)
			{
				status = SendIrp(DeviceObject, Irp);
			}

			UnlockHandle();

			if (Irp)
			{
				return 0;
			}
		}

		UnlockConnection();
	}

	return status;
}

NTSTATUS CPipeEnd::Write(CDataPacket* packet)
{
	NTSTATUS status = STATUS_INVALID_PIPE_STATE;

	if (LockConnection())
	{
		status = STATUS_INVALID_HANDLE;

		HANDLE hPipe;
		PFILE_OBJECT FileObject;

		if (LockHandle(hPipe, FileObject))
		{
			status = STATUS_INSUFFICIENT_RESOURCES;

			PDEVICE_OBJECT DeviceObject = IoGetRelatedDeviceObject(FileObject);

			ULONG pad = packet->getPad();

			PIRP Irp = BuildSynchronousFsdRequest(IRP_MJ_WRITE, DeviceObject, 
				packet->getData() + pad, packet->getDataSize() - pad, &NpStartingOffset, op_write, packet, 0);

			if (Irp)
			{
				status = SendIrp(DeviceObject, Irp);
			}

			UnlockHandle();

			if (Irp)
			{
				return 0;
			}
		}

		UnlockConnection();
	}

	return status;
}

NTSTATUS CPipeEnd::Write(const void* lpData, DWORD cbData)
{
	if (CDataPacket* packet = new(cbData) CDataPacket)
	{
		memcpy(packet->getData(), lpData, cbData);
		packet->setDataSize(cbData);
		NTSTATUS status = Write(packet);
		packet->Release();
		return status;
	}

	return STATUS_INSUFFICIENT_RESOURCES;
}

void AdjustIrpForFSC(PIRP Irp)
{
	PIO_STACK_LOCATION IrpSp = IoGetNextIrpStackLocation(Irp);
	IrpSp->MajorFunction = IRP_MJ_FILE_SYSTEM_CONTROL;
	IrpSp->MinorFunction = IRP_MN_USER_FS_REQUEST;
}

NTSTATUS CPipeEnd::Listen()
{
	if (InterlockedBitTestAndSetNoFence(&m_flags, flListenActive))
	{
		return STATUS_INVALID_PIPE_STATE;
	}

	NTSTATUS status = STATUS_INVALID_HANDLE;

	HANDLE hPipe;
	PFILE_OBJECT FileObject;

	if (LockHandle(hPipe, FileObject))
	{
		status = STATUS_INSUFFICIENT_RESOURCES;

		PDEVICE_OBJECT DeviceObject = IoGetRelatedDeviceObject(FileObject);

		PIRP Irp = BuildDeviceIoControlRequest(FSCTL_PIPE_LISTEN, 
			DeviceObject, 0, 0, 0, 0, TRUE, op_listen, 0, 0);

		if (Irp)
		{
			AdjustIrpForFSC(Irp);
			status = SendIrp(DeviceObject, Irp);
		}

		UnlockHandle();

		if (Irp)
		{
			return 0;
		}
	}

	IOCompletionRoutine(0, op_listen, status, 0, 0);
	return 0;
}

void CPipeEnd::Close()
{
	IO_OBJECT::Close();

	if (LockConnection())
	{
		RunDownConnection_l();
		UnlockConnection();
	}
}

void CPipeEnd::Disconnect()
{
	if (!IsServer())
	{
		// client can disconnect only via close handle
		Close();
		return ;
	}

	if (LockConnection())
	{
		// no more new read/write/disconnect
		RunDownConnection_l();

		// only once
		if (InterlockedBitTestAndSetNoFence(&m_flags, flDisconectActive))
		{
			UnlockConnection();
			return ;
		}

		HANDLE hPipe;
		PFILE_OBJECT FileObject;

		if (LockHandle(hPipe, FileObject))
		{
			PDEVICE_OBJECT DeviceObject = IoGetRelatedDeviceObject(FileObject);

			PIRP Irp = BuildDeviceIoControlRequest(FSCTL_PIPE_DISCONNECT, 
				DeviceObject, 0, 0, 0, 0, TRUE, op_disconnect, 0, 0);

			if (Irp)
			{
				AdjustIrpForFSC(Irp);
				SendIrp(DeviceObject, Irp);
			}

			UnlockHandle();

			if (Irp)
			{
				return;
			}
		}

		// if fail begin IO, direct call with error
		IOCompletionRoutine(0, op_disconnect, STATUS_INSUFFICIENT_RESOURCES, 0, 0);
	}
}

VOID CPipeEnd::OnUnknownCode(CDataPacket* /*packet*/, DWORD /*Code*/, NTSTATUS /*status*/, ULONG_PTR /*dwNumberOfBytesTransfered*/, PVOID /*Pointer*/)
{
	__debugbreak();
}

void CPipeEnd::OnReadWriteError(NTSTATUS status)
{
	switch (status)
	{
	case STATUS_CANCELLED:			// CancelIo[Ex]
		//break;					// what todo ? let Disconnect
	case STATUS_PIPE_BROKEN:		// pipe handle has been closed
		Disconnect();				// server must call DisconnectNamedPipe
		break;
	case STATUS_PIPE_DISCONNECTED:	// server call DisconnectNamedPipe
	case STATUS_INVALID_HANDLE:		// we close handle before I/O
		RunDownConnection_l();		// just activate OnDisconnect call
		break;
	default:/*__debugbreak();*/		// ? must not be
		if (0 > status) Close();
	}
}

void CPipeEnd::IOCompletionRoutine(CDataPacket* packet, DWORD Code, NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PVOID Pointer)
{
	if (status)
	{
		ULONG64 u = Code;
		DbgPrint("%c>%s %x\n", IsServer() ? 's' : 'c', &u, status);
	}
	BOOL f;

	switch(Code) 
	{
	case op_read:

		switch(status) 
		{
		case STATUS_SUCCESS:
			(f = OnRead(Pointer, (ULONG)dwNumberOfBytesTransfered)) ? 0 < f && Read() : (Disconnect(), 0);
			break;
		default: OnReadWriteError(status);
		}
		break;

	case op_write:

		OnWrite(packet, status);
		if (status)
		{
			OnReadWriteError(status);
		}
		break;

	case op_disconnect:

		switch (status)
		{
		case STATUS_SUCCESS:
		case STATUS_INVALID_HANDLE:
			break;
		default:
			// if fail disconnect, close handle
			if (0 > status) IO_OBJECT::Close();
		}
		InterlockedBitTestAndResetNoFence(&m_flags, flDisconectActive);
		break;

	case op_listen:

		switch(status) 
		{
		case STATUS_SUCCESS:		// client just connected
		case STATUS_PIPE_CONNECTED: // client already connected
		case STATUS_PIPE_CLOSING:	// client already connected and disconnected
			m_connectionLock.Init();
			(f = OnConnect(STATUS_SUCCESS)) ? 0 < f && Read() : (Disconnect(),0);
			break;
			//case STATUS_CANCELLED:			// CancelIo[Ex]
			//case STATUS_INVALID_HANDLE:		// we close handle before ConnectNamedPipe call  
			//case STATUS_PIPE_BROKEN:			// server call CloseHandle before ConnectNamedPipe complete
			//case STATUS_PIPE_DISCONNECTED:	// server call DisconnectNamedPipe before ConnectNamedPipe
		default:
			InterlockedBitTestAndResetNoFence(&m_flags, flListenActive);
			OnConnect(status);
		}
		return;

	default: 
		OnUnknownCode(packet, Code, status, dwNumberOfBytesTransfered, Pointer);
	}

	// DISCONNECT, WRITE, READ
	UnlockConnection();
}

_NT_END