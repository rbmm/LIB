#include "stdafx.h"

//#undef DbgPrint
_NT_BEGIN

#include "minidbg.h"
#include "DbgDoc.h"

void DumpBuffer(PBYTE pb, ULONG cb)
{
	if (!cb) return;
	DbgPrint("//========== %x\n", cb);
	cb = (cb + 3) & ~3;
	do 
	{
		DbgPrint("%08x\n", *(PULONG)pb);
	} while (pb += 4, cb -= 4);
	DbgPrint("//---------- %x\n", cb);
}

ULONG KdpComputeChecksum (PUCHAR Buffer,ULONG Length)
{
	if (!Length) return 0;
	ULONG Checksum = 0;
	do Checksum += *Buffer++; while (--Length);
	return Checksum;
}

PCSTR GetName(PACKET_KD_TYPE type)
{
	switch (type)
	{
	case PACKET_TYPE_KD_STATE_MANIPULATE: return "STATE_MANIPULATE";
	case PACKET_TYPE_KD_DEBUG_IO: return "DEBUG_IO";
	case PACKET_TYPE_KD_ACKNOWLEDGE: return "ACKNOWLEDGE";
	case PACKET_TYPE_KD_RESEND: return "RESEND";
	case PACKET_TYPE_KD_RESET: return "RESET";
	case PACKET_TYPE_KD_STATE_CHANGE: return "STATE_CHANGE";
	case PACKET_TYPE_KD_BREAKIN: return "BREAKIN";
	case PACKET_TYPE_KD_TRACE_IO: return "TRACE_IO";
	case PACKET_TYPE_KD_CONTROL_REQUEST: return "CONTROL_REQUEST";
	case PACKET_TYPE_KD_FILE_IO: return "FILE_IO";
	}
	return "???";
}

void CDbgPipe::Reset()
{
	m_NextPacketIdToSend = INITIAL_PACKET_ID;
	m_PacketIdExpected = INITIAL_PACKET_ID;
	m_cbNeed = sizeof(KD_PACKET);
	m_cbData = 0;
	m_WaitApiResponse = DbgKdNoWait;
	_bittestandset(&m_flags, ct_wait_header);
	if (_bittestandreset(&m_flags, ct_set_event))
	{
		ZwSetEvent(m_hEvent, 0);
	}
}

CDbgPipe::CDbgPipe()
{
	m_name = "";//$$$
	Reset();
	m_pDoc = 0;
	m_hEvent = CreateEvent(0, 0, 0, 0);
	m_bReadActive = false;
}

CDbgPipe::~CDbgPipe()
{
	DbgPrint("%s<%p>\n", __FUNCTION__, this);

	Reset();

	if (m_hEvent)
	{
		NtClose(m_hEvent);
	}
}

BOOL CDbgPipe::OnConnect(NTSTATUS status)
{
	DbgPrint("OnConnect<%p>(%X) %x\n", this, m_flags, status);
	if (0 > status)
	{
		return FALSE;
	}
	m_bReadActive = false;
	_bittestandreset(&m_flags, ct_remote_wait);
	_bittestandreset(&m_flags, ct_can_send);
	Reset();
	return TRUE;
}

void CDbgPipe::OnDisconnect()
{
	DbgPrint("OnDisconnect<%p>\n", this);
	//_bittestandset(&m_flags, ct_quit);
	OnRemoteEnd();
	Reset();
	Listen();
}

void CDbgPipe::OnRemoteEnd()
{
	if (m_pDoc)
	{
		m_pDoc->OnRemoteEnd();
		m_pDoc->Release();
		m_pDoc = 0;
	}
}

NTSTATUS CDbgPipe::SendControlPacket(PACKET_KD_TYPE Type, ULONG Id)
{
	if (CDataPacket* packet = new(sizeof (KD_PACKET)) CDataPacket)
	{
		KD_PACKET* pkp = (KD_PACKET*)packet->getData();
		packet->setDataSize(sizeof (KD_PACKET));
		pkp->ByteCount = 0;
		pkp->Checksum = 0;
		pkp->PacketId = Id;
		pkp->PacketLeader = CONTROL_PACKET_LEADER;
		pkp->PacketType = Type;
		NTSTATUS status = Write(packet);
		if (0 > status)
		{
			__debugbreak();
		}
		packet->Release();
		return status;
	}

	return STATUS_INSUFFICIENT_RESOURCES;
}

