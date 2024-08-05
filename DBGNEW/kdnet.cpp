#include "stdafx.h"

_NT_BEGIN

#include "kdnet.h"
#include "DbgDoc.h"

void PrintUTF8_v(PCSTR format, ...);
//  //1
#define __DBG__ 0

#if __DBG__
#undef DbgPrint
#define DbgPrint PrintUTF8_v
#endif

PCSTR get(PACKET_KD_TYPE PacketType, PSTR buf, ULONG cch)
{
	switch (PacketType)
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

	sprintf_s(buf, cch, "[%x]", PacketType);
	return buf;
}

void KdGetString(HWND hwnd, DBGKD_DEBUG_IO* pdio);

ULONG KdpComputeChecksum (PUCHAR Buffer, ULONG Length)
{
	ULONG Checksum = 0;
	if (Length)
	{
		do Checksum += *Buffer++; while (--Length);
	}
	return Checksum;
}

NTSTATUS KeyToBin(_In_ PCWSTR pcszKey, _Out_ PULONG64 pu, _In_ ULONG n)
{
	do 
	{
		*pu++ = _wcstoui64(pcszKey, const_cast<WCHAR**>(&pcszKey), 36);
		if ('.' != *pcszKey++)
		{
			return STATUS_BAD_DATA;
		}
	} while (--n);

	return *pcszKey ? STATUS_BAD_DATA : STATUS_SUCCESS;
}

void InvertBin(_Inout_ PULONG64 pu, _In_ ULONG n)
{
	do 
	{
		*pu = ~*pu;
	} while (pu++, --n);
}

NTSTATUS KeyPass(_In_ KdKey* key,
				 _Out_ BCRYPT_KEY_HANDLE* phKey, 
				 _Out_ BCRYPT_HASH_HANDLE *phHash,
				 _Out_ BCRYPT_HASH_HANDLE *phHash2,
				 _Out_ BYTE** ppbHashObject,
				 _Out_ ULONG* pcbHashObject)
{
	NTSTATUS status;
	BCRYPT_ALG_HANDLE hAlgorithm;
	BCRYPT_KEY_HANDLE hKey;

	BCRYPT_HASH_HANDLE hHash;
	if (0 <= (status = BCryptOpenAlgorithmProvider(&hAlgorithm, BCRYPT_SHA256_ALGORITHM, 0, 0)))
	{
		status = BCryptCreateHash(hAlgorithm, &hHash, 0, 0, 0, 0, 0);
		BCryptCloseAlgorithmProvider(hAlgorithm, 0);

		if (0 <= status)
		{
			if (0 <= (status = BCryptHashData(hHash, key->buf, sizeof(key->buf), 0)))
			{
				if (0 <= (status = BCryptOpenAlgorithmProvider(&hAlgorithm, BCRYPT_AES_ALGORITHM, 0, 0)))
				{
					status = BCryptGenerateSymmetricKey(hAlgorithm, &hKey, 0, 0, key->buf, sizeof(key->buf), 0);
					BCryptCloseAlgorithmProvider(hAlgorithm, 0);

					if (0 <= status)
					{
						if (0 <= (status = BCryptOpenAlgorithmProvider(&hAlgorithm, BCRYPT_SHA256_ALGORITHM, 0, BCRYPT_ALG_HANDLE_HMAC_FLAG)))
						{
							ULONG cbHashObject, cb;
							if (0 <= (status = BCryptGetProperty(hAlgorithm, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(cbHashObject), &cb, 0)))
							{
								cb = cbHashObject;
								cbHashObject = (cbHashObject + 0xF) & ~0xF;

								status = STATUS_NO_MEMORY;

								if (PUCHAR pbHashObject = new UCHAR[cbHashObject * 2])
								{
									InvertBin(key->u, _countof(key->u));

									status = BCryptCreateHash(hAlgorithm, phHash, pbHashObject, cbHashObject, key->buf, sizeof(key->buf), 0);

									BCryptCloseAlgorithmProvider(hAlgorithm, 0);

									if (0 <= status)
									{
										*ppbHashObject = pbHashObject;
										*pcbHashObject = cbHashObject;
										*phKey = hKey;
										*phHash2 = hHash;
										return status;
									}

									delete [] pbHashObject;
								}
							}
						}

						BCryptDestroyKey(hKey);
					}
				}
			}
			BCryptDestroyHash(hHash);
		}
	}

	return status;
}

void CALLBACK OnKdRecv(
					   _In_ DWORD dwError,
					   _In_ DWORD cbTransferred,
					   _In_ OVERLAPPED* lpOverlapped,
					   _In_ DWORD /*dwFlags*/
					   );

void CALLBACK OnKdSend(
					   _In_ DWORD /*dwError*/,
					   _In_ DWORD /*cbTransferred*/,
					   _In_ OVERLAPPED* lpOverlapped,
					   _In_ DWORD /*dwFlags*/
					   )
{
	if (lpOverlapped)
	{
		if (PVOID Pointer = lpOverlapped->Pointer)
		{
			delete [] Pointer;
		}

		delete lpOverlapped;
	}
}

ULONG CreateKdSocket(_Out_ SOCKET *ps, int af, WORD port)
{
	SOCKADDR_INET sa {};
	int len = 0;

	switch (af)
	{
	case AF_INET:
		sa.Ipv4.sin_family = AF_INET;
		sa.Ipv4.sin_port = port;
		len = sizeof(sa.Ipv4);
		break;
	case AF_INET6:
		sa.Ipv6.sin6_family = AF_INET6;
		sa.Ipv6.sin6_port = port;
		len = sizeof(sa.Ipv6);
		break;
	}

	if (!len)
	{
		return ERROR_INVALID_PARAMETER;
	}

	SOCKET s = WSASocketW(af, SOCK_DGRAM, IPPROTO_UDP, 0, 0, WSA_FLAG_OVERLAPPED|WSA_FLAG_NO_HANDLE_INHERIT);

	if (INVALID_SOCKET != s)
	{
		if (!bind(s, (sockaddr*)&sa, len))
		{
			*ps = s;
			return NOERROR;
		}

		ULONG dwError = GetLastError();
		
		closesocket(s);

		return dwError;
	}

	return GetLastError();
}

void CheckIo(_In_ DWORD dwError,
			 _In_ LPWSAOVERLAPPED lpOverlapped,
			 _In_ LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	switch (dwError)
	{
	case NOERROR:
	case WSA_IO_PENDING:
		break;
	default:
		lpCompletionRoutine(dwError, 0, lpOverlapped, 0);
	}
}

void KdSend(SOCKET s, KdNetPacket* packet, ULONG cb, const sockaddr * lpTo, ULONG iTolen)
{
	ULONG dwError = ERROR_OUTOFMEMORY;

	OVERLAPPED* ov;

	if (ov = new OVERLAPPED)
	{
		RtlZeroMemory(ov, sizeof(OVERLAPPED));
		ov->Pointer = packet;
		WSABUF wb = { cb - FIELD_OFFSET(KdNetPacket, Sig), (PSTR)&packet->Sig };
		dwError = WSASendTo(s, &wb, 1, 0, 0, lpTo, iTolen, ov, OnKdSend) ? GetLastError() : NOERROR;
	}

	CheckIo(dwError, ov, OnKdSend);
}

