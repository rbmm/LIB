#include "StdAfx.h"

_NT_BEGIN
#include <mstcpip.h>
#include <iphlpapi.h>
#include <WinDNS.h>
#include "socket.h"
#define DbgPrint /##/

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
	//LONGLONG _time;
	CDnsTask* _pTask;
	ULONG _crc;
	ULONG _DnsServerIp;
	USHORT _Xid;

	virtual void OnRecv(PSTR Buffer, ULONG cbTransferred);
	virtual void OnRecv(PSTR Buffer, ULONG cbTransferred, CDataPacket* /*packet*/, SOCKADDR_IN* addr);

	~CDnsSocket()
	{
		DbgPrint("%s<%p>\n", __FUNCTION__, this);
		_pTask->Release();
	}

public:

	BOOL Start(PCSTR Dns, DWORD DnsServerIp, ULONG crc);

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

LONGLONG GetBootTime()
{
	static LONGLONG BootTime;
	if (!BootTime)
	{
		SYSTEM_TIMEOFDAY_INFORMATION sti;
		if (0 <= NtQuerySystemInformation(SystemTimeOfDayInformation, &sti, sizeof(sti), 0))
		{
			BootTime = sti.BootTime.QuadPart;
		}
		else
		{
			BootTime = 1;
		}
	}
	return BootTime;
}

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

		LONGLONG BootTime = GetBootTime();
		//TIME_FIELDS tf;
		//LARGE_INTEGER li;
		//GetSystemTimeAsFileTime((LPFILETIME)&li);
		//li.QuadPart -= BootTime;
		//RtlTimeToTimeFields(&li, &tf);
		//DbgPrint("now %u-%02u-%02u %02u:%02u:%02u after boot\r\n", 
		//	tf.Year-1601, tf.Month-1, tf.Day-1, tf.Hour, tf.Minute, tf.Second);

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

					if (pkni->LastWriteTime.QuadPart < BootTime)
					{
						DbgPrint("!! modified before boot !!\r\n");
						goto __nextIndex;
					}
					//else
					//{
					//	pkni->LastWriteTime.QuadPart -= BootTime;
					//	RtlTimeToTimeFields(&pkni->LastWriteTime, &tf);
					//	DbgPrint("modified %u-%02u-%02u %02u:%02u:%02u after boot\r\n", 
					//		tf.Year-1601, tf.Month-1, tf.Day-1, tf.Hour, tf.Minute, tf.Second);
					//}

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

__nextIndex:
			Index++;

		} while (status != STATUS_NO_MORE_ENTRIES);
__exit:
		NtClose(oa.RootDirectory);
	}

	return m;
}

void CDnsTask::DnsToIp(PCSTR Dns, ULONG crc, DWORD dwMilliseconds)
{
	static ULONG seed;

	if (!seed)
	{
		seed = ~GetTickCount();
	}

	ULONG i = RtlRandomEx(&seed);

	static ULONG DnsServerAddressesGroup1[] = {
		IP(8, 8, 8, 8),
		IP(8, 8, 4, 4),
	};

	static ULONG DnsServerAddressesGroup2[] = {
		IP(208, 67, 222, 222),
		IP(208, 67, 222, 220),
		IP(208, 67, 220, 220),
		IP(208, 67, 220, 222),
	};

	ULONG DnsServerAddresses[16] = { 
		DnsServerAddressesGroup1[i % RTL_NUMBER_OF(DnsServerAddressesGroup1)],
		DnsServerAddressesGroup2[i % RTL_NUMBER_OF(DnsServerAddressesGroup2)],
	};

	if (ULONG n = Create(2 + FillDnsServerList(RTL_NUMBER_OF(DnsServerAddresses) - 2, DnsServerAddresses + 2)))
	{
		if (SetTimeout(dwMilliseconds))
		{
			_nRecvCount = n + 1;
			CDnsSocket** ppSocks = (CDnsSocket**)alloca(n * sizeof(CDnsSocket*)), *pSock;
			memcpy(ppSocks, _ppSocks, n * sizeof(CDnsSocket*));

			i = n;
			do 
			{
				(*ppSocks++)->AddRef();
			} while (--i);

			do 
			{
				--n, pSock = *--ppSocks;

				BOOL fOk = FALSE;

				if (CDataPacket* packet = new(DNS_RFC_MAX_UDP_PACKET_LENGTH) CDataPacket)
				{
					DbgPrint("-->%08x\n", DnsServerAddresses[n]);

					if (!pSock->RecvFrom(packet) && pSock->Start(Dns, DnsServerAddresses[n], crc))
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

			} while (n);

			DecRecvCount();
		}
		else
		{
			Cleanup();
		}
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

BOOL CDnsSocket::Start(PCSTR Dns, DWORD DnsServerIp, ULONG crc)
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
		pdh->Xid = _Xid = (USHORT)RtlRandomEx(&seed);
		pdh->RecursionDesired = 1;
		pdh++->QuestionCount = 0x0100; //_byteswap_ushort(1);

		_DnsServerIp = DnsServerIp;

		// { _byteswap_ushort(DNS_TYPE_A), _byteswap_ushort(DNS_CLASS_INTERNET) }
		static const DNS_WIRE_QUESTION dwq = { 0x0100, 0x0100 };

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

		//GetSystemTimeAsFileTime((PFILETIME)&_time);

		ULONG err = SendTo(DnsServerIp, DNS_PORT_NET_ORDER, packet);

		packet->Release();

		return err == NOERROR;
	}

	return FALSE;
}

void CDnsSocket::OnRecv(PSTR Buffer, ULONG cbTransferred)
{
	if (cbTransferred < sizeof(DNS_HEADER) || reinterpret_cast<DNS_HEADER*>(Buffer)->Xid != _Xid) return ;

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

		DbgPrint("[%08x]{ %04x %04x : %04x }\n", _DnsServerIp, dwr.RecordType, dwr.RecordClass, dwr.DataLength);
		
		if (dwr.RecordType == 0x100/*DNS_TYPE_A*/ && dwr.DataLength == sizeof(IP4_ADDRESS))
		{
			IP4_ADDRESS ip;
			memcpy(&ip, Buffer, sizeof(IP4_ADDRESS));

			if (ip)
			{
				//LONGLONG time;
				//GetSystemTimeAsFileTime((PFILETIME)&time);, time - _time [%016Iu]
				DbgPrint("[%08x]->%08x\n", _DnsServerIp, ip);
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