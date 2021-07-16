#include "StdAfx.h"

_NT_BEGIN
#include "socket.h"
#define DbgPrint /##/
//
static volatile UCHAR guz;

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

	static ULONG get(ULONG crc);
	static void set(ULONG crc, ULONG ip);
};

ULONG DnsCache::get(ULONG crc)
{
	CRC_IP_TIME* p = &g_ip_cache[crc & (RTL_NUMBER_OF(g_ip_cache) - 1)];

	ULONG time = GetTickCount();

	ULONG ip = 0;
	AcquireSRWLockShared(&g_icl);
	if (p->crc == crc && time < p->time) ip = p->ip;
	ReleaseSRWLockShared(&g_icl);

	return ip;
}

void DnsCache::set(ULONG crc, ULONG ip)
{
	CRC_IP_TIME* p = &g_ip_cache[crc & (RTL_NUMBER_OF(g_ip_cache) - 1)];

	ULONG time = GetTickCount() + 1000*100;// 100 sec

	AcquireSRWLockExclusive(&g_icl);
	p->crc = crc, p->time = time, p->ip = ip, p->n++;
	ReleaseSRWLockExclusive(&g_icl);
}

//////////////////////////////////////////////////////////////////////////

ULONG FillDnsServerList(ULONG MaxCount, ULONG IPs[])
{
	HANDLE hKey;
	STATIC_OBJECT_ATTRIBUTES(soa, "\\registry\\MACHINE\\SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces");
	UNICODE_STRING ObjectName;
	OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &ObjectName, OBJ_CASE_INSENSITIVE };

	ULONG m = 0;

	NTSTATUS status = ZwOpenKey(&oa.RootDirectory, KEY_READ, &soa);

	if (0 <= status)
	{
		PVOID stack = alloca(guz);
		union {
			PVOID buf;
			PKEY_BASIC_INFORMATION pkni;
			PKEY_VALUE_PARTIAL_INFORMATION pkvpi;
		};
		ULONG cb = 0, rcb = 0x100, Index = 0;

		do 
		{
			do 
			{
				if (cb < rcb) cb = RtlPointerToOffset(buf = alloca(rcb - cb), stack);

				if (0 <= (status = ZwEnumerateKey(oa.RootDirectory, Index, KeyBasicInformation, buf, cb, &rcb)))
				{
					ObjectName.Buffer = pkni->Name;
					ObjectName.MaximumLength = ObjectName.Length = (USHORT)pkni->NameLength;

					DbgPrint("========================\r\n%wZ:\r\n", &ObjectName);

					if (0 <= ZwOpenKey(&hKey, KEY_READ, &oa))
					{
						STATIC_UNICODE_STRING_(NameServer);
						STATIC_UNICODE_STRING_(DhcpNameServer);
						NTSTATUS ss;
						PCUNICODE_STRING aa[] = { &DhcpNameServer, &NameServer }, ValueName;
						ULONG n = _countof(aa);

						do 
						{
							ValueName = aa[--n];

							do 
							{
								if (cb < rcb) cb = RtlPointerToOffset(buf = alloca(rcb - cb), stack);

								if (0 <= (ss = ZwQueryValueKey(hKey, ValueName, KeyValuePartialInformation, buf, cb, &rcb)))
								{
									ULONG DataLength = pkvpi->DataLength;
									union {
										PWSTR psz;
										PBYTE Data;
									};
									Data = pkvpi->Data;
									if (pkvpi->Type == REG_SZ && 
										DataLength > sizeof(WCHAR) && 
										!(DataLength & (sizeof(WCHAR) - 1)) &&
										!*(WCHAR*)(Data + DataLength - sizeof(WCHAR)))
									{
										DbgPrint("found %wZ = %S\r\n", ValueName, psz);
										ULONG ip;

										for (;;)
										{
__skip:
											switch (*psz)
											{
											case ' ':
											case ',':
												psz++;
												goto __skip;
											}

											if (!*psz || 0 > RtlIpv4StringToAddressW(psz, TRUE, (PCWSTR*)&psz, (in_addr*)&ip))
											{
												DbgPrint("invalid Ipv4 String\r\n");
												break;
											}

											n = 0;// not look for DhcpNameServer if found NameServer
											*IPs++ = ip;
											DbgPrint("ip=%08x\r\n", ip);
											if (++m, !--MaxCount)
											{
												NtClose(hKey);
												goto __exit;
											}
										}
									}
								}

							} while (ss == STATUS_BUFFER_OVERFLOW);

						} while (n);

						NtClose(hKey);
					}
				}

			} while (status == STATUS_BUFFER_OVERFLOW);

			Index++;

		} while (status != STATUS_NO_MORE_ENTRIES);
__exit:
		NtClose(oa.RootDirectory);
	}

	return m;
}

//////////////////////////////////////////////////////////////////////////
struct DCD 
{
	USHORT _Xid;
	USHORT _QueryType;
};

class CDnsSocket : public CUdpEndpoint
{
	CSocketObject* _pEndp;
	ULONG _crc;
	LONG _waitCount = 1;
	ULONG t = GetTickCount();