NTSTATUS CDbgPipe::SendBreakIn()
{
	static BYTE brk = BREAKIN_PACKET_BYTE;
	return Write(&brk, sizeof(brk));
}

NTSTATUS CDbgPipe::SendPacket(PACKET_KD_TYPE Type, PVOID lpData1, DWORD cbData1, PVOID lpData2, DWORD cbData2)
{
	if (!_bittest(&m_flags, ct_can_send))
	{
		//__debugbreak();
		return STATUS_INVALID_DEVICE_STATE;
	}

	_bittestandreset(&m_flags, ct_can_send);

	if (CDataPacket* packet = new(sizeof (KD_PACKET) + cbData1 + cbData2 + 1) CDataPacket)
	{
		packet->setDataSize(sizeof (KD_PACKET) + cbData1 + cbData2 + 1);

		KD_PACKET* pkp = (KD_PACKET*)packet->getData();
		pkp->ByteCount = (WORD)(cbData1 + cbData2);
		pkp->Checksum = KdpComputeChecksum((PBYTE)lpData1, cbData1) + KdpComputeChecksum((PBYTE)lpData2, cbData2);
		pkp->PacketId = m_NextPacketIdToSend;
		pkp->PacketLeader = PACKET_LEADER;
		pkp->PacketType = Type;
		PBYTE p = (PBYTE)(pkp + 1);
		memcpy(p, lpData1, cbData1);
		memcpy(p + cbData1, lpData2, cbData2);
		p[cbData1 + cbData2] = PACKET_TRAILING_BYTE;

		NTSTATUS status = Write(packet);

		DbgPrint("%s:------->%s(%x)\n", m_name, GetName(Type), m_NextPacketIdToSend);

		packet->Release();
		return status;
	}

	return STATUS_INSUFFICIENT_RESOURCES;
}

NTSTATUS CDbgPipe::KdContinue(NTSTATUS status)
{
	DBGKD_MANIPULATE_STATE m;
	m.ApiNumber = DbgKdContinueApi;
	m.Continue.ContinueStatus = status;
	_bittestandreset(&m_flags, ct_remote_wait);
	return SendPacket(PACKET_TYPE_KD_STATE_MANIPULATE, &m, sizeof m, 0, 0);
}

#ifdef _WIN64

C_ASSERT(sizeof(WOW64_FLOATING_SAVE_AREA)==0x70);
C_ASSERT(sizeof(XMM_SAVE_AREA32)==512);

void CopyWowContext(CONTEXT* ctx, WOW64_CONTEXT* wow);

void CopyWowContext(WOW64_CONTEXT* wow, CONTEXT* ctx)
{
	wow->ContextFlags = ctx->ContextFlags;

	wow->Eax = (ULONG)ctx->Rax;
	wow->Ebx = (ULONG)ctx->Rbx;
	wow->Ecx = (ULONG)ctx->Rcx;
	wow->Edx = (ULONG)ctx->Rdx;
	wow->Edi = (ULONG)ctx->Rdi;
	wow->Esi = (ULONG)ctx->Rsi;
	wow->Ebp = (ULONG)ctx->Rbp;
	wow->Esp = (ULONG)ctx->Rsp;
	wow->Eip = (ULONG)ctx->Rip;

	wow->EFlags = ctx->EFlags;

	wow->Dr0 = (ULONG)ctx->Dr0;
	wow->Dr1 = (ULONG)ctx->Dr1;
	wow->Dr2 = (ULONG)ctx->Dr2;
	wow->Dr3 = (ULONG)ctx->Dr3;
	wow->Dr6 = (ULONG)ctx->Dr6;
	wow->Dr7 = (ULONG)ctx->Dr7;

	wow->SegSs = ctx->SegSs;
	wow->SegCs = ctx->SegCs;
	wow->SegDs = ctx->SegDs;
	wow->SegEs = ctx->SegEs;
	wow->SegFs = ctx->SegFs;
	wow->SegGs = ctx->SegGs;

	memcpy(&wow->FloatSave, ctx->VectorRegister, sizeof(wow->FloatSave));
	memcpy(wow->ExtendedRegisters, &ctx->FltSave, sizeof(wow->ExtendedRegisters));
}
#endif

