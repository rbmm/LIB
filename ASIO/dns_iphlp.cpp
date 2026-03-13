#include "StdAfx.h"

_NT_BEGIN
#include <mstcpip.h>
#include <iphlpapi.h>
#include <WinDNS.h>
#include "socket.h"
//#define DbgPrint /##/

static volatile UCHAR guz;

//////////////////////////////////////////////////////////////////////////
void ALS(PSRWLOCK SRWLock);
void ALE(PSRWLOCK SRWLock);
void RLS(PSRWLOCK SRWLock);
void RLE(PSRWLOCK SRWLock);

void (WINAPI*AcqSRWLockShared)(PSRWLOCK SRWLock) = ALS;
void (WINAPI*RelSRWLockShared)(PSRWLOCK SRWLock) = RLS;
void (WINAPI*AcqSRWLockExclusive)(PSRWLOCK SRWLock) = ALE;
void (WINAPI*RelSRWLockExclusive)(PSRWLOCK SRWLock) = RLE;

void WINAPI dummy_xp(PSRWLOCK)
{
}

void ALS(PSRWLOCK SRWLock)
{
	if (PVOID pv = GetProcAddress(GetModuleHandle(L"kernel32"), "AcquireSRWLockShared"))
	{
		AcqSRWLockShared = (void (WINAPI*)(PSRWLOCK))pv;
		AcqSRWLockShared(SRWLock);
	}
	else
	{
		AcqSRWLockShared = dummy_xp;
	}
}

void ALE(PSRWLOCK SRWLock)
{
	if (PVOID pv = GetProcAddress(GetModuleHandle(L"kernel32"), "AcquireSRWLockExclusive"))
	{
		AcqSRWLockExclusive = (void (WINAPI*)(PSRWLOCK))pv;
		AcqSRWLockExclusive(SRWLock);
	}
	else
	{
		AcqSRWLockExclusive = dummy_xp;
	}
}

void RLS(PSRWLOCK SRWLock)
{
	if (PVOID pv = GetProcAddress(GetModuleHandle(L"kernel32"), "ReleaseSRWLockShared"))
	{
		RelSRWLockShared = (void (WINAPI*)(PSRWLOCK))pv;
		RelSRWLockShared(SRWLock);
	}
	else
	{
		RelSRWLockShared = dummy_xp;
	}
}

void RLE(PSRWLOCK SRWLock)
{
	if (PVOID pv = GetProcAddress(GetModuleHandle(L"kernel32"), "ReleaseSRWLockExclusive"))
	{
		RelSRWLockExclusive = (void (WINAPI*)(PSRWLOCK))pv;
		RelSRWLockExclusive(SRWLock);
	}
	else
	{
		RelSRWLockExclusive = dummy_xp;
	}
}

DWORD HashString(PCSTR lpsz, DWORD hash = 0)
{
	while (char c = *lpsz++) hash = hash * 33 ^ c;
	return hash;
}


struct DnsCache 
{
	struct CRC_IP_TIME { ULONG crc, ip, time, n; };

	inline static CRC_IP_TIME g_ip_cache[64] = {};
	inline static SRWLOCK g_icl = SRWLOCK_INIT;

	static ULONG get(PCSTR name, ULONG& rcrc);
	static void set(ULONG crc, ULONG ip);
};

ULONG DnsCache::get(PCSTR name, ULONG& rcrc)
{
	ULONG crc = HashString(name);

	rcrc = crc;

	CRC_IP_TIME* p = &g_ip_cache[crc & (RTL_NUMBER_OF(g_ip_cache) - 1)];

	ULONG time = GetTickCount();

	ULONG ip = 0;
	AcqSRWLockShared(&g_icl);
	if (p->crc == crc && time < p->time) ip = p->ip;
	RelSRWLockShared(&g_icl);

	return ip;
}