	~CDnsSocket()
	{
		OnIp(0, 0);
		DbgPrint("%s<%p> %u\n", __FUNCTION__, this, GetTickCount() - t);
	}

	void OnIp(PSOCKADDR Address, DWORD AddressLength)
	{
		if (CSocketObject* pEndp = (CSocketObject*)InterlockedExchangePointerNoFence((void**)&_pEndp, 0))
		{
			Close();
			StopTimeout();
			pEndp->OnIp(Address, AddressLength);
			pEndp->Release();
		}
	}

	void OnRecv(PSTR Buffer, ULONG cbTransferred, USHORT Xid, USHORT QueryType);

	virtual void OnRecv(PSTR Buffer, ULONG cbTransferred, CDataPacket* packet, SOCKADDR_IN_EX* addr);

	void SendAndRecv(
		_In_ PSOCKADDR Address, 
		_In_ DWORD AddressLength, 
		_In_ PCSTR Dns, 
		_In_ USHORT QueryType,
		_In_ USHORT Xid,
		_In_ bool RecursionDesired);

	BOOL SendToServer(
		_In_ PSOCKADDR Address, 
		_In_ DWORD AddressLength, 
		_In_ PCSTR Dns, 
		_In_ USHORT QueryType,
		_In_ USHORT Xid,
		_In_ bool RecursionDesired);

	void DecWaitCount()
	{
		if (!InterlockedDecrement(&_waitCount))
		{
			StopTimeout();
		}
	}
public:

	CDnsSocket(CSocketObject* pEndp) : _pEndp(pEndp)
	{
		DbgPrint("%s<%p>\n", __FUNCTION__, this);
		_pEndp->AddRef();
	}

	void DnsToIp(_In_ PCSTR Dns, _In_ ULONG crc, _In_ WORD QueryType, _In_ LONG QueryOptions, _In_ DWORD dwMilliseconds = 4000);
};

void CDnsSocket::DnsToIp(_In_ PCSTR Dns, _In_ ULONG crc, _In_ USHORT QueryType, _In_ LONG QueryOptions, _In_ DWORD dwMilliseconds)
{
	_crc = crc;

	ULONG seed = (ULONG)(ULONG_PTR)this ^ ~GetTickCount();

	ULONG i = RtlRandomEx(&seed);

	USHORT Xid = (USHORT)i;

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

	ULONG DnsServerAddresses[16] = { 
		DnsServerAddressesGroup1[i % RTL_NUMBER_OF(DnsServerAddressesGroup1)],
		DnsServerAddressesGroup2[i % RTL_NUMBER_OF(DnsServerAddressesGroup2)],
	};

	i = QueryOptions & DNS_QUERY_NO_WIRE_QUERY ? 0 : 2;

	i += FillDnsServerList(RTL_NUMBER_OF(DnsServerAddresses) - i, DnsServerAddresses + i);

	if (i && Create(0) == NOERROR && SetTimeout(dwMilliseconds))
	{
		SOCKADDR_IN Ipv4 = { AF_INET, DNS_PORT_NET_ORDER };

		if (i)
		{
			bool RecursionDesired = !(QueryOptions & DNS_QUERY_NO_RECURSION);
			do 
			{
				Ipv4.sin_addr.s_addr = DnsServerAddresses[--i];

				SendAndRecv((PSOCKADDR)&Ipv4, sizeof(Ipv4), Dns, QueryType, Xid++, RecursionDesired);

			} while (i);
		}

		DecWaitCount();
	}
}

void CDnsSocket::SendAndRecv(_In_ PSOCKADDR Address, 
							 _In_ DWORD AddressLength, 
							 _In_ PCSTR Dns, 
							 _In_ USHORT QueryType,
							 _In_ USHORT Xid,
							 _In_ bool RecursionDesired)
{
	if (CDataPacket* packet = new(DNS_RFC_MAX_UDP_PACKET_LENGTH + sizeof(DCD)) CDataPacket)
	{
		packet->setDataSize(sizeof(DCD));
		DCD* p = (DCD*)packet->getData();
		p->_Xid = Xid;
		p->_QueryType = QueryType;

		InterlockedIncrementNoFence(&_waitCount);

		if (RecvFrom(packet) || !SendToServer(Address, AddressLength, Dns, QueryType, Xid, RecursionDesired))
		{
			DecWaitCount();
		}

		packet->Release();
	}
}

BOOL CDnsSocket::SendToServer(_In_ PSOCKADDR Address, 
							  _In_ DWORD AddressLength, 
							  _In_ PCSTR Dns, 
							  _In_ USHORT QueryType,
							  _In_ USHORT Xid,
							  _In_ bool RecursionDesired)
{
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
		pdh->Xid = Xid;
		pdh->RecursionDesired = RecursionDesired;
		pdh++->QuestionCount = 0x0100; //_byteswap_ushort(1);

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

		DNS_WIRE_QUESTION dwq = { QueryType, DNS_RCLASS_INTERNET };
		memcpy(lpsz, &dwq, sizeof(dwq));

		packet->setDataSize(RtlPointerToOffset(__lpsz, lpsz) + sizeof(dwq));

		ULONG err = SendTo(Address, AddressLength, packet);

		packet->Release();

		return err == NOERROR;
	}

	return FALSE;
}