void KdNetDbg::KdRecv(SOCKET s, OVERLAPPED* ov, PVOID buf, ULONG cb, sockaddr* lpFrom, LPINT lpFromlen)
{
	WSABUF wb = { cb, (PSTR)buf };
	DWORD Flags = 0;
	ULONG dwError = WSARecvFrom(s, &wb, 1, 0, &Flags, lpFrom, lpFromlen, ov, OnKdRecv) ? GetLastError() : NOERROR;
	CheckIo(dwError, ov, OnKdRecv);
}

void KdNetDbg::Recv()
{
	if (_M_s)
	{
		//DbgPrint("++Recv(0x%p, 0x%p)\r\n", ov, packet);
		if (!_bittestandset(&m_flags, ct_recv_active))
		{
			StartRecv();
			_M_len = sizeof(_M_sa);
			KdRecv(_M_s, this, &_M_packet.Sig, sizeof(_M_buf) - FIELD_OFFSET(KdNetPacket, Sig), (sockaddr*)&_M_sa, &_M_len);
		}
	}
}

BOOL KdNetDbg::OnPacket(ULONG Flags, ULONG SequenseNumber, PVOID packet, ULONG cb)
{
	return Flags & KdNetPacket::fControlChannel
		? OnControlChannelPacket(SequenseNumber, (ControlChannelPacket*)packet, cb)
		: OnPacket(SequenseNumber, (KD_PACKET_EX*)packet, cb);
}

BOOL KdNetDbg::OnRecv(_In_ KdNetPacket* packet, _In_ DWORD cbTransferred)
{
	if (!_bittestandreset(&m_flags, ct_recv_active))
	{
		__debugbreak();
	}

	_M_BytesRecv += cbTransferred;

	if (cbTransferred < sizeof(KdNetPacket) || !packet->IsSigValid())
	{
		DbgPrint("!! KDNET::MDBG\r\n");
		m_pDoc->printf(prGen, L"!! MDBG\r\n");
		return FALSE;
	}

	BCRYPT_KEY_HANDLE hKey = packet->Flags & KdNetPacket::fControlChannel ? _M_hKey : _M_hKey2;

	if (!hKey)
	{
		DbgPrint("!! KDNET::NoDataChannel\r\n");
		m_pDoc->printf(prGen, L"!! NoDataChannel\r\n");
		return FALSE;
	}

	ULONG cb = cbTransferred - sizeof(KdNetPacket) + sizeof(ULONG64);

	if (cb & 0xF)
	{
		DbgPrint("!! Invalid Data len = %x\r\n", cb);
		m_pDoc->printf(prGen, L"!! Invalid Data len = %x\r\n", cb);
		return FALSE;
	}

	UCHAR iv[0x10], hash[0x20];
	PBYTE pb = &packet->EncBegin;
	memcpy(iv, pb + cb, 0x10);

	NTSTATUS status;
	BCRYPT_HASH_HANDLE hHash;
	if (0 <= BCryptDecrypt(hKey, pb, cb, 0, iv, sizeof(iv), pb, cb, &cb, 0))
	{
		if (0 <= (status = BCryptDuplicateHash(_M_hHash, &hHash, _M_pbHashObject + _M_cbHashObject, _M_cbHashObject, 0)))
		{
			0 <= (status = BCryptHashData(hHash, (PBYTE)&packet->Sig, 6 + cb, 0)) &&
				0 <= (status = BCryptFinishHash(hHash, hash, sizeof(hash), 0));

			BCryptDestroyHash(hHash);

			if (0 <= status)
			{
				if (memcmp(hash, pb + cb, 0x10))
				{
					m_pDoc->printf(prGen, L"!! Invalid Hash\r\n");
					return FALSE;
				}

				packet->u = _byteswap_uint64(packet->u);

				ULONG UnusedBytes = packet->UnusedBytes;

				if (packet->Response)
				{
					m_pDoc->printf(prGen, L"!! Invalid Direction\r\n");
					return FALSE;
				}

				if (UnusedBytes > 0xF)
				{
					m_pDoc->printf(prGen, L"!! Invalid UnusedBytes(%x)\r\n", UnusedBytes);
					return FALSE;
				}

				if ((cb -= sizeof(ULONG64)) < UnusedBytes)
				{
					m_pDoc->printf(prGen, L"!! Invalid Data Length (%x < %x)\r\n", cb, UnusedBytes);
					return FALSE;
				}

				if (!_M_ProtocolVersion)
				{
					_M_ProtocolVersion = packet->ProtocolVersion;
					DbgPrint("ProtocolVersion=%x\r\n", _M_ProtocolVersion);
					m_pDoc->printf(prGen, L"ProtocolVersion=%x\r\n", _M_ProtocolVersion);
				}

				return OnPacket(packet->Flags, packet->SequenseNumber, packet->buf, cb - UnusedBytes);
			}
		}
	}

	DbgPrint("!! KDNET::FailDecryptPacket(#x %x)\r\n", packet->SequenseNumber, cbTransferred);
	m_pDoc->printf(prGen, L"!! FailDecryptPacket(#x %x)\r\n", packet->SequenseNumber, cbTransferred);

	return FALSE;
}

void CALLBACK KdNetDbg::OnKdRecv(
								 _In_ DWORD dwError,
								 _In_ DWORD cbTransferred,
								 _In_ OVERLAPPED* lpOverlapped,
								 _In_ DWORD /*dwFlags*/
								 )
{
	KdNetDbg* pDbg = reinterpret_cast<KdNetDbg*>(lpOverlapped->Pointer);

	//DbgPrint("--Recv(0x%p, 0x%p)\r\n", lpOverlapped, packet);

	if (NOERROR == dwError)
	{
		if (pDbg->OnRecv(&pDbg->_M_packet, cbTransferred + FIELD_OFFSET(KdNetPacket, Sig)))
		{
			pDbg->Recv();
		}
		else
		{
			pDbg->Terminate();
		}
	}
	else
	{
		pDbg->OnRecvError(dwError);
	}

	pDbg->EndRecv();
}

void KdNetDbg::OnRecvError(_In_ DWORD dwError)
{
	if (m_pDoc && !_bittest(&m_flags, ct_quit))
	{
		m_pDoc->printf(prGen, L"OnRecvError(%u, %x)\r\n", dwError, Internal);
		Terminate();
	}
}

void KdNetDbg::StartRecv()
{
	_M_RecvCount++;
}

void KdNetDbg::EndRecv()
{
	if (!--_M_RecvCount)
	{
		if (_bittest(&m_flags, ct_terminate))
		{
			m_pDoc->OnRemoteEnd();
		}
	}
}

void KdNetDbg::Terminate()
{
	_bittestandset(&m_flags, ct_terminate);
	if (!_M_RecvCount)
	{
		__debugbreak();
	}
}

static
VOID
NTAPI
KdPostCleanup (_In_opt_ PVOID /*NormalContext*/,
			   _In_opt_ PVOID /*SystemArgument1*/,
			   _In_opt_ PVOID /*SystemArgument2*/
			   )
{
	WSACleanup();
	DbgPrint("WSACleanup\r\n");
}

