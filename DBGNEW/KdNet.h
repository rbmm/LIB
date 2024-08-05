#pragma once

#include "kdbg.h"

#pragma pack(push, 1)
struct Mdbg
{
	enum { Invalid, MDBG = 'GBDM' } Sig;
};
#pragma pack(pop)

C_ASSERT(__alignof(Mdbg)==1);

struct KdNetPacket 
{
	WORD Pad;
	Mdbg Sig;
	UCHAR ProtocolVersion; // [2,5]
	UCHAR Flags; // 1,2

	enum { fControlChannel = 1 };

	union {
		ULONG64 u; // BigEndian
		struct {
			ULONG64 UnusedBytes : 7;
			ULONG64 Response : 1;
			ULONG64 SequenseNumber : 32;
			ULONG64 z : 24;
		};
		UCHAR EncBegin;
	};

	UCHAR buf[0x10];

	inline BOOL IsSigValid()
	{
		return Mdbg::MDBG == Sig.Sig;
	}

	inline void SetSig()
	{
		Sig.Sig = Mdbg::MDBG;
		Pad = 0;//$$$
	}
};

C_ASSERT(sizeof(KdNetPacket) == 0x20);

struct ControlChannelPacket {
	UCHAR u1;
	UCHAR u2;
	UCHAR key1[0x20];

	union {
		UCHAR key2[0x20];

		struct {
			IN6_ADDR ClientIp;
			WORD ClientPort;
			IN6_ADDR ServerIp;
			WORD ServerPort;
			IN6_ADDR Ip;
		};
	};
}; // IN -> 158, OUT <- 142

class ZDbgDoc;

union KdKey 
{
	ULONG64 u[4];
	UCHAR buf[sizeof(u)];
};

C_ASSERT(sizeof(KdKey) == 32);

class KdNetDbg : OVERLAPPED
{
	SOCKET _M_s = 0;
	BCRYPT_KEY_HANDLE _M_hKey = 0;
	BCRYPT_HASH_HANDLE _M_hHash = 0;
	BCRYPT_KEY_HANDLE _M_hKey2 = 0;
	BCRYPT_HASH_HANDLE _M_hHash2 = 0;

	ZDbgDoc* m_pDoc = 0;

	PUCHAR _M_pbHashObject = 0;
	ULONG _M_cbHashObject = 0;
	ULONG _M_SequenseNumber = 0;
	ULONG _M_RecvCount = 0;

	union {
		CHAR _M_buf[sizeof(KdNetPacket) + sizeof(KD_PACKET_EX)];
		KdNetPacket _M_packet;
	};

	KSPECIAL_REGISTERS_X64 _M_cs;

	union {
		CHAR _M_buf_ws[PACKET_BYTE_COUNT];
		DBGKD_WAIT_STATE_CHANGE _M_ws;
	};

	union {
		CHAR _M_buf_ms[PACKET_BYTE_COUNT];
		DBGKD_MANIPULATE_STATE _M_ms;
	};

	ULONG _M_wsByteCount;
	ULONG _M_msByteCount;

	SOCKADDR_INET _M_sa{};
	INT _M_len = 0;

	DBGK_API m_WaitApiResponse = DbgKdNoWait;
	DWORD m_NextPacketIdToSend = 0x80000000;
	DWORD m_PacketIdSent = 0;
	DWORD m_LastPacketId = 0;
	ULONG _M_BytesRecv = 0;
	ULONG _M_BytesSend = 0;
	LONG _M_dwRef = 1;
	LONG m_flags = 0;

	UCHAR _M_ProtocolVersion = 0;

	enum {
		ct_recv_active,
		ct_init,
		ct_data_init,
		ct_data_received,
		ct_event,
		ct_can_send,
		ct_quit,
		ct_shutdown,
		ct_terminate,
		ct_wsa_init
	};

	void StartRecv();
	void EndRecv();

	BOOL InitSession();

	static void KdRecv(SOCKET s, OVERLAPPED* ov, PVOID buf, ULONG cb, sockaddr* lpFrom, LPINT lpFromlen);

	static void CALLBACK OnKdRecv(
		_In_ DWORD dwError,
		_In_ DWORD cbTransferred,
		_In_ OVERLAPPED* lpOverlapped,
		_In_ DWORD /*dwFlags*/
		);

	void Recv();

	BOOL OnRecv(_In_ KdNetPacket* packet, _In_ DWORD cbTransferred);
	void OnRecvError(_In_ DWORD dwError);

	BOOL OnPacket(ULONG Flags, ULONG SequenseNumber, PVOID packet, ULONG cb);
	BOOL OnPacket(ULONG SequenseNumber, KD_PACKET_EX* packet, ULONG cb);
	BOOL OnControlChannelPacket(ULONG SequenseNumber, ControlChannelPacket* packet, ULONG cb);

	BOOL OnDataPacket(KD_PACKET_EX* packet, ULONG cb);
	BOOL OnControlPacket(KD_PACKET_EX* packet, ULONG cb);

	void Terminate();

	~KdNetDbg();

public:

	KdNetDbg()
	{
		RtlZeroMemory(static_cast<OVERLAPPED*>(this), sizeof(OVERLAPPED));
		Pointer = this;
	}

	void AddRef()
	{
		InterlockedIncrementNoFence(&_M_dwRef);
	}

	void Release()
	{
		if (!InterlockedDecrement(&_M_dwRef))
		{
			delete this;
		}
	}

	BOOL CanSend()
	{
		return _bittest(&m_flags, ct_can_send);
	}

	HRESULT SendPacket(const void* pvData, ULONG cbData, UCHAR Flags = 0);
	
	HRESULT SendControlPacket(PACKET_KD_TYPE Type, ULONG Id);

	HRESULT SendBreakIn();

	HRESULT SendPacket(PACKET_KD_TYPE Type, KD_PACKET* pkp, ULONG ByteCount);

	HRESULT KdContinue(NTSTATUS status);

	HRESULT GetContext(WORD Processor, PCONTEXT ctx);

	HRESULT ReadControlSpace(WORD Processor, PCONTEXT ctx);

	HRESULT WriteControlSpace(WORD Processor, PCONTEXT ctx);

	HRESULT SetContext(WORD Processor, PCONTEXT ctx);

	HRESULT GetVersion();

	HRESULT SendAndWait(DBGK_API ApiNumber, KD_PACKET_EX* pkt, ULONG ByteCount);

	HRESULT ReadMemory(_In_ ULONGLONG ulAddress, _In_ DWORD cb, _Out_ PVOID buf, _Out_ PULONG pcb);

	HRESULT ReadRemote(PBYTE RemoteAddress, PBYTE buf, DWORD cb, PSIZE_T pcb = 0);

	HRESULT WriteMemory(PVOID Address, PVOID Buffer, DWORD cb);

	HRESULT SetBreakPoint(_In_ PVOID BreakPointAddress, _Out_ PULONG BreakPointHandle);

	HRESULT DeleteBreakpoint(_In_ ULONG BreakPointHandle);

	//////////////////////////////////////////////////////////////////////////

	HRESULT Init(_In_ KdKey* key, _In_ int af, _In_ WORD port, _In_ PCWSTR pcszKey);

	NTSTATUS FormatFromAddress(PWSTR buf, ULONG cch);

	void GetNetStat(ULONG& BytesRecv, ULONG& BytesSend)
	{
		BytesRecv = _M_BytesRecv;
		BytesSend = _M_BytesSend;
	}
};

HRESULT StartKdNet(HWND hwnd);