NTSTATUS CDbgPipe::SetContext(WORD Processor, PCONTEXT ctx)
{
	m_WaitApiResponse = DbgKdSetContextApi;
	DBGKD_MANIPULATE_STATE m;
	m.ApiNumber = DbgKdSetContextApi;
	m.Processor = Processor;
	m.SetContext.ContextFlags = 0;
#ifdef _WIN64
	PVOID pv = ctx;
	ULONG cb = sizeof(CONTEXT);
	if (m_pDoc->IsWow64Process())
	{
		WOW64_CONTEXT wow;
		CopyWowContext(&wow, ctx);
		pv = &wow, cb = sizeof(wow);
	}
	return WaitForResponse(SendPacket(PACKET_TYPE_KD_STATE_MANIPULATE, &m, sizeof m, pv, cb));
#else
	return STATUS_NOT_IMPLEMENTED;
#endif
}

NTSTATUS CDbgPipe::GetContext(WORD Processor, PCONTEXT ctx)
{
	m_WaitApiResponse = DbgKdGetContextApi;
	DBGKD_MANIPULATE_STATE m;
	m.ApiNumber = DbgKdGetContextApi;
	m.Processor = Processor;
	NTSTATUS status = WaitForResponse(SendPacket(PACKET_TYPE_KD_STATE_MANIPULATE, &m, sizeof(m), 0, 0));
	if (0 > status)
	{
		return status;
	}

	if (ByteCount > sizeof(DBGKD_MANIPULATE_STATE))
	{
		ByteCount -= sizeof(DBGKD_MANIPULATE_STATE);
	}
	else
	{
		ByteCount = 0;
	}

#ifdef _WIN64
	if (m_pDoc->IsWow64Process())
	{
		CopyWowContext(ctx, (WOW64_CONTEXT*)(&m_ms + 1));
	}
	else
#endif
	{
		ULONG cb = sizeof(CONTEXT);

		if (cb > ByteCount)
		{
			cb = ByteCount;
		}

		if (cb)
		{
			memcpy(ctx, &m_ms + 1, cb);
		}
	}

	return STATUS_SUCCESS;
}

NTSTATUS CDbgPipe::ReadReadControlSpace(WORD Processor, PCONTEXT ctx)
{
	m_WaitApiResponse = DbgKdReadControlSpaceApi;
	DBGKD_MANIPULATE_STATE m;
	m.ApiNumber = DbgKdReadControlSpaceApi;
	m.Processor = Processor;
#ifdef _WIN64
	BOOL IsWow64Process = m_pDoc->IsWow64Process();

	if (IsWow64Process)
	{
		m.ReadWriteMemory.TargetBaseAddress = sizeof(WOW64_CONTEXT);
		m.ReadWriteMemory.TransferCount = sizeof(KSPECIAL_REGISTERS_X86);
	}
	else
#endif
	{
		m.ReadWriteMemory.TargetBaseAddress = 2;
		m.ReadWriteMemory.TransferCount = sizeof(KSPECIAL_REGISTERS_X64);
	}

	NTSTATUS status = WaitForResponse(SendPacket(PACKET_TYPE_KD_STATE_MANIPULATE, &m, sizeof(m), 0, 0));
	
	if (0 > status)
	{
		return status;
	}

	if (ByteCount > sizeof(DBGKD_MANIPULATE_STATE))
	{
		ByteCount -= sizeof(DBGKD_MANIPULATE_STATE);
	}
	else
	{
		ByteCount = 0;
	}

	if (ByteCount > m.ReadWriteMemory.TransferCount)
	{
		ByteCount = (WORD)m.ReadWriteMemory.TransferCount;
	}

	memcpy(&m_k64, &m_ms+1, ByteCount);

#ifdef _WIN64
	if (!IsWow64Process)
	{
		ctx->Dr0 = m_k64.KernelDr0;
		ctx->Dr1 = m_k64.KernelDr1;
		ctx->Dr2 = m_k64.KernelDr2;
		ctx->Dr3 = m_k64.KernelDr3;
		ctx->Dr6 = m_k64.KernelDr6;
		ctx->Dr7 = m_k64.KernelDr7;
	}
	else
#endif
	{
		ctx->Dr0 = m_k86.KernelDr0;
		ctx->Dr1 = m_k86.KernelDr1;
		ctx->Dr2 = m_k86.KernelDr2;
		ctx->Dr3 = m_k86.KernelDr3;
		ctx->Dr6 = m_k86.KernelDr6;
		ctx->Dr7 = m_k86.KernelDr7;
	}

	return STATUS_SUCCESS;
}