KdNetDbg::~KdNetDbg()
{
	DbgPrint("%s<%p>\n", __FUNCTION__, this);
	
	_bittestandset(&m_flags, ct_quit);

	if (_M_hKey2)
	{
		BCryptDestroyKey(_M_hKey2);
	}

	if (_M_hHash)
	{
		BCryptDestroyHash(_M_hHash);
	}

	if (_M_pbHashObject)
	{
		delete [] _M_pbHashObject;
	}

	if (_M_hKey)
	{
		BCryptDestroyKey(_M_hKey);
	}

	if (_M_hHash2)
	{
		BCryptDestroyHash(_M_hHash2);
	}

	if (m_pDoc)
	{
		m_pDoc->Release();
	}

	if (_M_s)
	{
		closesocket(_M_s);
		ZwTestAlert();
	}

	if (_bittest(&m_flags, ct_wsa_init))
	{
		ZwQueueApcThread(NtCurrentThread(), KdPostCleanup, 0, 0, 0);	
	}

	EnableCmd(ID_PIPE, TRUE);
}

HRESULT KdNetDbg::Init(_In_ KdKey* key, _In_ int af, _In_ WORD port, _In_ PCWSTR pcszKey)
{
	ULONG hr;

	if (NOERROR == (hr = KeyPass(key, &_M_hKey, &_M_hHash, &_M_hHash2, &_M_pbHashObject, &_M_cbHashObject)))
	{
		WSADATA wd;
		if (!WSAStartup(WINSOCK_VERSION, &wd))
		{
			_bittestandset(&m_flags, ct_wsa_init);

			if (NOERROR == (hr = CreateKdSocket(&_M_s, af, _byteswap_ushort(port))))
			{
				if (m_pDoc = new ZDbgDoc(TRUE))
				{
					if (m_pDoc->OnRemoteStart(this))
					{
						EnableCmd(ID_PIPE, FALSE);
						m_pDoc->printf(prGen, L"key=\"%s\", port = %u\r\n", pcszKey, port);
						Recv();
						return S_OK;
					}

					m_pDoc->OnRemoteEnd();
				}

				return E_OUTOFMEMORY;
			}

			return hr;
		}

		return GetLastError();
	}

	return hr;
}

HRESULT KdNetDbg::SendPacket(const void* pvData, ULONG cbData, UCHAR Flags)
{
	BCRYPT_KEY_HANDLE hKey = Flags & KdNetPacket::fControlChannel ? _M_hKey : _M_hKey2;

	if (!hKey)
	{
		DbgPrint("KDNET::NoDataChannel\r\n");
		return STATUS_DEVICE_NOT_READY;
	}

	ULONG cb = (sizeof(ULONG64) + cbData + 0xF) & ~0xF;

	union {
		ULONG64 u;
		struct {
			ULONG64 UnusedBytes : 7;
			ULONG64 Response : 1;
			ULONG64 n : 32;
			ULONG64 z : 24;
		};
	};

	u = 0;
	n = ++_M_SequenseNumber;
	UnusedBytes = (cb - cbData) - sizeof(ULONG64);
	Response = 1;

	ULONG cbPacket = cb + sizeof(KdNetPacket) - sizeof(ULONG64);

	if (KdNetPacket* pkt = (KdNetPacket*)new UCHAR[cbPacket])
	{
		pkt->SetSig();
		pkt->ProtocolVersion = _M_ProtocolVersion;
		pkt->Flags = Flags;

		pkt->u = _byteswap_uint64(u);

		memcpy(pkt->buf, pvData, cbData);

		UCHAR hash[0x20];

		NTSTATUS status;
		BCRYPT_HASH_HANDLE hHash;
		if (0 <= (status = BCryptDuplicateHash(_M_hHash, &hHash, _M_pbHashObject + _M_cbHashObject, _M_cbHashObject, 0)))
		{
			0 <= (status = BCryptHashData(hHash, (PBYTE)&pkt->Sig, 6 + cb, 0)) &&
				0 <= (status = BCryptFinishHash(hHash, hash, sizeof(hash), 0));

			BCryptDestroyHash(hHash);

			if (0 <= status)
			{
				memcpy(&pkt->EncBegin + cb, hash, 0x10);

				if (0 <= (status = BCryptEncrypt(hKey, &pkt->EncBegin, cb, 0, hash, 0x10, &pkt->EncBegin, cb, &cb, 0)))
				{
					_M_BytesSend += cbPacket;

					KdSend(_M_s, pkt, cbPacket, (sockaddr*)&_M_sa, _M_len);
				}
			}
		}

		return status;
	}

	return STATUS_NO_MEMORY;
}

HRESULT KdNetDbg::SendPacket(PACKET_KD_TYPE Type, KD_PACKET* pkp, ULONG ByteCount)
{
	ULONG cbPacket = sizeof (KD_PACKET) + ByteCount;

	PBYTE p = (PBYTE)(pkp + 1);

	pkp->PacketLeader = PACKET_LEADER;
	pkp->PacketType = Type;
	pkp->PacketId = m_NextPacketIdToSend;
	pkp->ByteCount = (WORD)ByteCount;
	pkp->Checksum = KdpComputeChecksum(p, ByteCount);

	DbgPrint("KdNetDbg::SendPacket(#%x: %s %x)\r\n", m_NextPacketIdToSend, get(Type, 0, 0), ByteCount);

	m_PacketIdSent = m_NextPacketIdToSend++;

	return SendPacket(pkp, cbPacket);
}

HRESULT KdNetDbg::SendControlPacket(PACKET_KD_TYPE Type, ULONG PacketId)
{
	KD_PACKET packet = {
		CONTROL_PACKET_LEADER, Type, 0, PacketId, 0
	};

	return SendPacket(&packet, sizeof(packet));
}

HRESULT KdNetDbg::SendBreakIn()
{
	DbgPrint("******* BREAKIN_PACKET_BYTE *******\r\n");
	static const BYTE brk = BREAKIN_PACKET_BYTE;
	return SendPacket(&brk, sizeof(brk));
}

HRESULT KdNetDbg::KdContinue(NTSTATUS status)
{
	DbgPrint("%s(%x)\r\n", __FUNCTION__, status);
	KD_PACKET_EX packet;
	RtlZeroMemory(&packet.m_ms, sizeof(packet.m_ms));

	packet.m_ms.ApiNumber = DbgKdContinueApi;
	packet.m_ms.Continue.ContinueStatus = status;
	_bittestandreset(&m_flags, ct_can_send);
	return SendPacket(PACKET_TYPE_KD_STATE_MANIPULATE, &packet, sizeof (DBGKD_MANIPULATE_STATE));
}

HRESULT KdNetDbg::SetBreakPoint(_In_ PVOID BreakPointAddress, _Out_ PULONG BreakPointHandle)
{
	KD_PACKET_EX packet;
	DBGKD_MANIPULATE_STATE* m = &packet.m_ms;

	RtlZeroMemory(m, sizeof(DBGKD_MANIPULATE_STATE));

	m->WriteBreakPoint.BreakPointAddress = (ULONG_PTR)BreakPointAddress;

	NTSTATUS status = SendAndWait(DbgKdWriteBreakPointApi, &packet, sizeof(DBGKD_MANIPULATE_STATE));

	DbgPrint("******************* %s(%p, %x)=%x\r\n", __FUNCTION__, BreakPointAddress, _M_ms.WriteBreakPoint.BreakPointHandle, status);

	if (0 <= status)
	{
		*BreakPointHandle = _M_ms.WriteBreakPoint.BreakPointHandle;
	}
	return status;
}