void DnsCache::set(ULONG crc, ULONG ip)
{
	CRC_IP_TIME* p = &g_ip_cache[crc & (RTL_NUMBER_OF(g_ip_cache) - 1)];

	ULONG time = GetTickCount() + 1000*100;// 100 sec

	AcqSRWLockExclusive(&g_icl);
	p->crc = crc, p->time = time, p->ip = ip, p->n++;
	RelSRWLockExclusive(&g_icl);
}

//////////////////////////////////////////////////////////////////////////
class CDnsSocket;

class CDnsTask : public IO_OBJECT_TIMEOUT
{
	CSocketObject* _pEndp;
	CDnsSocket** _ppSocks;
	ULONG _n;
	LONG _nRecvCount;
	LONG _bFirstIp;

	virtual void IOCompletionRoutine(CDataPacket* , DWORD , NTSTATUS , ULONG_PTR , PVOID )
	{
		__debugbreak();
	}

	virtual void OnTimeout()
	{
		DbgPrint("%s<%p>\n", __FUNCTION__, this);
		Cleanup();
	}

	ULONG Create(ULONG n);

	void Cleanup();

	~CDnsTask()
	{
		DbgPrint("%s<%p>\n", __FUNCTION__, this);
		Cleanup();
		if (_bFirstIp)
		{
			_pEndp->OnIp(0);
		}
		_pEndp->Release();
	}

public:

	void DecRecvCount();

	void OnIp(ULONG ip)
	{
		if (InterlockedExchangeNoFence(&_bFirstIp, FALSE))
		{
			DbgPrint("%s<%p> (%08x)\n", __FUNCTION__, this, ip);
			Cleanup();
			_pEndp->OnIp(ip);
		}
	}

	CDnsTask(CSocketObject* pEndp) : _ppSocks(0), _n(0), _bFirstIp(TRUE), _pEndp(pEndp)
	{
		DbgPrint("%s<%p>\n", __FUNCTION__, this);
		_pEndp->AddRef();
	}

	void DnsToIp(PCSTR Dns, ULONG crc, DWORD dwMilliseconds = 3000);
};

class CDnsSocket : public CUdpEndpoint
{
	LONGLONG _time;
	CDnsTask* _pTask;
	ULONG _crc;
	ULONG _ip;
	USHORT _id;

	virtual void OnRecv(PSTR Buffer, ULONG cbTransferred);
	virtual void OnRecv(PSTR Buffer, ULONG cbTransferred, CDataPacket* /*packet*/, SOCKADDR_IN* addr);

	~CDnsSocket()
	{
		DbgPrint("%s<%p>\n", __FUNCTION__, this);
		_pTask->Release();
	}

public:

	BOOL Start(PCSTR Dns, ULONG crc, PSOCKADDR Address, DWORD AddressLength);

	CDnsSocket(CDnsTask* pTask) : _pTask(pTask)
	{
		pTask->AddRef();
		DbgPrint("%s<%p>\n", __FUNCTION__, this);
	}
};

//////////////////////////////////////////////////////////////////////////

void CDnsTask::Cleanup()
{
	DbgPrint("%s<%p> (%p)\n", __FUNCTION__, this, _ppSocks);

	StopTimeout();

	if (PVOID pv = InterlockedExchangePointerAcquire((void**)&_ppSocks, 0))
	{
		CDnsSocket** ppSocks = (CDnsSocket**)pv;

		if (ULONG n = _n)
		{
			do 
			{
				CDnsSocket* pSocks = *ppSocks++;
				pSocks->Close();
				pSocks->Release();
			} while (--n);
		}

		delete [] pv;
	}
}

void CDnsTask::DecRecvCount()
{
	if (!InterlockedDecrementNoFence(&_nRecvCount))
	{
		DbgPrint("%s<%p> (%p)\n", __FUNCTION__, this, _ppSocks);
		Cleanup();
	}
}

