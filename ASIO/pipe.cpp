#include "StdAfx.h"
#include "..\NtVer\nt_ver.h"

_NT_BEGIN

#include "pipe.h"

NTSTATUS CreatePipeAnonymousPairPre7(PHANDLE phServerPipe, PHANDLE phClientPipe, ULONG Flags, DWORD nInBufferSize)
{
	IO_STATUS_BLOCK iosb;

	static LONG s;
	if (!s)
	{
		ULONG seed = GetTickCount();
		InterlockedCompareExchange(&s, RtlRandomEx(&seed), 0);
	}

	WCHAR name[64];

	swprintf(name, L"\\Device\\NamedPipe\\Win32Pipes.%08x.%08x", GetCurrentProcessId(), InterlockedIncrement(&s));

	UNICODE_STRING ObjectName;
	RtlInitUnicodeString(&ObjectName, name);

	OBJECT_ATTRIBUTES oa = { 
		sizeof(oa), 0, &ObjectName, 
		Flags & FLAG_PIPE_SERVER_INHERIT ? OBJ_CASE_INSENSITIVE | OBJ_INHERIT : OBJ_CASE_INSENSITIVE
	};

	NTSTATUS status;

	static LARGE_INTEGER timeout = { 0, MINLONG };

	if (0 <= (status = ZwCreateNamedPipeFile(phServerPipe,
		FILE_READ_ATTRIBUTES|FILE_READ_DATA|
		FILE_WRITE_ATTRIBUTES|FILE_WRITE_DATA|
		FILE_CREATE_PIPE_INSTANCE, 
		&oa, &iosb, FILE_SHARE_READ|FILE_SHARE_WRITE,
		FILE_CREATE, 
		Flags & FLAG_PIPE_SERVER_SYNCHRONOUS ? FILE_SYNCHRONOUS_IO_NONALERT : 0, 
		FILE_PIPE_BYTE_STREAM_TYPE, FILE_PIPE_BYTE_STREAM_MODE,
		FILE_PIPE_QUEUE_OPERATION, 1, nInBufferSize, nInBufferSize, &timeout)))
	{
		oa.Attributes = (Flags & FLAG_PIPE_CLIENT_INHERIT) ? OBJ_CASE_INSENSITIVE | OBJ_INHERIT : OBJ_CASE_INSENSITIVE;

		if (0 > (status = NtOpenFile(phClientPipe, SYNCHRONIZE|FILE_READ_ATTRIBUTES|FILE_READ_DATA|
			FILE_WRITE_ATTRIBUTES|FILE_WRITE_DATA, &oa, &iosb, FILE_SHARE_VALID_FLAGS, 
			Flags & FLAG_PIPE_CLIENT_SYNCHRONOUS ? FILE_SYNCHRONOUS_IO_NONALERT : 0)))
		{
			NtClose(*phServerPipe);
			*phServerPipe = 0;
		}
	}

	return status;
}

// win7+
NTSTATUS CreatePipeAnonymousPair(PHANDLE phServerPipe, PHANDLE phClientPipe, ULONG Flags, DWORD nInBufferSize)
{
	if (g_nt_ver.Version < _WIN32_WINNT_WIN7)
	{
		return CreatePipeAnonymousPairPre7(phServerPipe, phClientPipe, Flags, nInBufferSize);
	}

	HANDLE hFile;

	IO_STATUS_BLOCK iosb;

	STATIC_UNICODE_STRING(NamedPipe, "\\Device\\NamedPipe\\");

	OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, (PUNICODE_STRING)&NamedPipe, OBJ_CASE_INSENSITIVE };

	NTSTATUS status;

	if (0 <= (status = NtOpenFile(&hFile, SYNCHRONIZE, &oa, &iosb, FILE_SHARE_VALID_FLAGS, 0)))
	{
		oa.RootDirectory = hFile;

		static LARGE_INTEGER timeout = { 0, MINLONG };
		static UNICODE_STRING empty = {};

		oa.Attributes = (Flags & FLAG_PIPE_SERVER_INHERIT) ? OBJ_INHERIT : 0;
		oa.ObjectName = &empty;

		if (0 <= (status = ZwCreateNamedPipeFile(phServerPipe,
			FILE_READ_ATTRIBUTES|FILE_READ_DATA|
			FILE_WRITE_ATTRIBUTES|FILE_WRITE_DATA|
			FILE_CREATE_PIPE_INSTANCE|SYNCHRONIZE, 
			&oa, &iosb, FILE_SHARE_READ|FILE_SHARE_WRITE,
			FILE_CREATE, 
			Flags & FLAG_PIPE_SERVER_SYNCHRONOUS ? FILE_SYNCHRONOUS_IO_NONALERT : 0, 
			FILE_PIPE_BYTE_STREAM_TYPE, FILE_PIPE_BYTE_STREAM_MODE,
			FILE_PIPE_QUEUE_OPERATION, 1, nInBufferSize, nInBufferSize, &timeout)))
		{
			oa.RootDirectory = *phServerPipe;
			oa.Attributes = (Flags & FLAG_PIPE_CLIENT_INHERIT) ? OBJ_INHERIT : 0;

			if (0 > (status = NtOpenFile(phClientPipe, SYNCHRONIZE|FILE_READ_ATTRIBUTES|FILE_READ_DATA|
				FILE_WRITE_ATTRIBUTES|FILE_WRITE_DATA, &oa, &iosb, FILE_SHARE_VALID_FLAGS, 
				Flags & FLAG_PIPE_CLIENT_SYNCHRONOUS ? FILE_SYNCHRONOUS_IO_NONALERT : 0)))
			{
				NtClose(oa.RootDirectory);
				*phServerPipe = 0;
			}
		}

		NtClose(hFile);
	}

	return status;
}