HRESULT KdNetDbg::DeleteBreakpoint(_In_ ULONG BreakPointHandle)
{
	DbgPrint("******************* %s(%x)\r\n", __FUNCTION__, BreakPointHandle);

	KD_PACKET_EX packet;
	DBGKD_MANIPULATE_STATE* m = &packet.m_ms;

	RtlZeroMemory(m, sizeof(DBGKD_MANIPULATE_STATE));

	m->RestoreBreakPoint.BreakPointHandle = BreakPointHandle;

	return SendAndWait(DbgKdRestoreBreakPointApi, &packet, sizeof(DBGKD_MANIPULATE_STATE));
}

HRESULT KdNetDbg::ReadMemory(_In_ ULONGLONG ulAddress, _In_ DWORD cb, _Out_ PVOID buf, _Out_ PULONG pcb)
{
	KD_PACKET_EX packet;
	DBGKD_MANIPULATE_STATE* m = &packet.m_ms;

	RtlZeroMemory(m, sizeof(DBGKD_MANIPULATE_STATE));

	m->ReadWriteMemory.TargetBaseAddress = ulAddress;
	m->ReadWriteMemory.TransferCount = cb;

	DbgPrint("%s(%I64x, %x)...\r\n", __FUNCTION__, ulAddress, cb);

	*pcb = 0;

	NTSTATUS status = SendAndWait(DbgKdReadVirtualMemoryApi, &packet, sizeof(DBGKD_MANIPULATE_STATE));

	m = &_M_ms;

	DbgPrint("...%s(%I64x)=%x [%x]\n", __FUNCTION__, m->ReadWriteMemory.TargetBaseAddress, status, m->ReadWriteMemory.ActualTransferCount);

	if (0 > status)
	{
		return status;
	}

	if (m->ReadWriteMemory.TargetBaseAddress != ulAddress)
	{
		return STATUS_UNSUCCESSFUL;
	}

	if (ULONG ActualTransferCount = m->ReadWriteMemory.ActualTransferCount)
	{
		memcpy(buf, m + 1, ActualTransferCount);
		*pcb = ActualTransferCount;
	}

	return status;
}