void CDnsSocket::OnRecv(PSTR Buffer, ULONG cbTransferred, USHORT Xid, USHORT QueryType)
{
	if (cbTransferred < sizeof(DNS_HEADER) || 
		reinterpret_cast<DNS_HEADER*>(Buffer)->Xid != Xid ||
		reinterpret_cast<DNS_HEADER*>(Buffer)->ResponseCode != DNS_RCODE_NOERROR) return ;

	ULONG AnswerCount = _byteswap_ushort(reinterpret_cast<DNS_HEADER*>(Buffer)->AnswerCount);

	if (!AnswerCount) return ;

	Buffer += sizeof(DNS_HEADER), cbTransferred -= sizeof(DNS_HEADER);

	UCHAR c;

	while (cbTransferred-- && (c = *Buffer++))
	{
		if (cbTransferred < c) return;
		cbTransferred -= c, Buffer += c;
	}

	if (cbTransferred < sizeof(DNS_WIRE_QUESTION)) return ;
	Buffer += sizeof(DNS_WIRE_QUESTION), cbTransferred -= sizeof(DNS_WIRE_QUESTION);

	do 
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
	
		SOCKADDR_INET si;

		if (QueryType == dwr.RecordType) switch (dwr.RecordType)
		{
		case DNS_RTYPE_A:
			if (dwr.DataLength == sizeof(IP4_ADDRESS))
			{
				RtlZeroMemory(&si.Ipv4, sizeof(si.Ipv4));
				si.Ipv4.sin_family = AF_INET;
				memcpy(&si.Ipv4.sin_addr, Buffer, sizeof(IP4_ADDRESS));

				if (si.Ipv4.sin_addr.S_un.S_addr)
				{
					DnsCache::set(_crc, si.Ipv4.sin_addr.S_un.S_addr);
					OnIp((PSOCKADDR)&si.Ipv4, sizeof(si.Ipv4));
					return;
				}
			}
			break;
		case DNS_RTYPE_AAAA:
			if (dwr.DataLength == sizeof(IP6_ADDRESS))
			{
				RtlZeroMemory(&si.Ipv6, sizeof(si.Ipv6));
				si.Ipv6.sin6_family = AF_INET6;
				memcpy(&si.Ipv6.sin6_addr, Buffer, sizeof(IP6_ADDRESS));
				OnIp((PSOCKADDR)&si.Ipv6, sizeof(si.Ipv6));
				return;
			}
			break;
		}

		cbTransferred -= dwr.DataLength, Buffer += dwr.DataLength;

	} while(--AnswerCount);
}

void CDnsSocket::OnRecv(PSTR Buffer, ULONG cbTransferred, CDataPacket* packet, SOCKADDR_IN_EX* addr)
{
	DCD* p = (DCD*)packet->getData();

	DbgPrint("%s<%p>[%08x]->%u,%p\n", __FUNCTION__, this, addr->addr.Ipv4.sin_addr.s_addr, cbTransferred, Buffer);
	if (Buffer) OnRecv(Buffer, cbTransferred, p->_Xid, p->_QueryType);
	DecWaitCount();
}

//////////////////////////////////////////////////////////////////////////

void CSocketObject::DnsToIp(_In_ PCSTR Dns, _In_ USHORT QueryType/* = DNS_RTYPE_A*/, _In_ LONG QueryOptions /*= DNS_QUERY_STANDARD*/)
{
	if (strlen(Dns) > DNS_MAX_TEXT_STRING_LENGTH)
	{
		OnIp(0, 0);
		return;
	}

	ULONG crc = HashString(Dns);
	PCSTR c;

	SOCKADDR_INET si{};

	switch (QueryType)
	{
	case DNS_RTYPE_A:
		if (0 <= RtlIpv4StringToAddressA(Dns, TRUE, &c, &si.Ipv4.sin_addr) || 
			(si.Ipv4.sin_addr.S_un.S_addr = DnsCache::get(crc)))
		{
			si.Ipv4.sin_family = AF_INET;
			OnIp((PSOCKADDR)&si.Ipv4, sizeof(si.Ipv4));
			return;
		}
		break;
	case DNS_RTYPE_AAAA:
		if (0 <= RtlIpv6StringToAddressA(Dns, &c, &si.Ipv6.sin6_addr))
		{
			si.Ipv6.sin6_family = AF_INET6;
			OnIp((PSOCKADDR)&si.Ipv6, sizeof(si.Ipv6));
			return;
		}
		break;
	}

	if (CDnsSocket* pDns = new CDnsSocket(this))
	{
		pDns->DnsToIp(Dns, crc, QueryType, QueryOptions);
		pDns->Release();
	}
	else
	{
		OnIp(0, 0);
	}
}

_NT_END