#include "StdAfx.h"

_NT_BEGIN
#include <Cryptuiapi.h>
#include "SSL.h"
//#define __DBG__
//#define __DBG_EX__

#ifdef __DBG__
__pragma(message(" !! __DBG__ !! "))
#define __DBG
#else
#define __DBG /##/
#define DbgPrint /##/
#define Dump /##/
#define PrintStr /##/
#define DumpBytes /##/
#endif

SECURITY_STATUS SharedCred::Acquire(ULONG fCredentialUse, PCCERT_CONTEXT pCertContext, ULONG dwFlags, ULONG grbitEnabledProtocols)
{ 
	SCHANNEL_CRED sc { SCHANNEL_CRED_VERSION };

	if (pCertContext)
	{
		sc.paCred = &pCertContext;
		sc.cCreds = 1;
	}

	sc.dwFlags = dwFlags;
	sc.grbitEnabledProtocols = grbitEnabledProtocols;

	SECURITY_STATUS ss = AcquireCredentialsHandleW(0, const_cast<PWSTR>(SCHANNEL_NAME), fCredentialUse, 0, &sc, 0, 0, &m_hCred, 0);

	if (0 > ss) 
	{
		m_hCred.dwLower = 0, m_hCred.dwUpper = 0;
		DbgPrint("\r\n%p>%s(%p)=%x\r\n", this, __FUNCTION__, pCertContext, ss);
	}

	return ss;
}

void CSSLStream::OnEncryptDecryptError([[maybe_unused]] HRESULT hr)
{
	DbgPrint("\n\n%p>%s(%x) !!!!!!!!!!\n\n", this, __FUNCTION__, hr);
}

PCCERT_CONTEXT CryptUIDlgGetUserCert()
{
	typedef PCCERT_CONTEXT (WINAPI * CRYPTUIDLGSELECTCERTIFICATEFROMSTORE)(
		_In_     HCERTSTORE hCertStore,
		_In_opt_ HWND hwnd,              // Defaults to the desktop window
		_In_opt_ LPCWSTR pwszTitle,
		_In_opt_ LPCWSTR pwszDisplayString,
		_In_     DWORD dwDontUseColumn,
		_In_     DWORD dwFlags,
		_In_     void *pvReserved
		);

	static CRYPTUIDLGSELECTCERTIFICATEFROMSTORE CryptUIDlgSelectCertificateFromStore;

	if (!CryptUIDlgSelectCertificateFromStore)
	{
		if (HMODULE hmod = LoadLibraryW(L"Cryptui"))
		{
			if (PVOID pv = GetProcAddress(hmod, "CryptUIDlgSelectCertificateFromStore"))
			{
				InterlockedCompareExchangePointerNoFence((void**)&CryptUIDlgSelectCertificateFromStore, pv, 0);
			}
		}

		if (!CryptUIDlgSelectCertificateFromStore)
		{
			return 0;
		}
	}

	if (HCERTSTORE hCertStore = CertOpenStore(CERT_STORE_PROV_SYSTEM, X509_ASN_ENCODING,
		0, CERT_STORE_OPEN_EXISTING_FLAG|CERT_SYSTEM_STORE_CURRENT_USER, L"MY"))
	{
		PCCERT_CONTEXT pCertContext = CryptUIDlgSelectCertificateFromStore(hCertStore, 0, 0, 0, 0, 0, 0);

		CertCloseStore(hCertStore, 0);

		return pCertContext;
	}
	return 0;
}

PCCERT_CONTEXT CSSLStream::GetUserCert()
{
	return 0;
}

SECURITY_STATUS CSSLStream::OnRequestUserCert()
{
	DbgPrint("\r\n%p>OnRequestUserCert()\r\n", this);

	SECURITY_STATUS ss = SEC_E_INTERNAL_ERROR;

	if (PCCERT_CONTEXT pCertContext = GetUserCert())
	{
		if (SharedCred* pCred = new SharedCred)
		{
			ss = pCred->Acquire(SECPKG_CRED_OUTBOUND, pCertContext, 
				m_pszTargetName ? 0 : SCH_CRED_MANUAL_CRED_VALIDATION|SCH_CRED_NO_DEFAULT_CREDS);

			if (ss == SEC_E_OK)
			{
				if (m_pCred)
				{
					m_pCred->Release();
				}

				m_pCred = pCred;
			}
		}
		CertFreeCertificateContext(pCertContext);
	}

	return ss;
}