NTSTATUS KdNetDbg::ReadRemote(PBYTE RemoteAddress, PBYTE buf, DWORD cb, PSIZE_T pcb)
{
	SIZE_T rcb;

	if (!pcb)
	{
		pcb = &rcb;
	}

	*pcb = 0;

	RemoteAddress = (PBYTE)m_pDoc->EXT(RemoteAddress);

__loop:

	ULONG dwBytesReaded = 0;

	NTSTATUS status = ReadMemory((ULONGLONG)RemoteAddress, min(cb, PACKET_BYTE_COUNT - sizeof(DBGKD_MANIPULATE_STATE)), buf, &dwBytesReaded);

	DbgPrint("%s=%x [%x]\r\n", __FUNCTION__, status, dwBytesReaded);
	if (0 > status)
	{
		return status;
	}

	if (dwBytesReaded)
	{
		RemoteAddress += dwBytesReaded, buf += dwBytesReaded;
		*pcb += dwBytesReaded;

		if (cb -= dwBytesReaded)
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

NTSTATUS KdNetDbg::WriteMemory(PVOID Address, PVOID Buffer, DWORD cb)
{
	DbgPrint("%s(%p, %x)\r\n", __FUNCTION__, Address, cb);

	if (cb > PACKET_BYTE_COUNT - sizeof(DBGKD_MANIPULATE_STATE))
	{
		return STATUS_BUFFER_OVERFLOW;
	}

	KD_PACKET_EX packet;
	DBGKD_MANIPULATE_STATE* m = &packet.m_ms;

	RtlZeroMemory(m, sizeof(DBGKD_MANIPULATE_STATE));

	m->ReadWriteMemory.TargetBaseAddress = (ULONGLONG)(ULONG_PTR)m_pDoc->EXT(Address);
	m->ReadWriteMemory.TransferCount = cb;

	memcpy(m + 1, Buffer, cb);

	return SendAndWait(DbgKdWriteVirtualMemoryApi, &packet, sizeof (DBGKD_MANIPULATE_STATE) + cb);
}

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

#if __DBG__
void DumpContext(PCONTEXT ctx, PCSTR api, WORD Processor)
{
	DbgPrint("\r\n%hs(%x: EFlags=%p Rsp=%p Rip=%p\r\n"
		"Dr0=%p Dr1=%p Dr2=%p Dr3=%p Dr6=%p Dr7=%p\r\n\r\n", api, Processor, 
		ctx->EFlags, ctx->Rsp, ctx->Rip, 
		ctx->Dr0, ctx->Dr1, ctx->Dr2, ctx->Dr3, ctx->Dr6, ctx->Dr7);
}
#else
#define DumpContext /##/ 
#endif

HRESULT KdNetDbg::SetContext(WORD Processor, PCONTEXT ctx)
{
	DbgPrint("%s(%x)\r\n", __FUNCTION__, Processor);

	KD_PACKET_EX packet;
	DBGKD_MANIPULATE_STATE* m = &packet.m_ms;

	RtlZeroMemory(m, sizeof(DBGKD_MANIPULATE_STATE));

	m->Processor = Processor;
	m->SetContextEx.ByteCount = FIELD_OFFSET(CONTEXT, FltSave);
	m->SetContextEx.BytesCopied = FIELD_OFFSET(CONTEXT, FltSave);

	if (m_pDoc->IsWow64Process())
	{
		m->SetContextEx.ByteCount = FIELD_OFFSET(WOW64_CONTEXT, ExtendedRegisters);
		m->SetContextEx.BytesCopied = FIELD_OFFSET(WOW64_CONTEXT, ExtendedRegisters);
		CopyWowContext((WOW64_CONTEXT*)(m + 1), ctx); 
	}
	else
	{
		memcpy(m + 1, ctx, FIELD_OFFSET(CONTEXT, FltSave));
	}

	DumpContext(ctx, __FUNCTION__, Processor);

	return SendAndWait(DbgKdSetContextExApi, &packet, sizeof(DBGKD_MANIPULATE_STATE) + m->SetContextEx.ByteCount);
}

HRESULT KdNetDbg::GetContext(WORD Processor, PCONTEXT ctx)
{
	DbgPrint("%s(%x)\r\n", __FUNCTION__, Processor);

	KD_PACKET_EX packet;
	DBGKD_MANIPULATE_STATE* m = &packet.m_ms;

	RtlZeroMemory(m, sizeof(DBGKD_MANIPULATE_STATE));

	m->Processor = Processor;

	WOW64_CONTEXT wow_ctx;
	PBYTE pb = (PBYTE)ctx;

	ULONG CtxSize = sizeof(CONTEXT);

	if (m_pDoc->IsWow64Process())
	{
		CtxSize = sizeof(WOW64_CONTEXT);
		pb = (PBYTE)&wow_ctx;
	}

	ULONG Ofs = 0;
	ULONG BytesNeed = CtxSize;

__loop:

	m->GetContextEx.ByteCount = min(BytesNeed, PACKET_BYTE_COUNT - sizeof(DBGKD_MANIPULATE_STATE));
	
	NTSTATUS status = SendAndWait(DbgKdGetContextExApi, &packet, sizeof(DBGKD_MANIPULATE_STATE));
	
	if (0 > status)
	{
		return status;
	}

	m = &_M_ms;

	ULONG ByteCount = _M_msByteCount;

	if (ByteCount > sizeof(DBGKD_MANIPULATE_STATE))
	{
		ByteCount -= sizeof(DBGKD_MANIPULATE_STATE);
	}
	else
	{
		ByteCount = 0;
	}

	DbgPrint("%s(%x, [%x, %x, %x])\r\n", __FUNCTION__, ByteCount, m->GetContextEx.Offset, m->GetContextEx.ByteCount, m->GetContextEx.BytesCopied);

	if (Ofs != m->GetContextEx.Offset || 0x1000 < m->GetContextEx.ByteCount)
	{
		return STATUS_UNSUCCESSFUL;
	}

	ULONG BytesCopied = m->GetContextEx.BytesCopied;

	if (!BytesCopied || BytesCopied != ByteCount)
	{
		return STATUS_UNSUCCESSFUL;
	}

	if (ULONG cb = min(CtxSize, BytesCopied))
	{
		memcpy(pb, m + 1, cb);
		CtxSize -= cb;
	}

	Ofs += BytesCopied, pb += BytesCopied;

	if (Ofs > m->GetContextEx.ByteCount)
	{
		return STATUS_UNSUCCESSFUL;
	}

	if (BytesNeed = m->GetContextEx.ByteCount - Ofs)
	{
		m = &packet.m_ms;
		m->GetContextEx.Offset = Ofs;

		goto __loop;
	}

	if (m_pDoc->IsWow64Process())
	{
		CopyWowContext(ctx, &wow_ctx);
	}

	DumpContext(ctx, __FUNCTION__, Processor);

	return STATUS_SUCCESS;
}

HRESULT KdNetDbg::ReadControlSpace(WORD Processor, PCONTEXT ctx)
{
	DbgPrint("%s(%x)\r\n", __FUNCTION__, Processor);

	KD_PACKET_EX packet;
	DBGKD_MANIPULATE_STATE* m = &packet.m_ms;

	RtlZeroMemory(m, sizeof(DBGKD_MANIPULATE_STATE));

	m->Processor = Processor;
	m->ControlSpace.Type = AMD64_DEBUG_CONTROL_SPACE_KSPECIAL;
	m->ControlSpace.ByteCount = sizeof(KSPECIAL_REGISTERS_X64);

	BOOL IsWow64Process = m_pDoc->IsWow64Process();

	if (IsWow64Process)
	{
		m->ControlSpace.Type = X86_DEBUG_CONTROL_SPACE_KSPECIAL;
		m->ControlSpace.ByteCount = sizeof(KSPECIAL_REGISTERS_X86);
	}

	NTSTATUS status = SendAndWait(DbgKdReadControlSpaceApi, &packet, sizeof(DBGKD_MANIPULATE_STATE));

	if (0 > status)
	{
		return status;
	}

	m = &_M_ms;

	ULONG ByteCount = _M_msByteCount;

	if (ByteCount > sizeof(DBGKD_MANIPULATE_STATE))
	{
		ByteCount -= sizeof(DBGKD_MANIPULATE_STATE);
	}
	else
	{
		ByteCount = 0;
	}

	if (ByteCount > m->ControlSpace.BytesCopied)
	{
		ByteCount = m->ControlSpace.BytesCopied;
	}

	union {
		KSPECIAL_REGISTERS_X86* pk86;
		KSPECIAL_REGISTERS_X64* pk64;
		PVOID pvks;
	};

	pvks = m + 1;

	// save KSPECIAL_REGISTERS for write
	memcpy(&_M_cs, pvks, ByteCount);

#define CTKS(x,i) ctx->Dr##i = pk##x->KernelDr##i;

	if (IsWow64Process)
	{
		CTKS(86, 0);
		CTKS(86, 1);
		CTKS(86, 2);
		CTKS(86, 3);
		CTKS(86, 6);
		CTKS(86, 7);
	}
	else
	{
		CTKS(64, 0);
		CTKS(64, 1);
		CTKS(64, 2);
		CTKS(64, 3);
		CTKS(64, 6);
		CTKS(64, 7);
	}

	DumpContext(ctx, __FUNCTION__, Processor);

	return STATUS_SUCCESS;
}

HRESULT KdNetDbg::WriteControlSpace(WORD Processor, PCONTEXT ctx)
{
	KD_PACKET_EX packet;
	DBGKD_MANIPULATE_STATE* m = &packet.m_ms;

	RtlZeroMemory(m, sizeof(DBGKD_MANIPULATE_STATE));

	m->Processor = Processor;

	m->ControlSpace.Type = AMD64_DEBUG_CONTROL_SPACE_KSPECIAL;

	union {
		KSPECIAL_REGISTERS_X86* pk86;
		KSPECIAL_REGISTERS_X64* pk64;
		PVOID pvks;
	};

	pvks = m + 1;

	memcpy(pvks, &_M_cs, sizeof(KSPECIAL_REGISTERS_X64));

	BOOL IsWow64Process = m_pDoc->IsWow64Process();

	BOOLEAN bModified = FALSE;

#define CTKSK86(m) if (pk86->KernelDr##m != ctx->Dr##m) pk86->KernelDr##m = (ULONG)ctx->Dr##m, bModified = TRUE;
#define CTKSK64(m) if (pk64->KernelDr##m != ctx->Dr##m) pk64->KernelDr##m =        ctx->Dr##m, bModified = TRUE;

	if (IsWow64Process)
	{
		m->ControlSpace.Type = X86_DEBUG_CONTROL_SPACE_KSPECIAL;
		m->ControlSpace.ByteCount = sizeof(KSPECIAL_REGISTERS_X86);

		CTKSK86(0);
		CTKSK86(1);
		CTKSK86(2);
		CTKSK86(3);
		CTKSK86(6);
		CTKSK86(7);
	}
	else
	{
		m->ControlSpace.ByteCount = sizeof(KSPECIAL_REGISTERS_X64);

		CTKSK64(0);
		CTKSK64(1);
		CTKSK64(2);
		CTKSK64(3);
		CTKSK64(6);
		CTKSK64(7);
	}

	if (!bModified)
	{
		return STATUS_SUCCESS;
	}

	DbgPrint("%s(%x)\r\n", __FUNCTION__, Processor);

	DumpContext(ctx, __FUNCTION__, Processor);

	return SendAndWait(DbgKdWriteControlSpaceApi, &packet, sizeof(DBGKD_MANIPULATE_STATE) + m->ControlSpace.ByteCount);
}

HRESULT KdNetDbg::GetVersion()
{
	DbgPrint("*** GetVersion() ****\r\n");

	KD_PACKET_EX packet;
	DBGKD_MANIPULATE_STATE* m = &packet.m_ms;

	RtlZeroMemory(m, sizeof(DBGKD_MANIPULATE_STATE));

	return SendAndWait(DbgKdGetVersionApi, &packet, sizeof (DBGKD_MANIPULATE_STATE));
}

NTSTATUS KdNetDbg::SendAndWait(DBGK_API ApiNumber, KD_PACKET_EX* pkt, ULONG ByteCount)
{
	if (m_WaitApiResponse != DbgKdNoWait || _bittest(&m_flags, ct_event))
	{
		__debugbreak();
	}

	m_WaitApiResponse = ApiNumber;

	pkt->m_ms.ApiNumber = ApiNumber;
	pkt->m_ms.ReturnStatus = STATUS_PENDING;

	NTSTATUS status = SendPacket(PACKET_TYPE_KD_STATE_MANIPULATE, pkt, ByteCount);

	if (S_OK == status)
	{
		Recv();

		ULONG64 time = GetTickCount64() + 4000;
		do 
		{
			SleepEx(1000, TRUE);
		} while (!_bittest(&m_flags, ct_event) && GetTickCount64() < time);
	}

	if (!_bittestandreset(&m_flags, ct_event))
	{
		_M_ms.ReturnStatus = STATUS_IO_TIMEOUT;
	}

	m_WaitApiResponse = DbgKdNoWait;

	return _M_ms.ReturnStatus;
}

BOOL KdNetDbg::OnControlChannelPacket(ULONG SequenseNumber, ControlChannelPacket* packet, ULONG cb)
{
	union {
		NTSTATUS status;
		ULONG len;
	};

	status = STATUS_BAD_DATA;

	if (cb >= sizeof(ControlChannelPacket))
	{
		if (1 == packet->u1)
		{
			union {
				char sz[64];
				UCHAR hash[0x20];
			};

			BCRYPT_HASH_HANDLE hHash;

			switch (packet->u2)
			{
			default:
				DbgPrint("!! OnControlChannelPacket(%x)\r\n", packet->u2);
				break;

			case 1:

				if (_bittest(&m_flags, ct_data_init))
				{
					return TRUE;
				}

				if (0 <= RtlIpv6AddressToStringExA(&packet->ClientIp, 0, packet->ClientPort, sz, &(len = sizeof(sz))))
				{
					DbgPrint("Client: %s\n", sz);
					m_pDoc->printf(prGen, L"From: %S\r\n", sz);
				}

				if (0 <= RtlIpv6AddressToStringExA(&packet->ServerIp, 0, packet->ServerPort, sz, &(len = sizeof(sz))))
				{
					DbgPrint("Server: %s\n", sz);
					m_pDoc->printf(prGen, L"To: %S\r\n", sz);
				}

				if (_M_hKey2)
				{
					BCryptDestroyKey(_M_hKey2);
					_M_hKey2 = 0;
				}

				packet->u2 = 2;

				cb -= (0x158 - 0x142);
				RtlZeroMemory(packet->key2, cb - offsetof(ControlChannelPacket, key2));

				BCryptGenRandom(0, packet->key2, sizeof(packet->key2), BCRYPT_USE_SYSTEM_PREFERRED_RNG);

				if (0 <= (status = BCryptDuplicateHash(_M_hHash2, &hHash, 0, 0, 0)))
				{
					0 <= (status = BCryptHashData(hHash, (PBYTE)packet, cb, 0)) &&
						0 <= (status = BCryptFinishHash(hHash, hash, sizeof(hash), 0));

					BCryptDestroyHash(hHash);

					if (0 <= status)
					{
						BCRYPT_ALG_HANDLE hAlgorithm;

						if (0 <= (status = BCryptOpenAlgorithmProvider(&hAlgorithm, BCRYPT_AES_ALGORITHM, 0, 0)))
						{
							BCRYPT_KEY_HANDLE hKey;
							status = BCryptGenerateSymmetricKey(hAlgorithm, &hKey, 0, 0, hash, sizeof(hash), 0);
							BCryptCloseAlgorithmProvider(hAlgorithm, 0);

							if (0 <= status)
							{
								_M_hKey2 = hKey;
							}
						}
					}
				}

				if (0 <= status)
				{
					DbgPrint("KdNetPacket::InitializeDataChannel\r\n");
					m_pDoc->printf(prGen, L"InitializeDataChannel\r\n");
					SendPacket(packet, cb, KdNetPacket::fControlChannel);
				}

				break;
			}
		}
	}

	DbgPrint("OnControlChannelPacket<#%x>(%x)\r\n", SequenseNumber, cb);

	return 0 <= status;
}

BOOL KdNetDbg::OnPacket(ULONG SequenseNumber, KD_PACKET_EX* packet, ULONG cb)
{
	_bittestandset(&m_flags, ct_data_init);

	if (12 == cb && !_bittest(&m_flags, ct_init))
	{
		DbgPrint(">>> PingPacket[%08x](%x, %x, %x)\r\n", packet->PacketId, packet->PacketLeader, packet->PacketType, packet->ByteCount);

		return TRUE;
	}

	if (cb < sizeof(KD_PACKET) ||
		(cb -= sizeof(KD_PACKET)) != packet->ByteCount ||
		KdpComputeChecksum(packet->m_buffer, cb) != packet->Checksum)
	{
		DbgPrint("!! KDNET::BadPacket(#%x:%x)\r\n", SequenseNumber, cb);
		m_pDoc->printf(prGen, L"!! BadPacket(#%x:%x)\r\n", SequenseNumber, cb);
		return FALSE;
	}

	char szType[16];
	DbgPrint("#%x:OnPacket[%x]> %.4s %s %x\r\n", SequenseNumber, packet->PacketId, &packet->PacketLeader, get(packet->PacketType, szType, _countof(szType)), packet->ByteCount);

	switch (packet->PacketLeader)
	{
	case PACKET_LEADER:
		return OnDataPacket(packet, cb);
	case CONTROL_PACKET_LEADER:
		return OnControlPacket(packet, cb);
	}

	m_pDoc->printf(prGen, L"!! PacketLeader=%08x\r\n", packet->PacketLeader);
	return FALSE;
}

BOOL KdNetDbg::OnControlPacket(KD_PACKET_EX* packet, ULONG cb)
{
	switch (packet->PacketType)
	{
	case PACKET_TYPE_KD_ACKNOWLEDGE:
		return !cb && m_PacketIdSent == packet->PacketId && !_bittest(&m_flags, ct_shutdown);
	case PACKET_TYPE_KD_RESEND:
		__nop();
		break;
	}

	return FALSE;
}

BOOL KdNetDbg::InitSession()
{
	BOOL fOk = FALSE;

	if (0 <= GetVersion())
	{
		_DBGKD_GET_VERSION Version;
		memcpy(&Version, &_M_ms.GetVersion, sizeof(_DBGKD_GET_VERSION));

#if __DBG__
		DbgPrint("DbgKdGetVersionApi: %u.%u %u %x %p %p %p\n", 
			Version.MajorVersion,
			Version.MinorVersion,
			Version.ProtocolVersion,
			Version.MachineType,
			Version.KernBase,
			Version.PsLoadedModuleList,
			Version.DebuggerDataList
			);
#endif

		fOk = m_pDoc->OnRemoteConnect(this, &Version);
	}
	
	if (!fOk)
	{
		Terminate();
	}

	return fOk;
}

BOOL KdNetDbg::OnDataPacket(KD_PACKET_EX* packet, ULONG cb)
{
	ULONG PacketId = packet->PacketId;

	if (PacketId < m_LastPacketId)
	{
		return FALSE;
	}

	SendControlPacket(PACKET_TYPE_KD_ACKNOWLEDGE, PacketId);

	if (_bittestandset(&m_flags, ct_data_received) && PacketId == m_LastPacketId)
	{
		DbgPrint("******* Resend #%x\r\n", PacketId);
		return TRUE;
	}

	m_LastPacketId = PacketId;

	NTSTATUS status = DBG_CONTINUE;

	switch (packet->PacketType)
	{
	default:
		m_pDoc->printf(prGen, L"!! Unknown Packet Type = %x\r\n", packet->PacketType);
		return FALSE;
		//////////////////////////////////////////////////////////////////////////

	case PACKET_TYPE_KD_STATE_CHANGE:

		if (DbgKdNoWait != m_WaitApiResponse)
		{
			goto __api_err;
		}

		if (cb > PACKET_BYTE_COUNT)
		{
			__debugbreak();
		}

		memcpy(&_M_ws, &packet->m_ws, cb), _M_wsByteCount = cb;

		_bittestandset(&m_flags, ct_can_send);

		if (!_bittestandset(&m_flags, ct_init) && !InitSession())
		{
			m_pDoc->printf(prGen, L"!! InitSession()\r\n");
			return FALSE;
		}

		switch (_M_ws.ApiNumber)
		{
		case DbgKdLoadSymbolsStateChange:
			if (_M_ws.LoadSymbols.UnloadSymbols && _M_ws.LoadSymbols.BaseOfDll == ~0)
			{
				DbgPrint("System Shutdown\n");
				m_pDoc->cprintf(prGen, L"System Shutdown\r\n");
				_bittestandset(&m_flags, ct_shutdown);
				break;
			}

#if __DBG__
			DbgPrint("%s(%x) %p [%x %x %s]\n",
				_M_ws.LoadSymbols.UnloadSymbols ? "Unload" : "Load",
				_M_ws.LoadSymbols.ProcessId,
				(PVOID)(ULONG_PTR)_M_ws.LoadSymbols.BaseOfDll, 
				_M_ws.LoadSymbols.SizeOfImage,
				_M_ws.LoadSymbols.CheckSum, 
				_M_ws.Name);
#endif

			goto __cc;

		case DbgKdExceptionStateChange:

			DbgPrint("[Exception] %X at %I64X\n", _M_ws.Exception.ExceptionRecord.ExceptionCode, _M_ws.Exception.ExceptionRecord.ExceptionAddress);
__cc:
			status = m_pDoc->OnWaitStateChange(&_M_ws);

			if (!status)
			{
				return TRUE;
			}
			break;

		default:
			DbgPrint("!! Unknown STATE_CHANGE[%x]\r\n", _M_ws.ApiNumber);
			m_pDoc->printf(prGen, L"!! Unknown STATE_CHANGE[%x]\r\n", _M_ws.ApiNumber);
			return FALSE;
		}

		KdContinue(status);
		return TRUE;

		//////////////////////////////////////////////////////////////////////////

	case PACKET_TYPE_KD_FILE_IO:

		switch (packet->m_fi.ApiNumber)
		{
		case DbgKdCreateFileApi:
			DbgPrint("DbgKdCreateFileApi:%S\r\n", packet->m_fi.CreateFile.Name);
			m_pDoc->printf(prGen, L"DbgKdCreateFileApi:%s\r\n", packet->m_fi.CreateFile.Name);
			break;
		case DbgKdReadFileApi:
			DbgPrint("DbgKdReadFileApi:%p %I64x %x\n", packet->m_fi.ReadFile.Handle, packet->m_fi.ReadFile.Offset, packet->m_fi.ReadFile.Length);
			m_pDoc->printf(prGen, L"DbgKdReadFileApi:%p %I64x %x\r\n", packet->m_fi.ReadFile.Handle, packet->m_fi.ReadFile.Offset, packet->m_fi.ReadFile.Length);
			break;
		case DbgKdWriteFileApi:
			DbgPrint("DbgKdWriteFileApi:%p %I64x %x\n", packet->m_fi.WriteFile.Handle, packet->m_fi.ReadFile.Offset, packet->m_fi.ReadFile.Length);
			m_pDoc->printf(prGen, L"DbgKdWriteFileApi:%p %I64x %x\r\n", packet->m_fi.WriteFile.Handle, packet->m_fi.ReadFile.Offset, packet->m_fi.ReadFile.Length);
			break;
		case DbgKdCloseFileApi:
			DbgPrint("DbgKdCloseFileApi:%p\n", packet->m_fi.CloseFile.Handle);
			m_pDoc->printf(prGen, L"DbgKdCloseFileApi:%p\r\n", packet->m_fi.CloseFile.Handle);
			break;
		default:
			DbgPrint("FILE_IO(%x)\n", packet->m_fi.ApiNumber);
			m_pDoc->printf(prGen, L"DEBUG_IO[%x]\r\n", packet->m_fi.ApiNumber);
			break;
		}

		//DumpBuffer(Buffer, cb);
		packet->m_hd.Status = STATUS_NO_SUCH_FILE;
		SendPacket(PACKET_TYPE_KD_FILE_IO, packet, packet->ByteCount);
		return TRUE;

		//////////////////////////////////////////////////////////////////////////

	case PACKET_TYPE_KD_DEBUG_IO:

		if (cb < offsetof(DBGKD_DEBUG_IO, String[packet->m_io.LengthOfPromptString]))
		{
			packet->m_io.LengthOfPromptString = cb - offsetof(DBGKD_DEBUG_IO, String);
		}
		
		DbgPrint("DEBUG_IO[%x]: %.*s\r\n", packet->m_io.ApiNumber, packet->m_io.LengthOfPromptString, packet->m_io.String);

		switch(packet->m_io.ApiNumber)
		{
		case DbgKdPrintStringApi:
			m_pDoc->printf(prDbgPrint, L"%.*S\r\n", packet->m_io.LengthOfPromptString, packet->m_io.String);
			break;

		case DbgKdGetStringApi:
			KdGetString(GLOBALS_EX::getMainHWND(), &packet->m_io);
			SendPacket(PACKET_TYPE_KD_DEBUG_IO, packet, FIELD_OFFSET(DBGKD_DEBUG_IO, String[packet->m_io.LengthOfStringRead]));
			break;

		default:
			m_pDoc->printf(prGen, L"DEBUG_IO[%x]\r\n", packet->m_io.ApiNumber);
			break;
		}
		return TRUE;

	case PACKET_TYPE_KD_STATE_MANIPULATE:

		if (m_WaitApiResponse != packet->m_ms.ApiNumber)
		{
__api_err:
			DbgPrint("!! API[%x] - bad response(%x)\r\n", m_WaitApiResponse, packet->PacketType);
			m_pDoc->printf(prGen, L"!! API[%x] - bad response(%x)\r\n", m_WaitApiResponse, packet->PacketType);
			return FALSE;
		}

		DbgPrint("**[#%x] API[%x]=%x\r\n", packet->PacketId, packet->m_ms.ApiNumber, packet->m_ms.ReturnStatus);
		
		m_WaitApiResponse = DbgKdNoWait;

		if (cb > PACKET_BYTE_COUNT)
		{
			__debugbreak();
		}

		memcpy(&_M_ms, &packet->m_ms, cb), _M_msByteCount = cb;

		_bittestandset(&m_flags, ct_event);

		return TRUE;
	}
}

NTSTATUS KdNetDbg::FormatFromAddress(PWSTR buf, ULONG cch)
{
	if (_M_len)
	{
		switch (_M_sa.si_family)
		{
		case AF_INET:
			return RtlIpv4AddressToStringExW(&_M_sa.Ipv4.sin_addr, _M_sa.Ipv4.sin_port, buf, &cch);
		case AF_INET6:
			return RtlIpv6AddressToStringExW(&_M_sa.Ipv6.sin6_addr, _M_sa.Ipv6.sin6_scope_id, _M_sa.Ipv6.sin6_port, buf, &cch);
		}
	}

	return STATUS_UNSUCCESSFUL;
}

#include "resource.h"

static INT_PTR CALLBACK KdGetStringProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam)
{
	switch (umsg)
	{
	case WM_INITDIALOG:
		SetWindowLongPtrW(hwnd, DWLP_USER, lParam);

		if (umsg = reinterpret_cast<DBGKD_DEBUG_IO*>(lParam)->LengthOfPromptString)
		{
			PWSTR psz = 0;
			ULONG cch = 0;
			while (cch = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<DBGKD_DEBUG_IO*>(lParam)->String, umsg, psz, cch))
			{
				if (psz)
				{
					psz[cch] = 0;
					SetDlgItemTextW(hwnd, IDC_STATIC, psz);
					break;
				}

				psz = (PWSTR)alloca((1 + cch) * sizeof(WCHAR));
			}
		}

		if (0 <= LoadIconWithScaleDown(0, IDI_INFORMATION, 
			GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), (HICON*)&lParam))
		{
			SendMessageW(hwnd, WM_SETICON, ICON_SMALL, lParam);
		}
		break;

	case WM_NCDESTROY:
		if (lParam = SendMessageW(hwnd, WM_GETICON, ICON_SMALL, 0))
		{
			DestroyIcon((HICON)lParam);
		}
		break;

	case WM_COMMAND:
		switch (wParam)
		{
		case IDOK:
			if (lParam = GetWindowLongPtrW(hwnd, DWLP_USER))
			{
				PWSTR psz = (PWSTR)alloca(0x100);
				if (umsg = GetDlgItemTextW(hwnd, IDC_EDIT1, psz, 0x100))
				{
					umsg = WideCharToMultiByte(CP_UTF8, 0, psz, umsg + 1, 
						reinterpret_cast<DBGKD_DEBUG_IO*>(lParam)->String, 0x100, 0, 0);

					if (umsg <= reinterpret_cast<DBGKD_DEBUG_IO*>(lParam)->LengthOfStringRead)
					{
						reinterpret_cast<DBGKD_DEBUG_IO*>(lParam)->LengthOfStringRead = umsg;
						EndDialog(hwnd, S_OK);
						break;
					}

					swprintf_s(psz, 0x100, L"Max accepted characters is %u:", 
						reinterpret_cast<DBGKD_DEBUG_IO*>(lParam)->LengthOfStringRead - 1);

					ShowErrorBox(hwnd, STATUS_NAME_TOO_LONG, psz, MB_ICONWARNING);
				}
				else
				{
					SetFocus(GetDlgItem(hwnd, IDC_EDIT1));
				}
			}
			break;
		case IDCANCEL:
			EndDialog(hwnd, ERROR_CANCELLED);
			break;
		}
		break;
	}
	return 0;
}

