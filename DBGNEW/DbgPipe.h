#pragma once

#include "kdbg.h"
#include "../asio/pipe.h"

class ZDbgDoc;

class CDbgPipe : public CPipeEnd, KD_PACKET_EX
{
protected:

	PCSTR m_name;

	HANDLE m_hEvent;
	ZDbgDoc* m_pDoc;
	DBGK_API m_WaitApiResponse;

	DWORD m_NextPacketIdToSend;
	DWORD m_PacketIdExpected;
	DWORD m_cbNeed;
	DWORD m_cbData;

	union {
		KSPECIAL_REGISTERS_X86 m_k86;
		KSPECIAL_REGISTERS_X64 m_k64;
	};

	bool m_bReadActive;

	enum {
		ct_wait_header = flMaxFlag,
		ct_can_send,
		ct_remote_wait,
		ct_quit,
		ct_data_flag,
		ct_set_event
	};

	void Reset();

	virtual ~CDbgPipe();

	virtual BOOL IsServer()
	{
		return TRUE;
	}

	virtual BOOL UseApcCompletion()
	{
		return TRUE;
	}

	virtual BOOL OnConnect(NTSTATUS status);

	virtual void OnDisconnect();

	void OnRemoteEnd();

	ULONG GetReadBuffer(void** ppv)
	{
		if (m_bReadActive)
		{
			return 0;
		}
		m_bReadActive = true;

		*ppv = (PBYTE)static_cast<KD_PACKET_EX*>(this) + m_cbData;

		//DbgPrint("GetReadBuffer(%p)\n", *ppv);
		return sizeof(KD_PACKET_EX) - m_cbData;
	}
	BOOL OnPacket(KD_PACKET_EX* packet);

public:

	CDbgPipe();

	BOOL CanSend()
	{
		return _bittest(&m_flags, ct_can_send);
	}

	NTSTATUS SendControlPacket(PACKET_KD_TYPE Type, ULONG Id);

	NTSTATUS SendBreakIn();

	NTSTATUS SendPacket(PACKET_KD_TYPE Type, PVOID lpData1, DWORD cbData1, PVOID lpData2, DWORD cbData2);

	NTSTATUS KdContinue(NTSTATUS status);

	NTSTATUS GetContext(WORD Processor, PCONTEXT ctx);

	NTSTATUS ReadReadControlSpace(WORD Processor, PCONTEXT ctx);
	NTSTATUS WriteReadControlSpace(WORD Processor, PCONTEXT ctx);

	NTSTATUS SetContext(WORD Processor, PCONTEXT ctx);

	NTSTATUS GetVersion();

	NTSTATUS WaitForResponse(NTSTATUS status);

	NTSTATUS ReadMemory(PVOID Address, DWORD cb);

	NTSTATUS ReadRemote(PBYTE RemoteAddress, PBYTE buf, DWORD cb, PSIZE_T pcb = 0);

	NTSTATUS WriteMemory(PVOID Address, PVOID Buffer, DWORD cb);

	virtual BOOL OnRead(PVOID Buffer, ULONG cbData);
};