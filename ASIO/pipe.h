#pragma once

#include "io.h"

enum {
	FLAG_PIPE_CLIENT_SYNCHRONOUS = 0x01,
	FLAG_PIPE_CLIENT_INHERIT = 0x02,
	FLAG_PIPE_SERVER_SYNCHRONOUS = 0x04,
	FLAG_PIPE_SERVER_INHERIT = 0x8,
	FLAG_PIPE_SERVER_MESSAGE_MODE = 0x10,
	FLAG_PIPE_CLIENT_MESSAGE_MODE = 0x18,
};
NTSTATUS CreatePipeAnonymousPair(
								 PHANDLE phServerPipe, 
								 PHANDLE phClientPipe, 
								 ULONG Flags = FLAG_PIPE_CLIENT_INHERIT|FLAG_PIPE_CLIENT_SYNCHRONOUS, 
								 DWORD nInBufferSize = 0);

class __declspec(novtable) CPipeEnd : public IO_OBJECT_TIMEOUT
{
protected:
	CDataPacket*				m_packet;
private:
	RundownProtection			m_connectionLock;
protected:
	LONG						m_flags;

	enum {
		flListenActive, flDisconectActive, flMaxFlag
	};

private:

	enum OC {
		op_listen = 'llll', op_read = 'rrrr', op_write = 'wwww', op_disconnect = 'dddd', op_transact = 'tttt'
	};

	virtual VOID IOCompletionRoutine(CDataPacket* packet, DWORD Code, NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PVOID Pointer);

	_NODISCARD BOOL LockConnection()
	{
		return m_connectionLock.Acquire();
	}

	void UnlockConnection()
	{
		if (m_connectionLock.Release())
		{
			InterlockedBitTestAndResetNoFence(&m_flags, flListenActive);
			OnDisconnect();
		}
	}

	void RunDownConnection_l()
	{
		m_connectionLock.Rundown_l();
	}

	void OnReadWriteError(NTSTATUS status);

protected:

	CPipeEnd() : m_packet(0), m_flags(0)
	{
	}

	virtual ~CPipeEnd()
	{
		if (m_packet) m_packet->Release(), m_packet = 0;
	}

	NTSTATUS Listen();

	virtual BOOL IsServer() = 0; 
	virtual BOOL OnRead(PVOID Buffer, ULONG cbTransferred) = 0;
	virtual BOOL OnConnect(NTSTATUS status) = 0;	
	virtual void OnDisconnect() = 0;

	virtual void OnWrite(CDataPacket* , NTSTATUS /*status*/)
	{
	}

#pragma warning(suppress:4100)
	virtual void OnTransact(PVOID context, NTSTATUS status, PSTR buf, ULONG_PTR dwNumberOfBytesTransfered)
	{

	}

	virtual BOOL UseApcCompletion()
	{
		return FALSE;
	}

	virtual VOID OnUnknownCode(CDataPacket* packet, DWORD Code, NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PVOID Pointer);

	virtual ULONG GetReadBuffer(void** ppv);

	void SetConnected()
	{
		m_connectionLock.Init();
	}

public:

	NTSTATUS Assign(HANDLE hFile);
	NTSTATUS SetBuffer(ULONG InBufferSize);
	NTSTATUS Create(_In_ POBJECT_ATTRIBUTES poa, 
		_In_ ULONG InBufferSize = 0, 
		_In_ ULONG nMaxInstances = PIPE_UNLIMITED_INSTANCES, 
		_In_opt_ ULONG PipeType = FILE_PIPE_BYTE_STREAM_TYPE);

	NTSTATUS Write(CDataPacket* packet);
	NTSTATUS Write(const void* lpData, DWORD cbData);

	NTSTATUS Transact(_In_ PVOID pv, _In_ ULONG cb, _In_ ULONG cbMaxOut, _In_opt_ PVOID ctx = 0);

	void Disconnect();
	void Close();

	NTSTATUS Read();
	NTSTATUS Read(PVOID pv, ULONG cb);
};