void KdGetString(HWND hwnd, DBGKD_DEBUG_IO* pdio)
{
	HRESULT hr = (HRESULT)DialogBoxParamW((HINSTANCE)&__ImageBase, MAKEINTRESOURCEW(IDD_DIALOG26), hwnd, KdGetStringProc, (LPARAM)pdio);
	if (S_OK != hr)
	{
		pdio->LengthOfStringRead = 0;
	}
}

//////////////////////////////////////////////////////////////////////////

static BOOL OnKdOk(HWND hwnd)
{
	ULONG a1 = -(BST_CHECKED == IsDlgButtonChecked(hwnd, IDC_RADIO1));
	ULONG a2 = -(BST_CHECKED == IsDlgButtonChecked(hwnd, IDC_RADIO2));
	if (!(a1 ^ a2))
	{
		return FALSE;
	}

	INT af = (a1 & AF_INET) | (a2 & AF_INET6);

	BOOL f;
	ULONG port = GetDlgItemInt(hwnd, IDC_EDIT2, &f, FALSE);

	if (!f || port - 1 > MAXUSHORT)
	{
		ShowErrorBox(hwnd, STATUS_INVALID_PARAMETER, L"Port Number", MB_ICONWARNING);
		SetFocus(GetDlgItem(hwnd, IDC_EDIT2));
		return FALSE;
	}

	WCHAR szKey[64];
	if (ULONG cch = GetDlgItemTextW(hwnd, IDC_EDIT1, szKey, _countof(szKey) - 1))
	{
		szKey[cch] = '.';
		szKey[cch + 1] = 0;
		KdKey key;

		if (S_OK == KeyToBin(szKey, key.u, _countof(key.u)))
		{
			szKey[cch] = 0;

			HRESULT hr = E_OUTOFMEMORY;

			if (KdNetDbg* pDbg = new KdNetDbg)
			{
				hr = pDbg->Init(&key, af, (USHORT)port, szKey);

				pDbg->Release();

				if (S_OK == hr)
				{
					return TRUE;
				}

				ZwTestAlert();
			}

			ShowErrorBox(hwnd, hr, L"Init Fail", MB_ICONWARNING);
			return FALSE;
		}
	}

	ShowErrorBox(hwnd, STATUS_BAD_DATA, L"Invalid Key", MB_ICONWARNING);
	SetFocus(GetDlgItem(hwnd, IDC_EDIT2));
	return FALSE;
}

