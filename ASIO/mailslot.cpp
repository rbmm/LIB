#include "stdafx.h"

_NT_BEGIN

#include "mailslot.h"

void MailSlot::IOCompletionRoutine(CDataPacket* packet, DWORD Code, NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PVOID Pointer)
{
	switch (Code)
	{
	case opRead:
		OnRead(status, Pointer, (ULONG)dwNumberOfBytesTransfered, packet);
		break;
	case opWrite:
		OnWrite(status, Pointer, (ULONG)dwNumberOfBytesTransfered);
		break;
	default: __debugbreak();
	}
}

void MailSlot::OnWrite(NTSTATUS status, PVOID /*buf*/, ULONG /*dwNumberOfBytesTransfered*/)
{
	if (0 > status)
	{
		if (status == STATUS_FILE_FORCED_CLOSED)
		{
			Close();
			OnServerClosed();
		}
		else
		{
			OnError(status);
		}
	}
}

NTSTATUS MailSlot::Read(CDataPacket* packet)
{
	PVOID buf = packet->getData();
	if (NT_IRP* irp = new NT_IRP(this, opRead, packet, buf))
	{
		HANDLE hFile;
		NTSTATUS status = STATUS_INVALID_HANDLE;
		if (LockHandle(hFile))
		{
			status = NtReadFile(hFile, 0, 0, irp, irp, buf, packet->getBufferSize(), 0, 0);
			UnlockHandle();
		}
		irp->CheckNtStatus(status);

		return STATUS_SUCCESS;
	}

	return STATUS_INSUFFICIENT_RESOURCES;
}

NTSTATUS MailSlot::Write(PVOID Buffer, ULONG Length)
{
	if (NT_IRP* irp = new NT_IRP(this, opWrite, 0, Buffer))
	{
		HANDLE hFile;
		NTSTATUS status = STATUS_INVALID_HANDLE;
		if (LockHandle(hFile))
		{
			status = NtWriteFile(hFile, 0, 0, irp, irp, Buffer, Length, 0, 0);
			UnlockHandle();
		}
		irp->CheckNtStatus(status);

		return STATUS_SUCCESS;
	}

	return STATUS_INSUFFICIENT_RESOURCES;
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