CSSLStream::CSSLStream(SharedCred* pCred)
{
	m_pCred = pCred;
	dwLower = 0;
	dwUpper = 0;
	m_flags = 0;
	m_pszTargetName = 0;
	RtlZeroMemory(static_cast<SecPkgContext_StreamSizes*>(this), sizeof(SecPkgContext_StreamSizes));

	if (pCred)
	{
		pCred->AddRef();
	}
	else
	{
		cbMaximumMessage = MAXULONG, cbBlockSize = MAXULONG;
	}
}

CSSLStream::~CSSLStream()
{
	StopSSL();
	if (m_pCred) m_pCred->Release();
}

void CSSLStream::StopSSL()
{
	if (dwLower | dwUpper) 
	{
		::DeleteSecurityContext(this);
		dwLower = 0;
		dwUpper = 0;
	}
	RtlZeroMemory(static_cast<SecPkgContext_StreamSizes*>(this), sizeof(SecPkgContext_StreamSizes));
}

BOOL CSSLStream::StartSSL(PSTR buf, DWORD cb)
{
	m_cbSavedData = 0;

	m_flags = 1 << f_Handshake;

	return IsServer() && !cb ? TRUE : ProcessSecurityContext(buf, cb) == SEC_I_CONTINUE_NEEDED;
}

#ifdef __DBG__

#ifdef __DBG_EX__

void PrintStr(PSTR psz, ULONG cch)
{
	ULONG len;
	do 
	{
		DbgPrint("%.*s", len = min(0x100, cch), psz);
	} while (psz += len, cch -= len);
}

void DumpBytes(const UCHAR* pb, ULONG cb)
{
	PSTR psz = 0;
	ULONG cch = 0;
	while (CryptBinaryToStringA(pb, cb, CRYPT_STRING_HEXASCIIADDR, psz, &cch))
	{
		if (psz)
		{
			PrintStr( psz, cch);
			break;
		}

		if (!(psz = new CHAR[cch]))
		{
			break;
		}
	}
	
	if (psz)
	{
		delete [] psz;
	}
}
#endif

#define CASE(t) case SECBUFFER_##t: return #t

PCSTR GetBufTypeName(ULONG BufferType, PSTR buf)
{
	switch (BufferType & ~0xF0000000)
	{
		CASE(EMPTY);
		CASE(DATA);
		CASE(TOKEN);
		CASE(PKG_PARAMS);
		CASE(MISSING);
		CASE(EXTRA);
		CASE(STREAM_TRAILER);
		CASE(STREAM_HEADER);
		CASE(NEGOTIATION_INFO);
		CASE(PADDING);
		CASE(STREAM);
		CASE(MECHLIST);
		CASE(MECHLIST_SIGNATURE);
		CASE(TARGET);
		CASE(CHANNEL_BINDINGS);
		CASE(CHANGE_PASS_RESPONSE);
		CASE(TARGET_HOST);
		CASE(ALERT);
		CASE(APPLICATION_PROTOCOLS);
		CASE(SRTP_PROTECTION_PROFILES);
		CASE(SRTP_MASTER_KEY_IDENTIFIER);
		CASE(TOKEN_BINDING);
		CASE(PRESHARED_KEY);
		CASE(PRESHARED_KEY_IDENTITY);
		CASE(DTLS_MTU);
		CASE(SEND_GENERIC_TLS_EXTENSION);
		CASE(SUBSCRIBE_GENERIC_TLS_EXTENSION);
		CASE(FLAGS);
		CASE(TRAFFIC_SECRETS);
	}

	sprintf_s(buf, 16, "%x", BufferType);
	return buf;
}

void Dump(PSecBufferDesc psbd, PCSTR msg, ULONG id, CHAR c)
{
	if (ULONG cBuffers = psbd->cBuffers)
	{
		PSecBuffer pBuffers = psbd->pBuffers;
		DbgPrint("PSecBufferDesc<%c%u>(v=%x, n=%x) : %s\r\n", c, id, psbd->ulVersion, cBuffers, msg);

		do 
		{
			char buf[16];
			DbgPrint("BufferType = %s, cb = %x, pv = %p\r\n", 
				GetBufTypeName(pBuffers->BufferType, buf), pBuffers->cbBuffer, pBuffers->pvBuffer);

#ifdef __DBG_EX__

			if (pBuffers->BufferType != SECBUFFER_EMPTY)
			{
				union {
					PVOID pv;
					PUCHAR pb;
				};

				if (pv = pBuffers->pvBuffer)
				{
					ULONG cb = pBuffers->cbBuffer;
					DumpBytes(pb, cb);
				}
			}
#endif

		} while (pBuffers++, --cBuffers);
	}
}
#endif // __DBG__