static INT_PTR CALLBACK KdDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam)
{
	switch (umsg)
	{
	case WM_INITDIALOG:
		SendDlgItemMessageW(hwnd, IDC_RADIO2, BM_SETCHECK, BST_CHECKED, 0);
		SendDlgItemMessageW(hwnd, IDC_EDIT1, EM_SETCUEBANNER, TRUE, (LPARAM)L"Enter key from \"bcdedit /dbgsettings\" command");
		SetDlgItemTextW(hwnd, IDC_EDIT2, L"50000");//
		//SetDlgItemTextW(hwnd, IDC_EDIT1, L"1ekbauz6ljo0j.2ajy4zzw9r1q3.25pxv5zv64rsp.27az9eqfd9wwm");

		if (0 <= LoadIconWithScaleDown(0, IDI_INFORMATION, 
			GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), (HICON*)&lParam))
		{
			SendMessageW(hwnd, WM_SETICON, ICON_SMALL, lParam);
		}
		break;

	case WM_NCDESTROY:
		if (lParam = SendMessageW(hwnd, WM_GETICON, ICON_SMALL, 0))
		{
			DestroyIcon((HICON)lParam);
		}
		break;

	case WM_COMMAND:
		switch (wParam)
		{
		case IDOK:
			if (OnKdOk(hwnd))
			{
				EndDialog(hwnd, S_OK);
			}
			break;
		case IDCANCEL:
			EndDialog(hwnd, ERROR_CANCELLED);
			break;
		}
		break;
	}
	return 0;
}

HRESULT StartKdNet(HWND hwnd)
{
	HRESULT hr;
	if (hr = (HRESULT)DialogBoxParamW((HINSTANCE)&__ImageBase, MAKEINTRESOURCEW(IDD_DIALOG25), hwnd, KdDlgProc, 0))
	{
		
	}
	return -1 == hr ? GetLastError() : hr;
}

_NT_END