NTSTATUS CDbgPipe::WriteReadControlSpace(WORD Processor, PCONTEXT ctx)
{
	m_WaitApiResponse = DbgKdWriteControlSpaceApi;
	DBGKD_MANIPULATE_STATE m;
	m.ApiNumber = DbgKdWriteControlSpaceApi;
	m.Processor = Processor;

#ifdef _WIN64
	BOOL IsWow64Process = m_pDoc->IsWow64Process();

	if (IsWow64Process)
	{
		m.ReadWriteMemory.TargetBaseAddress = sizeof(WOW64_CONTEXT);
		m.ReadWriteMemory.TransferCount = sizeof(KSPECIAL_REGISTERS_X86);

		m_k86.KernelDr0 = (ULONG)ctx->Dr0;
		m_k86.KernelDr1 = (ULONG)ctx->Dr1;
		m_k86.KernelDr2 = (ULONG)ctx->Dr2;
		m_k86.KernelDr3 = (ULONG)ctx->Dr3;
		m_k86.KernelDr6 = (ULONG)ctx->Dr6;
		m_k86.KernelDr7 = (ULONG)ctx->Dr7;
	}
	else
#endif
	{
		m.ReadWriteMemory.TargetBaseAddress = 2;
		m.ReadWriteMemory.TransferCount = sizeof(KSPECIAL_REGISTERS_X64);

		m_k64.KernelDr0 = ctx->Dr0;
		m_k64.KernelDr1 = ctx->Dr1;
		m_k64.KernelDr2 = ctx->Dr2;
		m_k64.KernelDr3 = ctx->Dr3;
		m_k64.KernelDr6 = ctx->Dr6;
		m_k64.KernelDr7 = ctx->Dr7;
	}

	return WaitForResponse(SendPacket(PACKET_TYPE_KD_STATE_MANIPULATE, &m, sizeof(m), &m_k64, m.ReadWriteMemory.TransferCount));
}

NTSTATUS CDbgPipe::GetVersion()
{
	m_WaitApiResponse = DbgKdGetVersionApi;
	DBGKD_MANIPULATE_STATE m;
	m.ApiNumber = DbgKdGetVersionApi;
	return WaitForResponse(SendPacket(PACKET_TYPE_KD_STATE_MANIPULATE, &m, sizeof m, 0, 0));
}

NTSTATUS CDbgPipe::WaitForResponse(NTSTATUS status)
{
	if (0 > status)
	{
		m_WaitApiResponse = DbgKdNoWait;
		return status;
	}

	Read();

	do ; while (STATUS_USER_APC == WaitForSingleObjectEx(m_hEvent, INFINITE, TRUE));

	return m_ms.ReturnStatus;
}

NTSTATUS CDbgPipe::ReadMemory(PVOID Address, DWORD cb)
{
	m_WaitApiResponse = DbgKdReadVirtualMemoryApi;
	DBGKD_MANIPULATE_STATE m;
	m.ApiNumber = DbgKdReadVirtualMemoryApi;

	ULONGLONG ulAddress;
#ifdef _WIN64
	ulAddress = m_pDoc->IsWow64Process() ? EXTEND64(Address) : (ULONGLONG)(ULONG_PTR)Address;
#else
	ulAddress = EXTEND64(Address);
#endif
	
	m.ReadWriteMemory.TargetBaseAddress = ulAddress;
	m.ReadWriteMemory.TransferCount = cb;

	DbgPrint("ReadMemory(%I64x, %x)...\n", ulAddress, cb);

	NTSTATUS status = WaitForResponse(SendPacket(PACKET_TYPE_KD_STATE_MANIPULATE, &m, sizeof m, 0, 0));

	DbgPrint("ReadMemory=%x\n", status);

	if (0 > status)
	{
		return status;
	}

	if (m_ms.ReadWriteMemory.TargetBaseAddress != ulAddress)
	{
		return STATUS_UNSUCCESSFUL;
	}

	return status;
}