PIP_ADAPTER_ADDRESSES AdaptersAddresses()
{
	ULONG cb = max(sizeof(IP_ADAPTER_ADDRESSES), 0x4000), dwError;

	union {
		PVOID buf;
		PIP_ADAPTER_ADDRESSES AdapterAddresses;
		PBYTE pb;
	};

	do 
	{
		enum { 
			ex = (__alignof(IP_ADAPTER_ADDRESSES) + sizeof(IP_ADAPTER_ADDRESSES) + 
			2*(sizeof(IP_ADAPTER_DNS_SERVER_ADDRESS_XP) + sizeof(SOCKADDR_IN)) - 1) 
			& ~(__alignof(IP_ADAPTER_ADDRESSES) - 1)
		};

		if (buf = LocalAlloc(0, cb + ex ))
		{
			PIP_ADAPTER_ADDRESSES SystemAdapterAddresses = (PIP_ADAPTER_ADDRESSES)(pb + ex);

			dwError = GetAdaptersAddresses(AF_INET, 
				GAA_FLAG_SKIP_UNICAST|GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST|
				GAA_FLAG_SKIP_FRIENDLY_NAME, 0, SystemAdapterAddresses, &cb);

			if (dwError == NOERROR)
			{
				AdapterAddresses->Next = SystemAdapterAddresses;
				AdapterAddresses->OperStatus = IfOperStatusUp;
				AdapterAddresses->Ipv4Enabled = TRUE;
				AdapterAddresses->IfType = IF_TYPE_OTHER;
				IP_ADAPTER_DNS_SERVER_ADDRESS_XP* DnsServerAddress = (IP_ADAPTER_DNS_SERVER_ADDRESS_XP*)(AdapterAddresses+1);
				AdapterAddresses->FirstDnsServerAddress = DnsServerAddress;
				
				union {
					SOCKADDR_IN* psain;
					SOCKADDR* psa;
					PVOID pv;
				};

				pv = DnsServerAddress + 2;

				static ULONG seed;

				if (!seed)
				{
					seed = ~GetTickCount();
				}

				ULONG i = RtlRandomEx(&seed);

				static const ULONG DnsServerAddressesGroup1[] = {
					IP(8, 8, 8, 8),
					IP(8, 8, 4, 4),
				};

				static const ULONG DnsServerAddressesGroup2[] = {
					IP(208, 67, 222, 222),
					IP(208, 67, 222, 220),
					IP(208, 67, 220, 220),
					IP(208, 67, 220, 222),
				};

				DnsServerAddress->Next = DnsServerAddress + 1;
				DnsServerAddress->Address.lpSockaddr = psa;
				DnsServerAddress++->Address.iSockaddrLength = sizeof(SOCKADDR_IN);

				RtlZeroMemory(psain->sin_zero, sizeof(psain->sin_zero));
				psain->sin_addr.S_un.S_addr = DnsServerAddressesGroup1[i % _countof(DnsServerAddressesGroup1)];
				psain++->sin_family = AF_INET;

				DnsServerAddress->Next = 0;
				DnsServerAddress->Address.lpSockaddr = psa;
				DnsServerAddress->Address.iSockaddrLength = sizeof(SOCKADDR_IN);

				RtlZeroMemory(psain->sin_zero, sizeof(psain->sin_zero));
				psain->sin_addr.S_un.S_addr = DnsServerAddressesGroup2[i % _countof(DnsServerAddressesGroup2)];
				psain->sin_family = AF_INET;

				return AdapterAddresses;
			}

			LocalFree(buf);
		}
		else
		{
			break;
		}
	} while(dwError == ERROR_BUFFER_OVERFLOW);

	return 0;
}

