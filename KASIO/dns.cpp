#include "StdAfx.h"

_NT_BEGIN

#include "tdi.h"
//#define DbgPrint /##/

class CDnsSocket;

class CDnsTask : public IO_OBJECT_TIMEOUT
{
	CTdiObject* _pEndp;
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

	CDnsTask(CTdiObject* pEndp) : _ppSocks(0), _n(0), _bFirstIp(TRUE), _pEndp(pEndp)
	{
		DbgPrint("%s<%p>\n", __FUNCTION__, this);
		_pEndp->AddRef();
	}

	void DnsToIp(PCSTR Dns, DWORD dwMilliseconds = 3000);
};

class CDnsSocket : public CUdpEndpoint
{
	CDnsTask* _pTask;

	virtual void OnRecv(PSTR Buffer, ULONG cbTransferred);

	virtual void OnRecv(PSTR Buffer, ULONG cbTransferred, CDataPacket* , TA_IP_ADDRESS*  )
	{
		if (Buffer) OnRecv(Buffer, cbTransferred);
		_pTask->DecRecvCount();
	}

	~CDnsSocket()
	{
		DbgPrint("%s<%p>\n", __FUNCTION__, this);
		_pTask->Release();
	}

public:

	BOOL Start(PCSTR Dns, DWORD ip);

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

void CDnsTask::DnsToIp(PCSTR Dns, DWORD dwMilliseconds)
{
	static ULONG seed;

	if (!seed)
	{
		LARGE_INTEGER  TickCount;
		KeQueryTickCount(&TickCount);
		seed = ~TickCount.LowPart;
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

	ULONG DnsServerAddresses[2] = { 
		DnsServerAddressesGroup1[i % RTL_NUMBER_OF(DnsServerAddressesGroup1)],
		DnsServerAddressesGroup2[i % RTL_NUMBER_OF(DnsServerAddressesGroup2)],
	};

	if (ULONG n = Create( RTL_NUMBER_OF(DnsServerAddresses)))
	{
		if (SetTimeout(dwMilliseconds))
		{
			_nRecvCount = n;
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

				if (CDataPacket* packet = new(1024) CDataPacket)
				{
					if (0 <= pSock->RecvFrom(packet) && pSock->Start(Dns, DnsServerAddresses[n]))
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

	if (CDnsSocket** ppSocks = new(PagedPool) CDnsSocket*[n])
	{
		CDnsSocket** ppSocks2 = ppSocks;
		ULONG m = 0;

		do 
		{
			if (CDnsSocket* pSocks = new CDnsSocket(this))
			{
				if (0 > pSocks->Create(0))
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

//////////////////////////////////////////////////////////////////////////
void CDnsSocket::OnRecv(PSTR Buffer, ULONG cbTransferred)
{
	if (cbTransferred < 13) return ;

	Buffer += 12, cbTransferred -= 12;

	UCHAR c;

	while (c = *Buffer++)
	{
		if (cbTransferred < (DWORD)(2 + c)) return;
		cbTransferred -= 1 + c;
		Buffer += c;
	}

	Buffer += 4;
	if (cbTransferred < 4) return ;
	cbTransferred -= 4;

	struct DNS_RR
	{
		WORD name, type, cls, ttl1, ttl2, len;
	} x;

	for(;;) 
	{
		if (cbTransferred < sizeof(DNS_RR)) return;
		memcpy(&x, Buffer, sizeof(x));
		cbTransferred -= sizeof (DNS_RR), Buffer += sizeof (DNS_RR);
		x.len = _byteswap_ushort(x.len);
		if (cbTransferred < x.len) return;
		cbTransferred -= x.len;
		if (x.type == 0x100 && x.cls == 0x100 && x.len == sizeof(DWORD))
		{
			ULONG ip;
			memcpy(&ip, Buffer, sizeof(DWORD));

			_pTask->OnIp(ip);
			return;
		}
		Buffer += x.len;
	}
}

BOOL CDnsSocket::Start(PCSTR Dns, DWORD ip)
{
	if (strlen(Dns) > 256) return FALSE;

	if (CDataPacket* packet = new(1024) CDataPacket)
	{
		PSTR __lpsz = packet->getData(), _lpsz, lpsz = __lpsz;
		char c, i;
		static WORD bb1[6]={ 0x3333, 1, 0x0100 };
		static WORD bb2[2]={ 0x0100, 0x0100 };
		memcpy(lpsz, bb1, sizeof bb1);
		lpsz += sizeof bb1;

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

		memcpy(lpsz, bb2, sizeof bb2);

		packet->setDataSize(RtlPointerToOffset(__lpsz, lpsz) + sizeof(bb2));

		NTSTATUS status = SendTo(ip, 0x3500, packet);

		packet->Release();

		return 0 <= status;
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////

void CTdiObject::DnsToIp(PCSTR Dns)
{
	DWORD ip;
	PSTR c;
	if (0 <= RtlIpv4StringToAddressA(Dns, TRUE, c, ip))
	{
		OnIp(ip);
		return;
	}

	if (CDnsTask* pTask = new CDnsTask(this))
	{
		pTask->DnsToIp(Dns);
		pTask->Release();
	}
	else
	{
		OnIp(0);
	}
}

_NT_END