NTSTATUS CDbgPipe::ReadRemote(PBYTE RemoteAddress, PBYTE buf, DWORD cb, PSIZE_T pcb)
{
	SIZE_T rcb;

	if (!pcb)
	{
		pcb = &rcb;
	}

	*pcb = 0;

__loop:
	NTSTATUS status = ReadMemory(RemoteAddress, cb);

	if (0 > status)
	{
		return status;
	}

	if (ULONG ActualTransferCount = m_ms.ReadWriteMemory.ActualTransferCount)
	{
		memcpy(buf, &m_ms + 1, ActualTransferCount);
		RemoteAddress += ActualTransferCount, buf += ActualTransferCount;
		*pcb += ActualTransferCount;

		if (cb -= ActualTransferCount)
		{
			goto __loop;
		}
	}
	else
	{
		return cb ? *pcb ? STATUS_PARTIAL_COPY : STATUS_ACCESS_VIOLATION : STATUS_SUCCESS;
	}

	return STATUS_SUCCESS;
}

NTSTATUS CDbgPipe::WriteMemory(PVOID Address, PVOID Buffer, DWORD cb)
{
	m_WaitApiResponse = DbgKdWriteVirtualMemoryApi;
	DBGKD_MANIPULATE_STATE m;
	m.ApiNumber = DbgKdWriteVirtualMemoryApi;
#ifdef _WIN64
	m.ReadWriteMemory.TargetBaseAddress = m_pDoc->IsWow64Process() ? EXTEND64(Address) : (ULONGLONG)(ULONG_PTR)Address;
#else
	m.ReadWriteMemory.TargetBaseAddress = EXTEND64(Address);
#endif
	m.ReadWriteMemory.TransferCount = cb;
	return WaitForResponse(SendPacket(PACKET_TYPE_KD_STATE_MANIPULATE, &m, sizeof m, Buffer, cb));
}

