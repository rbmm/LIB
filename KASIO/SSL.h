#pragma once

#include "tdi.h"

EXTERN_C
SECURITY_STATUS 
SEC_ENTRY
SealMessage(   _In_    PCtxtHandle         phContext,
			   _In_    unsigned long       fQOP,
			   _In_    PSecBufferDesc      pMessage,
			   _In_    unsigned long       MessageSeqNo);


EXTERN_C
SECURITY_STATUS 
SEC_ENTRY
UnsealMessage( _In_      PCtxtHandle         phContext,
			   _In_      PSecBufferDesc      pMessage,
			   _In_      unsigned long       MessageSeqNo,
			   _Out_opt_ unsigned long *     pfQOP);

class SharedCred
{
public:
	CredHandle m_hCred;
private:
	LONG m_dwRef;

	~SharedCred()
	{
		if (m_hCred.dwLower | m_hCred.dwUpper) FreeCredentialsHandle(&m_hCred);
	}

public:

	void AddRef()
	{
		InterlockedIncrementNoFence(&m_dwRef);
	}

	void Release()
	{
		if (!InterlockedDecrement(&m_dwRef))
		{
			delete this;
		}
	}

	SharedCred()
	{
		m_hCred.dwLower = 0;
		m_hCred.dwUpper = 0;
		m_dwRef = 1;
	}

	// SCHANNEL_CRED::dwFlags
	// SCHANNEL_CRED::grbitEnabledProtocols
	SECURITY_STATUS Acquire(ULONG fCredentialUse, PCCERT_CONTEXT pCertContext = 0, ULONG dwFlags = 0, ULONG grbitEnabledProtocols = 0);
};

class __declspec(novtable) CSSLStream : CtxtHandle, SecPkgContext_StreamSizes
{
protected:
	SharedCred* m_pCred;
	PWSTR m_pszTargetName;
private:
	ULONG m_cbSavedData;
	LONG m_flags;

	enum{
		f_Handshake
	};

	SECURITY_STATUS ProcessSecurityContext(PSecBufferDesc pInput, PSecBufferDesc pOutput);

	SECURITY_STATUS ProcessSecurityContext(PSTR& rbuf, ULONG& rcb);

	virtual void OnEncryptDecryptError(SECURITY_STATUS );

	virtual PCCERT_CONTEXT GetUserCert();

	virtual SECURITY_STATUS OnRequestUserCert();

	virtual BOOL OnRenegotiate();

	virtual BOOL OnEndHandshake() = 0;

	virtual void OnShutdown() = 0;

	virtual BOOL OnUserData(PSTR , ULONG ) = 0;

	virtual NTSTATUS SendData(CDataPacket* ) = 0;

	virtual CDataPacket* get_packet() = 0;

public:

	virtual BOOL IsServer(PBOOLEAN pbMutualAuth = 0) = 0;

	CDataPacket* AllocPacket(ULONG cbBody, PSTR& buf);

	PSTR GetUserData(CDataPacket* packet)
	{
		return packet->getData() + cbHeader;
	}

	NTSTATUS SendUserData(ULONG cbBody, CDataPacket* packet);

	NTSTATUS SendUserData(const void* buf, ULONG cb);

	ULONG getHeaderSize()
	{
		return cbHeader;
	}

	ULONG getMaximumMessage()
	{
		return cbMaximumMessage;
	}

	ULONG getTrailerSize()
	{
		return cbTrailer;
	}

	SECURITY_STATUS QueryContext(ULONG ulAttribute, PVOID pBuffer)
	{
		return QueryContextAttributesW(this, ulAttribute, pBuffer);
	}

protected:

	CSSLStream(SharedCred* pCred);

	~CSSLStream();

	BOOL StartSSL();

	void FreeCredentials();

	void StopSSL();

	BOOL OnData(PSTR Buffer, ULONG cbTransferred);

	SECURITY_STATUS Shutdown();

	SECURITY_STATUS Renegotiate();
};

class __declspec(novtable) CSSLEndpoint : public CSSLStream, public CTcpEndpoint
{
protected:

	CSSLEndpoint(SharedCred* pCred, CTdiAddress* pAddress) : CSSLStream(pCred), CTcpEndpoint(pAddress)
	{
	}

	virtual NTSTATUS SendData(CDataPacket* packet)
	{
		return Send(packet);
	}

	virtual CDataPacket* get_packet()
	{
		return m_packet;
	}

	virtual void OnRenegotiateError()
	{
		Disconnect();
	}

	virtual void OnShutdown()
	{
	}

	virtual BOOL OnConnect(NTSTATUS status)
	{
		return 0 > status ? FALSE : StartSSL();
	}

	virtual void OnDisconnect()
	{
		StopSSL();
	}

	virtual BOOL OnRecv(PSTR Buffer, ULONG cbTransferred)
	{
		return OnData(Buffer, cbTransferred);
	}

	/************************************************************************/
	/* implement this !

	virtual PCCERT_CONTEXT GetUserCert();

	virtual BOOL OnEndHandshake();

	virtual BOOL OnUserData(PSTR buf, ULONG cb);

	virtual BOOL IsServer(PBOOLEAN pbMutualAuth = 0);

	*/
	/************************************************************************/
};