#define ASC_REQ ASC_REQ_REPLAY_DETECT|ASC_REQ_SEQUENCE_DETECT|ASC_REQ_CONFIDENTIALITY|ASC_REQ_ALLOCATE_MEMORY|ASC_REQ_EXTENDED_ERROR|ASC_REQ_STREAM
#define ISC_REQ ISC_REQ_REPLAY_DETECT|ISC_REQ_SEQUENCE_DETECT|ISC_REQ_CONFIDENTIALITY|ISC_REQ_ALLOCATE_MEMORY|ISC_REQ_EXTENDED_ERROR|ISC_REQ_STREAM//|ISC_REQ_USE_SUPPLIED_CREDS//|ISC_REQ_MANUAL_CRED_VALIDATION

SECURITY_STATUS CSSLStream::ProcessSecurityContext(PSecBufferDesc pInput, PSecBufferDesc pOutput)
{
	//DbgPrint("%p>%c:ProcessSecurityContext<%p.%p> %S\n", this, IsServer() ? 'S' : 'C', dwLower, dwUpper, m_pszTargetName);
	DWORD ContextAttr;

	PCtxtHandle phContext = 0, phNewContext = 0;

	dwLower | dwUpper ? phContext = this : phNewContext = this;

	BOOLEAN bMutualAuth = FALSE;

	return IsServer(&bMutualAuth) 
		?
		::AcceptSecurityContext(&m_pCred->m_hCred, phContext, pInput,
		bMutualAuth ? ASC_REQ_MUTUAL_AUTH|ASC_REQ : ASC_REQ, 0, phNewContext, pOutput, &ContextAttr, 0)
		:
		::InitializeSecurityContextW(&m_pCred->m_hCred, phContext, m_pszTargetName, ISC_REQ, 0, 0, pInput, 0, phNewContext, pOutput, &ContextAttr, 0);
}

SECURITY_STATUS CSSLStream::ProcessSecurityContext(PSTR& rbuf, DWORD& rcb)
{
	PSTR buf = rbuf;
	DWORD cb = rcb;

	DbgPrint("%p>%c:ProcessSecurityContext(%p, %x)\n\n", this, IsServer() ? 'S' : 'C', buf, cb);

loop:

	SecBuffer InBuf[2] = {{ cb, SECBUFFER_TOKEN, buf }}, OutBuf = { 0, SECBUFFER_TOKEN }; 

	SecBufferDesc sbd_in = { SECBUFFER_VERSION, 2, InBuf }, sbd_out = { SECBUFFER_VERSION, 1, &OutBuf };

	SECURITY_STATUS ss = ProcessSecurityContext(&sbd_in, &sbd_out);

	DbgPrint("%p>%c:ProcessSecurityContext(%x)=%x, out=%x\n\n", this, IsServer() ? 'S' : 'C', cb, ss, OutBuf.cbBuffer);
	__DBG static LONG id = 0;
	Dump(&sbd_in, "ProcessSecurityContext", InterlockedIncrementNoFence(&id), IsServer() ? 's' : 'c');

	switch (ss)
	{
	case SEC_E_INCOMPLETE_MESSAGE:
		return SEC_E_INCOMPLETE_MESSAGE;
	case SEC_I_INCOMPLETE_CREDENTIALS:
		if (SEC_E_OK == (ss = OnRequestUserCert()))
		{
			goto loop;
		}
		break;
	}

	if (InBuf[1].BufferType == SECBUFFER_EXTRA)
	{
		buf += cb - InBuf[1].cbBuffer;
		cb = InBuf[1].cbBuffer;
	}
	else
	{
		cb = 0;
	}

	rcb = cb, rbuf = buf;

	BOOL fOk = TRUE;

	if (OutBuf.cbBuffer)
	{
		fOk = FALSE;

		if (CDataPacket* packet = allocPacket(OutBuf.cbBuffer))
		{
			memcpy(packet->getData(), OutBuf.pvBuffer, OutBuf.cbBuffer);
			packet->setDataSize(OutBuf.cbBuffer);
			fOk = !SendData(packet);
			DbgPrint(">>>> SEND %X Bytes\n", OutBuf.cbBuffer);
			packet->Release();
		}
		FreeContextBuffer(OutBuf.pvBuffer);
	}

	if (!fOk)
	{
		ss = SEC_E_INTERNAL_ERROR;
	}

	switch (ss)
	{
	case SEC_E_OK:
		if (_bittestandreset(&m_flags, f_Handshake))
		{
			if (SEC_E_OK == (ss = ::QueryContextAttributes(this, SECPKG_ATTR_STREAM_SIZES, static_cast<SecPkgContext_StreamSizes*>(this))))
			{
				DbgPrint("\n\nStreamSizes( %x-%x-%x %xx%x)\n\n", cbHeader, cbMaximumMessage, cbTrailer, cBuffers, cbBlockSize);
				ss = OnEndHandshake();
			}
		}
	case SEC_I_CONTINUE_NEEDED:
		break;
	}

	if (0 > ss)
	{
		OnEncryptDecryptError(ss);
	}

	return ss;
}