BOOL CDbgPipe::OnRead(PVOID Buffer, ULONG cbData)
{
	if (cbData > 1) DbgPrint("CDbgPipe::OnRead(%p, %x [%02x]) <%08x,%08x>\n", Buffer, cbData, *(PUCHAR)Buffer, m_cbData, m_cbNeed);

	if (!cbData)
	{
		__debugbreak();
		return FALSE;
	}

	if (!m_bReadActive)
	{
		__debugbreak();
	}
	m_bReadActive = false;

	//DbgPrint("m_bReadActive = false\n");

	//////////////////////////////////////////////////////////////////////////
	//
	if (cbData == 1 && BREAKIN_PACKET_BYTE == *(PBYTE)Buffer && _bittest(&m_flags, ct_wait_header) && !m_cbData)
	{
		//m_PacketIdExpected ^= 1;
		m_cbNeed = sizeof(KD_PACKET), m_cbData = 0;
		DbgPrint("%s: >>> BREAKIN_PACKET_BYTE(%x) <<<\n", m_name, m_PacketIdExpected);
		return TRUE;
	}
	//
	//////////////////////////////////////////////////////////////////////////

	if (cbData > m_cbNeed)
	{
		__debugbreak();
		return FALSE;
	}

	if (m_cbData += cbData, m_cbNeed -= cbData) return TRUE;

	if (_bittestandcomplement(&m_flags, ct_wait_header))
	{
		if (sizeof(m_buffer) <= ByteCount) 
		{
			__debugbreak();
			return FALSE;
		}

		switch (PacketLeader)
		{
		case PACKET_LEADER:

			DbgPrint("%s: KD_PACKET(%s(%X), %X, cb=%X)\n", m_name, GetName(PacketType), PacketType, PacketId, ByteCount);

			if (PacketId & SYNC_PACKET_ID)
			{
				PacketId &= ~SYNC_PACKET_ID;
				m_PacketIdExpected = PacketId;
				m_NextPacketIdToSend = INITIAL_PACKET_ID;
				DbgPrint("^^^^^^^^^^^^^ SYNC_PACKET_ID ^^^^^^^^^^^^^^^^^^\n");
			}

			m_cbNeed = ByteCount + 1;
			break;

		case CONTROL_PACKET_LEADER:

			DbgPrint("\t\t\t\t\t\t\t* %s:KD_PACKET(%s(%X), %X)\n", m_name, GetName(PacketType), PacketType, PacketId);

			if (ByteCount || Checksum)
			{
				__debugbreak();
				return FALSE;
			}

			m_cbNeed = sizeof(KD_PACKET), m_cbData = 0;

			_bittestandset(&m_flags, ct_wait_header);

			switch(PacketType)
			{
			case PACKET_TYPE_KD_ACKNOWLEDGE:
				if (PacketId != m_NextPacketIdToSend)
				{
					__debugbreak();
				}

				m_NextPacketIdToSend ^= 1;
				if (_bittest(&m_flags, ct_remote_wait)) _bittestandset(&m_flags, ct_can_send);
				break;

			case PACKET_TYPE_KD_RESET:
				//__debugbreak();
				DbgPrint("%s: RESET\n", m_name);
				Reset();
				break;
			case PACKET_TYPE_KD_RESEND:
				DbgPrint("%s: RESEND\n", m_name);
				//__debugbreak();
				break;
			default:
				__debugbreak();
				return FALSE;
			}
			break;
		default:
			__debugbreak();
			return FALSE;
		}

		return TRUE;
	}

	m_cbNeed = sizeof(KD_PACKET), m_cbData = 0;

	if (PacketId != m_PacketIdExpected)
	{
		__debugbreak();
		return FALSE;
	}

	m_PacketIdExpected ^= 1;

	if (m_buffer[ByteCount] != PACKET_TRAILING_BYTE || KdpComputeChecksum(m_buffer, ByteCount) != Checksum)
	{
		__debugbreak();
		return FALSE;
	}

	if (0 > SendControlPacket(PACKET_TYPE_KD_ACKNOWLEDGE, PacketId & ~SYNC_PACKET_ID))
	{
		__debugbreak();
		return FALSE;
	}

	_bittestandset(&m_flags, ct_can_send);

	DbgPrint("-------> ACKNOWLEDGE(%x)\n", PacketId & ~SYNC_PACKET_ID);

	if (m_WaitApiResponse != DbgKdNoWait)
	{
		if (PacketType != PACKET_TYPE_KD_STATE_MANIPULATE || m_WaitApiResponse != m_ms.ApiNumber)
		{
			//__debugbreak();
			m_ms.ReturnStatus = STATUS_LPC_REPLY_LOST;
		}

		m_WaitApiResponse = DbgKdNoWait;

		return SetEvent(m_hEvent) != 0;
	}

	KD_PACKET_EX* packet = (KD_PACKET_EX*)alloca(ByteCount + sizeof(KD_PACKET));
	memcpy(packet, static_cast<KD_PACKET_EX*>(this), ByteCount + sizeof(KD_PACKET));

	return OnPacket(packet);
}

