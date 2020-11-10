#include "stdafx.h"

_NT_BEGIN

#include "mailslot.h"

void MailSlot::IOCompletionRoutine(CDataPacket* /*packet*/, DWORD Code, NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PVOID Pointer)
{
	switch (Code)
	{
	case opRead:
		if (0 > status)
		{
			switch (status)
			{
			case STATUS_FILE_FORCED_CLOSED: 
				OnServerClosed();
				break;
			case STATUS_BUFFER_TOO_SMALL: 
				OnBufferTooSmall();
				break;
			default:
				OnError(status);
			}
			break;
		}
		Pointer = OnRead(Pointer, dwNumberOfBytesTransfered);
		break;
	case opWrite:
		if (0 > status)
		{
			switch (status)
			{
			case STATUS_FILE_FORCED_CLOSED: 
				OnServerClosed();
				break;
			default:
				OnError(status);
			}
			break;
		}
		Pointer = OnWrite(Pointer, dwNumberOfBytesTransfered);
		break;
	default: __debugbreak();
	}

	BufferNotInUse(Pointer);
}

void MailSlot::OnBufferTooSmall()
{
	HANDLE hFile;

	if (LockHandle(hFile))
	{
		ULONG NextSize = 0;
		BOOL f = GetMailslotInfo(hFile, 0, &NextSize, 0, 0);
		UnlockHandle();
		if (f)
		{
			OnBufferTooSmall(NextSize);
		}
	}
}

void MailSlot::ReadWrite(PVOID Buffer, ULONG Length, ULONG op, NTSTATUS (NTAPI * fn)(
			   HANDLE ,HANDLE ,PIO_APC_ROUTINE ,PVOID ,PIO_STATUS_BLOCK ,PVOID , ULONG ,PLARGE_INTEGER, PULONG))
{
	if (NT_IRP* irp = new NT_IRP(this, op, 0, Buffer))
	{
		HANDLE hFile;
		NTSTATUS status = STATUS_INVALID_HANDLE;
		if (LockHandle(hFile))
		{
			status = fn(hFile, 0, 0, irp, irp, Buffer, Length, 0, 0);
			UnlockHandle();
		}
		irp->CheckNtStatus(status);
	}
	else
	{
		IOCompletionRoutine(0, op, STATUS_INSUFFICIENT_RESOURCES, 0, Buffer);
	}
}

void MailSlot::Read(PVOID Buffer, ULONG Length)
{
	ReadWrite(Buffer, Length, opRead, NtReadFile);
}

void MailSlot::Write(PVOID Buffer, ULONG Length)
{
	ReadWrite(Buffer, Length, opWrite, NtWriteFile);
}

NTSTATUS MailSlot::Init(HANDLE hFile)
{
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return RtlGetLastNtStatus();
	}

	Assign(hFile);

	return NT_IRP::RtlBindIoCompletion(hFile);
}

NTSTATUS MailSlot::Create(
				_In_     PCWSTR lpName,
				_In_     DWORD nMaxMessageSize,
				_In_     DWORD lReadTimeout,
				_In_opt_ PSECURITY_ATTRIBUTES lpSecurityAttributes)
{
	return Init(CreateMailslotW(lpName, nMaxMessageSize, lReadTimeout, lpSecurityAttributes));
}

NTSTATUS MailSlot::Open(PCWSTR lpName, PSECURITY_ATTRIBUTES lpSecurityAttributes)
{
	return Init(CreateFileW(lpName, FILE_WRITE_DATA, FILE_SHARE_VALID_FLAGS, lpSecurityAttributes, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0));
}

_NT_END