NTSTATUS CPipeEnd::SetBuffer(ULONG InBufferSize)
{
	return (m_packet = new(InBufferSize) CDataPacket) ? 0 : STATUS_INSUFFICIENT_RESOURCES;
}

NTSTATUS CPipeEnd::Assign(HANDLE hFile)
{
	IO_OBJECT::Assign(hFile);

	if (!UseApcCompletion())
	{
		NTSTATUS status = NT_IRP::RtlBindIoCompletion(hFile);
		
		if (0 > status)
		{
			return status;
		}
	}

	if (IsServer())
	{
		return Listen();
	}

	IOCompletionRoutine(0, op_listen, 0, 0, 0);
	return STATUS_SUCCESS;
}

NTSTATUS CPipeEnd::Create(POBJECT_ATTRIBUTES poa, ULONG InBufferSize, DWORD nMaxInstances)
{
	HANDLE hFile;
	IO_STATUS_BLOCK iosb;
	static LARGE_INTEGER timeout = { 0, MINLONG };

	NTSTATUS status = IsServer() 
		?
		ZwCreateNamedPipeFile(&hFile,
		FILE_READ_ATTRIBUTES|FILE_READ_DATA|
		FILE_WRITE_ATTRIBUTES|FILE_WRITE_DATA|
		FILE_CREATE_PIPE_INSTANCE, 
		poa, &iosb, FILE_SHARE_READ|FILE_SHARE_WRITE,
		FILE_OPEN_IF, 0, FILE_PIPE_BYTE_STREAM_TYPE, FILE_PIPE_BYTE_STREAM_MODE,
		FILE_PIPE_QUEUE_OPERATION, nMaxInstances, InBufferSize, InBufferSize, &timeout)
		:
		NtOpenFile(&hFile, 
		FILE_READ_ATTRIBUTES|FILE_READ_DATA|
		FILE_WRITE_ATTRIBUTES|FILE_WRITE_DATA,
		poa, &iosb, FILE_SHARE_READ|FILE_SHARE_WRITE, 0);

	return 0 > status ? status : Assign(hFile);
}

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
	if (!cb)
	{
		return STATUS_BUFFER_OVERFLOW;
	}

	NTSTATUS status = STATUS_INVALID_PIPE_STATE;

	if (LockConnection())
	{
		status = STATUS_INSUFFICIENT_RESOURCES;

		if (NT_IRP* Irp = new NT_IRP(this, op_read, m_packet, pv))
		{
			status = STATUS_INVALID_HANDLE;

			HANDLE hPipe;
			if (LockHandle(hPipe))
			{
				status = UseApcCompletion() ? 
					NtReadFile(hPipe, 0, NT_IRP::ApcRoutine, this, Irp, pv, cb, 0, 0):
					NtReadFile(hPipe, 0,                  0,  Irp, Irp, pv, cb, 0, 0);
				UnlockHandle();
			}
			return Irp->CheckNtStatus(status);
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
		status = STATUS_INSUFFICIENT_RESOURCES;

		if (NT_IRP* Irp = new NT_IRP(this, op_write, packet))
		{
			ULONG pad = packet->getPad();

			status = STATUS_INVALID_HANDLE;

			HANDLE hPipe;
			if (LockHandle(hPipe))
			{
				PVOID pv = packet->getData() + pad;
				ULONG cb = packet->getDataSize() - pad;

				status = UseApcCompletion() ?
					NtWriteFile(hPipe, 0, NT_IRP::ApcRoutine, this, Irp, pv, cb, 0, 0):
					NtWriteFile(hPipe, 0,                  0,  Irp, Irp, pv, cb, 0, 0);

				UnlockHandle();
			}

			return Irp->CheckNtStatus(status);
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

NTSTATUS CPipeEnd::Listen()
{
	if (InterlockedBitTestAndSetNoFence(&m_flags, flListenActive))
	{
		return STATUS_INVALID_PIPE_STATE;
	}

	if (NT_IRP* Irp = new NT_IRP(this, op_listen, 0))
	{
		NTSTATUS status = STATUS_INVALID_HANDLE;

		HANDLE hPipe;
		if (LockHandle(hPipe))
		{
			status = UseApcCompletion() ?
				NtFsControlFile(hPipe, 0, NT_IRP::ApcRoutine, this, Irp, FSCTL_PIPE_LISTEN, 0, 0, 0, 0):
				NtFsControlFile(hPipe, 0,                  0,  Irp, Irp, FSCTL_PIPE_LISTEN, 0, 0, 0, 0);
			UnlockHandle();
		}
		return Irp->CheckNtStatus(status);
	}

	IOCompletionRoutine(0, op_listen, STATUS_INSUFFICIENT_RESOURCES, 0, 0);
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

		if (NT_IRP* Irp = new NT_IRP(this, op_disconnect, 0))
		{
			NTSTATUS status = STATUS_INVALID_HANDLE;

			HANDLE hPipe;
			if (LockHandle(hPipe))
			{
				status = UseApcCompletion() ?
					NtFsControlFile(hPipe, 0, NT_IRP::ApcRoutine, this, Irp, FSCTL_PIPE_DISCONNECT, 0, 0, 0, 0):
					NtFsControlFile(hPipe, 0,                  0,  Irp, Irp, FSCTL_PIPE_DISCONNECT, 0, 0, 0, 0);
				UnlockHandle();
			}

			Irp->CheckNtStatus(status);
			return ;
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
	case STATUS_PIPE_CLOSING:
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
			(f = OnConnect(STATUS_SUCCESS)) ? 0 < f && Read() : (Disconnect(), 0);
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