BOOL CSSLStream::OnData(PSTR buf, ULONG cb)
{
	CDataPacket* packet = get_packet();
	
	DbgPrint("%p>%c:OnRecv(%p, %X+%X) %x\n\n", this, IsServer() ? 'S' : 'C', buf, m_cbSavedData, cb, m_flags);
	
	if (m_cbSavedData)
	{
		buf -= m_cbSavedData;
		cb += m_cbSavedData;
		packet->decData(m_cbSavedData);
		m_cbSavedData = 0;
	}

	BOOL fOk = TRUE;

	if (_bittest(&m_flags, f_Handshake))
	{
		switch(ProcessSecurityContext(buf, cb))
		{
		case STATUS_PENDING: // can be returned by OnEndHandshake()
			fOk = -1;        // no recv
			[[fallthrough]];
		case SEC_E_INCOMPLETE_MESSAGE:
		case SEC_I_CONTINUE_NEEDED:
__save_and_exit:
			m_cbSavedData = cb;
			packet->addData(buf, cb);
			return fOk;
		case SEC_E_OK:
			break;
		default:return FALSE;
		}
	}

	if (!cb) return TRUE;

	DbgPrint("%p>%c:OnData(%p, %X) %x\n\n", this, IsServer() ? 'S' : 'C', buf, cb, m_flags);

	for (;;)
	{
		SecBuffer sb[4] = { { cb, SECBUFFER_DATA, buf } };

		SecBufferDesc sbd = { SECBUFFER_VERSION, 4, sb };

		SECURITY_STATUS ss = ::DecryptMessage(this, &sbd, 0, 0);

		DbgPrint("%p>%c:DecryptMessage(%x)=%x\n\n", this, IsServer() ? 'S' : 'C', cb, ss);
		__DBG static LONG id = 0;
		Dump(&sbd, "DecryptMessage", InterlockedIncrementNoFence(&id), IsServer() ? 's' : 'c');

		switch(ss)
		{
		case SEC_I_CONTEXT_EXPIRED:
			DbgPrint("\n\n%p>%c:Shutdown\n\n", this, IsServer() ? 'S' : 'C');
			OnShutdown();
			return FALSE;

		case SEC_I_RENEGOTIATE:
			DbgPrint("\n\n%p>%u:SEC_I_RENEGOTIATE\n\n", this, IsServer() ? 'S' : 'C');

			if (sb[0].BufferType == SECBUFFER_STREAM_HEADER &&
				sb[1].BufferType == SECBUFFER_DATA && !sb[1].cbBuffer &&
				sb[2].BufferType == SECBUFFER_STREAM_TRAILER)
			{
				switch (sb[3].BufferType)
				{
				case SECBUFFER_EMPTY:
					sb[3].cbBuffer = 0;
					[[fallthrough]];
				case SECBUFFER_EXTRA:
					switch ( ProcessSecurityContext(buf = (PSTR)sb[3].pvBuffer, cb = sb[3].cbBuffer))
					{
					case SEC_E_OK:
					case SEC_I_CONTINUE_NEEDED:
						if (cb)
						{
							continue;
						}
						return TRUE;
					}
				}
			}
			return FALSE;

		case SEC_E_INCOMPLETE_MESSAGE:
			goto __save_and_exit;

		case SEC_E_OK:
			if (
				sb[0].BufferType == SECBUFFER_STREAM_HEADER &&
				sb[1].BufferType == SECBUFFER_DATA && sb[1].cbBuffer &&
				sb[2].BufferType == SECBUFFER_STREAM_TRAILER
				)
			{
				if (!OnUserData((PSTR)sb[1].pvBuffer, sb[1].cbBuffer))
				{
					return FALSE;
				}
			}

			if (sb[3].BufferType == SECBUFFER_EXTRA)
			{
				buf = (PSTR)sb[3].pvBuffer;
				cb = sb[3].cbBuffer;
				continue;
			}

			return TRUE;

		default:
			OnEncryptDecryptError(ss);
			return FALSE;
		}
	}
}