BOOL CDbgPipe::OnPacket(KD_PACKET_EX* packet)
{
	if (!m_pDoc)
	{
		if (PacketType != PACKET_TYPE_KD_STATE_CHANGE)
		{
			return FALSE;
		}

		if (0 > GetVersion())
		{
			return FALSE;
		}

		BOOL fOk = FALSE;

		if (m_pDoc = new ZDbgDoc(TRUE))
		{
			fOk = m_pDoc->OnRemoteStart(this, &m_ms.GetVersion);
		}

		if (!fOk)
		{
			return FALSE;
		}
	}

	NTSTATUS status = DBG_CONTINUE;

	switch(packet->PacketType)
	{

	case PACKET_TYPE_KD_STATE_CHANGE:

		_bittestandset(&m_flags, ct_remote_wait);

		//DbgPrint("%s: WAIT_STATE_CHANGE(%X) pc = %I64X thread = %I64X\n", m_name, 
		//	packet->m_ws.ApiNumber, packet->m_ws.ProgramCounter, packet->m_ws.Thread);

		switch(packet->m_ws.ApiNumber)
		{
		case DbgKdLoadSymbolsStateChange:
			if (packet->m_ws.LoadSymbols.UnloadSymbols && packet->m_ws.LoadSymbols.BaseOfDll == ~0)
			{
				DbgPrint("%s: System Shutdown\n", m_name);
				m_pDoc->cprintf(prGen, L"System Shutdown\n");
				break;
			}

			//DbgPrint("%s: %s(%x) %p [%x %x %s]\n", m_name,
			//	packet->m_ws.LoadSymbols.UnloadSymbols ? "Unload" : "Load",
			//	packet->m_ws.LoadSymbols.ProcessId,
			//	(PVOID)(ULONG_PTR)packet->m_ws.LoadSymbols.BaseOfDll, 
			//	packet->m_ws.LoadSymbols.SizeOfImage,
			//	packet->m_ws.LoadSymbols.CheckSum, 
			//	packet->m_ws.Name);

			goto __cc;

		case DbgKdExceptionStateChange:

			DbgPrint("%s: [Exception]%X at %I64X\n", m_name, packet->m_ws.Exception.ExceptionRecord.ExceptionCode,packet->m_ws.Exception.ExceptionRecord.ExceptionAddress);
__cc:
			status = m_pDoc->OnWaitStateChange(&packet->m_ws);

			if (!status)
			{
				return TRUE;
			}
			break;
		default:
			;DbgPrint("%s(%u): !!!!!!!!!!!!!\n", __FILE__, __LINE__);
		}

		KdContinue(status);
		return TRUE;

	case PACKET_TYPE_KD_FILE_IO:

		switch (packet->m_fi.ApiNumber)
		{
		case DbgKdCreateFileApi:
			DbgPrint("%s: DbgKdCreateFileApi:%S\n", m_name, packet->m_fi.CreateFile.Name);
			break;
		case DbgKdReadFileApi:
			DbgPrint("%s: DbgKdReadFileApi:%p %I64x %x\n", m_name, packet->m_fi.ReadFile.Handle, packet->m_fi.ReadFile.Offset, packet->m_fi.ReadFile.Length);
			break;
		case DbgKdWriteFileApi:
			DbgPrint("%s: DbgKdWriteFileApi:%p %I64x %x\n", m_name, packet->m_fi.WriteFile.Handle, packet->m_fi.ReadFile.Offset, packet->m_fi.ReadFile.Length);
			break;
		case DbgKdCloseFileApi:
			DbgPrint("%s: DbgKdCloseFileApi:%p\n", m_name, packet->m_fi.CloseFile.Handle);
			break;
		default:
			;DbgPrint("%s: FILE_IO(%x)\n", m_name, packet->m_fi.ApiNumber);
		}

		//DumpBuffer(Buffer, cb);
		packet->m_hd.Status = STATUS_NO_SUCH_FILE;
		SendPacket(PACKET_TYPE_KD_FILE_IO, &packet->m_hd, packet->ByteCount, 0, 0);
		return TRUE;

	case PACKET_TYPE_KD_DEBUG_IO:

		DbgPrint("%s: DEBUG_IO:(%X)%.*s\n", m_name, packet->m_io.ApiNumber, packet->m_io.LengthOfPromptString, packet->m_io.String);

		switch(packet->m_io.ApiNumber)
		{
		case DbgKdPrintStringApi:
			//SendBreakIn();
			return TRUE;
		case DbgKdGetStringApi:
			SendPacket(PACKET_TYPE_KD_DEBUG_IO, 0, 0, 0, 0);
			return TRUE;
		default:
			__debugbreak();
			return TRUE;
		}
	case PACKET_TYPE_KD_STATE_MANIPULATE:
		__debugbreak();
		return FALSE;
	}
	DbgPrint("%s: OnPacket(%s(%x)::%u)\n", m_name, GetName(packet->PacketType), packet->PacketType, packet->m_hd.ApiNumber);
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
//
#include "common.h"

void StopDebugPipe(CDbgPipe* p)
{
	p->Close();
	p->Release();
	ZwTestAlert();
}

BOOL StartDebugPipe(CDbgPipe** pp)
{
	if (CDbgPipe* p = new CDbgPipe)
	{
		STATIC_OBJECT_ATTRIBUTES(oa, "\\device\\namedpipe\\VmDbgPipe");//VmDbgPipe

		if (/*0 <= p->SetBuffer(256) &&*/ 0 <= p->Create(&oa, 0, 1))
		{
			*pp = p;

			return TRUE;
		}

		p->Release();
	}

	return FALSE;
}

_NT_END