void CDnsTask::DnsToIp(PCSTR Dns, ULONG crc, DWORD dwMilliseconds)
{
	if (PIP_ADAPTER_ADDRESSES _AdapterAddresses = AdaptersAddresses())
	{
		ULONG n = 0, i;
		CDnsSocket** ppSocks = 0, *pSock;

		for(bool bFinal = false;;) 
		{
			PIP_ADAPTER_ADDRESSES AdapterAddresses = _AdapterAddresses;

			do 
			{
				if (AdapterAddresses->OperStatus != IfOperStatusUp || 
					!AdapterAddresses->Ipv4Enabled ||
					AdapterAddresses->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
				{
					continue;
				}

				if (IP_ADAPTER_DNS_SERVER_ADDRESS_XP* DnsServerAddress = AdapterAddresses->FirstDnsServerAddress)
				{
					do 
					{
						switch (DnsServerAddress->Address.lpSockaddr->sa_family)
						{
						case AF_INET:
							if (sizeof(SOCKADDR_IN) <= DnsServerAddress->Address.iSockaddrLength)
							{
								if (bFinal)
								{
									pSock = *--ppSocks;

									BOOL fOk = FALSE;

									if (CDataPacket* packet = new(1024) CDataPacket)
									{
										if (!pSock->RecvFrom(packet) && 
											pSock->Start(Dns, crc, DnsServerAddress->Address.lpSockaddr, 
											DnsServerAddress->Address.iSockaddrLength))
										{
											fOk = TRUE;
										}
										packet->Release();
									}

									if (!fOk)
									{
										DecRecvCount();
									}

									pSock->Release();
								}
								else
								{
									reinterpret_cast<SOCKADDR_IN*>(DnsServerAddress->Address.lpSockaddr)->sin_port = DNS_PORT_NET_ORDER;
									n++;
								}
							}
							break;
						}

					} while (DnsServerAddress = DnsServerAddress->Next);
				}
			} while (AdapterAddresses = AdapterAddresses->Next);

			if (bFinal)
			{
				DecRecvCount();
				break;
			}

			bFinal = true;

			if (!(n = Create(n)) || !SetTimeout(dwMilliseconds))
			{
				Cleanup();
				break;
			}

			_nRecvCount = n + 1;
			ppSocks = _ppSocks;

			i = n;
			do 
			{
				(*ppSocks++)->AddRef();
			} while (--i);
		}

		LocalFree(_AdapterAddresses);
	}
}

ULONG CDnsTask::Create(ULONG n)
{
	if (!n)
	{
		return 0;
	}

	if (CDnsSocket** ppSocks = new CDnsSocket*[n])
	{
		CDnsSocket** ppSocks2 = ppSocks;
		ULONG m = 0;

		do 
		{
			if (CDnsSocket* pSocks = new CDnsSocket(this))
			{
				if (pSocks->Create(0))
				{
					pSocks->Release();
				}
				else
				{
					*ppSocks++ = pSocks;
					m++;
				}
			}
		} while (--n);

		_ppSocks = ppSocks2;		
		// -- memory_order_release

		if (m)
		{
			_n = m;
			return m;
		}
	}

	return 0;
}

BOOL CDnsSocket::Start(PCSTR Dns, ULONG crc, PSOCKADDR Address, DWORD AddressLength)
{
	_crc = crc;

	if (CDataPacket* packet = new(DNS_RFC_MAX_UDP_PACKET_LENGTH) CDataPacket)
	{
		union {
			DNS_HEADER* pdh;
			PSTR lpsz;
			PBYTE pb;
		};
		
		lpsz = packet->getData();
		PSTR _lpsz, __lpsz = lpsz;
		char c, i;
		RtlZeroMemory(pdh, sizeof(DNS_HEADER));
		ULONG seed = (ULONG)(ULONG_PTR)this ^ ~GetTickCount();
		pdh->Xid = _id = (USHORT)RtlRandomEx(&seed);
		pdh->RecursionDesired = 1;
		pdh++->QuestionCount = 0x0100; //_byteswap_ushort(1);

		_ip = 0;
		if (Address->sa_family == AF_INET)
		{
			_ip = reinterpret_cast<SOCKADDR_IN*>(Address)->sin_addr.S_un.S_addr;
		}

		static const DNS_WIRE_QUESTION dwq = { 0x0100, 0x0100 }; // { _byteswap_ushort(DNS_TYPE_A), _byteswap_ushort(DNS_CLASS_INTERNET) }

		do 
		{
			_lpsz = lpsz++, i = 0;
mm:
			switch (c = *Dns++)
			{
			case '.':
			case 0:
				break;
			default:*lpsz++ = c, ++i;
				goto mm;
			}
			*_lpsz = i;
		} while (c);

		*lpsz++ = 0;

		memcpy(lpsz, &dwq, sizeof(dwq));

		packet->setDataSize(RtlPointerToOffset(__lpsz, lpsz) + sizeof(dwq));

		GetSystemTimeAsFileTime((PFILETIME)&_time);
		ULONG err = SendTo(Address, AddressLength, packet);

		packet->Release();

		return !err;
	}

	return FALSE;
}

void CDnsSocket::OnRecv(PSTR Buffer, ULONG cbTransferred)
{
	if (cbTransferred < sizeof(DNS_HEADER) || reinterpret_cast<DNS_HEADER*>(Buffer)->Xid != _id) return ;

	Buffer += sizeof(DNS_HEADER), cbTransferred -= sizeof(DNS_HEADER);

	UCHAR c;

	while (cbTransferred-- && (c = *Buffer++))
	{
		if (cbTransferred < c) return;
		cbTransferred -= c, Buffer += c;
	}

	if (cbTransferred < sizeof(DNS_WIRE_QUESTION)) return ;
	Buffer += sizeof(DNS_WIRE_QUESTION), cbTransferred -= sizeof(DNS_WIRE_QUESTION);

	for(;;) 
	{
		if (!cbTransferred)
		{
			return;
		}
		
		if ((*Buffer & 0xc0) == 0xc0)
		{
			//compressed question name
			if (cbTransferred < sizeof(USHORT)) return ;
			Buffer += sizeof(USHORT), cbTransferred -= sizeof(USHORT);
		}
		else
		{
			while (cbTransferred-- && (c = *Buffer++))
			{
				if (cbTransferred < c) return;
				cbTransferred -= c, Buffer += c;
			}
		}

		if (cbTransferred < sizeof(DNS_WIRE_RECORD)) return;

		DNS_WIRE_RECORD dwr;

		memcpy(&dwr, Buffer, sizeof(dwr));

		cbTransferred -= sizeof (DNS_WIRE_RECORD), Buffer += sizeof (DNS_WIRE_RECORD);
		
		if (cbTransferred < (dwr.DataLength = _byteswap_ushort(dwr.DataLength))) return;

		DbgPrint("[%08x]{ %04x %04x : %04x }\n", _ip, dwr.RecordType, dwr.RecordClass, dwr.DataLength);
		
		if (dwr.RecordType == 0x100/*DNS_TYPE_A*/ && dwr.DataLength == sizeof(IP4_ADDRESS))
		{
			IP4_ADDRESS ip;
			memcpy(&ip, Buffer, sizeof(IP4_ADDRESS));

			if (ip)
			{
				LONGLONG time;
				GetSystemTimeAsFileTime((PFILETIME)&time);
				DbgPrint("[%08x]->%08x [%016Iu]\n", _ip, ip, time - _time);
				DnsCache::set(_crc, ip);
				_pTask->OnIp(ip);
				return;
			}
		}
		cbTransferred -= dwr.DataLength, Buffer += dwr.DataLength;
	}
}

void CDnsSocket::OnRecv(PSTR Buffer, ULONG cbTransferred, CDataPacket* /*packet*/, SOCKADDR_IN* /*addr*/)
{
	if (Buffer) OnRecv(Buffer, cbTransferred);
	_pTask->DecRecvCount();
}

//////////////////////////////////////////////////////////////////////////

void CSocketObject::DnsToIp(PCSTR Dns)
{
	ULONG crc, ip;
	PCSTR c;
	if (0 <= RtlIpv4StringToAddressA(Dns, TRUE, &c, (in_addr*)&ip) || (ip = DnsCache::get(Dns, crc)))
	{
		OnIp(ip);
		return;
	}

	if (strlen(Dns) > DNS_MAX_TEXT_STRING_LENGTH)
	{
		OnIp(0);
		return;
	}

	if (CDnsTask* pTask = new CDnsTask(this))
	{
		pTask->DnsToIp(Dns, crc);
		pTask->Release();
	}
	else
	{
		OnIp(0);
	}
}

_NT_END