CDataPacket* CSSLStream::AllocPacket(DWORD cbBody, PSTR& buf)
{
	if (cbBody <= cbMaximumMessage)
	{
		if (CDataPacket* packet = allocPacket(cbHeader + cbBody + cbTrailer))
		{
			buf = packet->getData() + cbHeader;
			return packet;
		}
	}
	return 0;
}

ULONG CSSLStream::SendUserData(DWORD cbBody, CDataPacket* packet)
{
	DbgPrint("\r\nCSSLStream::SendUserData(%x)\r\n", cbBody);

	if (m_pCred)
	{
		PSTR Buffer = packet->getData();

		SecBuffer sb[4] = {
			{ cbHeader, SECBUFFER_STREAM_HEADER, Buffer },
			{ cbBody, SECBUFFER_DATA, Buffer + cbHeader },
			{ cbTrailer, SECBUFFER_STREAM_TRAILER, Buffer + cbHeader + cbBody },
		};

		SecBufferDesc sbd = { 
			SECBUFFER_VERSION, 4, sb
		};

		if (ULONG err = ::EncryptMessage(this, 0, &sbd, 0))
		{
			return err;
		}

		packet->setDataSize(sb[0].cbBuffer + sb[1].cbBuffer + sb[2].cbBuffer + sb[3].cbBuffer);
	}
	else
	{
		packet->setDataSize(cbBody);
	}

	return SendData(packet);
}

ULONG CSSLStream::SendUserData(const void* buf, DWORD cb)
{
	DWORD s, cbMax = cbMaximumMessage;

	do 
	{
		s = min(cb, cbMax);

		ULONG err = (ULONG)SEC_E_INSUFFICIENT_MEMORY;

		PSTR _buf;

		if (CDataPacket* packet = AllocPacket(s, _buf))
		{
			memcpy(_buf, buf, s);
			err = SendUserData(s, packet);
			packet->Release();
		}

		if (err)
		{
			DbgPrint("\r\n======== SendData=%x\r\n", err);
			return err;
		}

		buf = RtlOffsetToPointer(buf , s);

	} while (cb -= s);

	return 0;
}

SECURITY_STATUS CSSLStream::Shutdown()
{
	ULONG dw = SCHANNEL_SHUTDOWN;
	SecBuffer sb = { sizeof(dw), SECBUFFER_TOKEN, &dw };
	SecBufferDesc sbd = { SECBUFFER_VERSION, 1, &sb };
	SECURITY_STATUS ss = ::ApplyControlToken(this, &sbd);

	if (ss != SEC_E_OK)
	{
		return ss;
	}

	SecBuffer OutBuf = { 0, SECBUFFER_TOKEN }; 

	SecBufferDesc sbd_out = { SECBUFFER_VERSION, 1, &OutBuf };

	ss = ProcessSecurityContext(0, &sbd_out);

	if (OutBuf.cbBuffer)
	{
		if (CDataPacket* packet = allocPacket(OutBuf.cbBuffer))
		{
			memcpy(packet->getData(), OutBuf.pvBuffer, OutBuf.cbBuffer);
			packet->setDataSize(OutBuf.cbBuffer);
			SendData(packet);
			DbgPrint("send %x bytes\n", OutBuf.cbBuffer);
			packet->Release();
		}
		FreeContextBuffer(OutBuf.pvBuffer);
	}

	return ss;
}

SECURITY_STATUS CSSLStream::Renegotiate()
{
	SecBuffer OutBuf = { 0, SECBUFFER_TOKEN }; 

	SecBufferDesc sbd_out = { SECBUFFER_VERSION, 1, &OutBuf };

	SECURITY_STATUS ss = ProcessSecurityContext(0, &sbd_out);

	if (OutBuf.cbBuffer)
	{
		if (CDataPacket* packet = allocPacket(OutBuf.cbBuffer))
		{
			memcpy(packet->getData(), OutBuf.pvBuffer, OutBuf.cbBuffer);
			packet->setDataSize(OutBuf.cbBuffer);
			SendData(packet);
			DbgPrint("send %x bytes\n", OutBuf.cbBuffer);
			packet->Release();
		}
		FreeContextBuffer(OutBuf.pvBuffer);
	}

	return ss;
}

_NT_END