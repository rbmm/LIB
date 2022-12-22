#pragma once

#include "socket.h"

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
	friend class CSSLEndpointEx;

protected:
	SharedCred* m_pCred;
	PWSTR m_pszTargetName;
private:
	ULONG m_cbSavedData;
	LONG m_flags;

	enum {
		f_Handshake
	};

	SECURITY_STATUS ProcessSecurityContext(PSecBufferDesc pInput, PSecBufferDesc pOutput);

	SECURITY_STATUS ProcessSecurityContext(PSTR& rbuf, DWORD& rcb);
	
	virtual void OnEncryptDecryptError(SECURITY_STATUS );

	virtual PCCERT_CONTEXT GetUserCert();

	virtual SECURITY_STATUS OnRequestUserCert();

	virtual SECURITY_STATUS OnEndHandshake() = 0;

	virtual void OnShutdown() = 0;

	virtual BOOL OnUserData(PSTR , ULONG ) = 0;

	virtual ULONG SendData(CDataPacket* ) = 0;

	virtual CDataPacket* get_packet() = 0;

	virtual CDataPacket* allocPacket(ULONG cb) { return new(cb) CDataPacket; };

public:

	virtual BOOL IsServer(PBOOLEAN pbMutualAuth = 0) = 0;

	CDataPacket* AllocPacket(DWORD cbBody, PSTR& buf);

	PSTR GetUserData(CDataPacket* packet)
	{
		return packet->getData() + cbHeader;
	}

	ULONG SendUserData(DWORD cbBody, CDataPacket* packet);

	ULONG SendUserData(const void* buf, DWORD cb);

	DWORD getHeaderSize()
	{
		return cbHeader;
	}

	DWORD getMaximumMessage()
	{
		return cbMaximumMessage;
	}

	DWORD getTrailerSize()
	{
		return cbTrailer;
	}

	SECURITY_STATUS QueryContext(ULONG ulAttribute, PVOID pBuffer)
	{
		return QueryContextAttributesW(this, ulAttribute, pBuffer);
	}

	BOOL StartSSL(PSTR buf = 0, DWORD cb = 0);

	void StopSSL();

	SECURITY_STATUS Shutdown();

	SECURITY_STATUS Renegotiate();

protected:

	CSSLStream(SharedCred* pCred);

	~CSSLStream();

	void FreeCredentials();

	BOOL OnData(PSTR Buffer, ULONG cbTransferred);
	
	BOOL OnData()
	{
		return OnData(get_packet()->getData() + m_cbSavedData, 0);
	}
};

class __declspec(novtable) CSSLEndpoint : public CSSLStream, public CTcpEndpoint
{
protected:

	CSSLEndpoint(SharedCred* pCred, CSocketObject* pAddress = 0) : CSSLStream(pCred), CTcpEndpoint(pAddress)
	{
	}

	virtual ULONG SendData(CDataPacket* packet)
	{
		return Send(packet);
	}

	virtual CDataPacket* get_packet()
	{
		return m_packet;
	}

	virtual void OnShutdown()
	{
	}

	virtual BOOL OnConnect(ULONG status)
	{
		return status ? FALSE : StartSSL();
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

	virtual SECURITY_STATUS OnEndHandshake();

	virtual BOOL OnUserData(PSTR buf, ULONG cb);

	virtual BOOL IsServer(PBOOLEAN pbMutualAuth = 0);

	*/
	/************************************************************************/

};

class __declspec(novtable) CSSLEndpointEx : public CSSLStream, public CTcpEndpoint
{
protected:

	CSSLEndpointEx(SharedCred* pCred = 0, CSocketObject* pAddress = 0) : CSSLStream(pCred), CTcpEndpoint(pAddress)
	{
	}

	virtual ULONG SendData(CDataPacket* packet)
	{
		return Send(packet);
	}

	virtual CDataPacket* get_packet()
	{
		return m_packet;
	}

	virtual void OnShutdown()
	{
	}

	virtual void OnServerConnect(ULONG /*status*/)
	{
	}

	virtual void OnServerDisconnect()
	{
	}

	virtual BOOL OnConnect(ULONG status)
	{
		OnServerConnect(status);
		return status ? FALSE : (m_pCred ? StartSSL() : SEC_E_OK == OnEndHandshake());
	}

	virtual void OnDisconnect()
	{
		StopSSL();
		OnServerDisconnect();
	}

	virtual BOOL OnRecv(PSTR Buffer, ULONG cbTransferred)
	{
		return m_pCred ? OnData(Buffer, cbTransferred) : OnUserData(Buffer, cbTransferred);
	}

	/************************************************************************/
	/* implement this !

	virtual PCCERT_CONTEXT GetUserCert();

	virtual SECURITY_STATUS OnEndHandshake();

	virtual BOOL OnUserData(PSTR buf, ULONG cb);

	virtual BOOL IsServer(PBOOLEAN pbMutualAuth = 0);

	*/
	/************************************************************************/
};

PCCERT_CONTEXT CryptUIDlgGetUserCert();