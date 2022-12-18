#pragma once

#include "tdi.h"

class CTcpEndpointEvt;

class CTdiAddressEvt : public CTdiAddress, LIST_ENTRY
{
	LIST_ENTRY _readHead = { &_readHead, &_readHead };
	KSPIN_LOCK _lock = 0;
private:

	static NTSTATUS NTAPI ClientEventConnect(
		IN PVOID  TdiEventContext,
		IN LONG  RemoteAddressLength,
		IN PVOID  RemoteAddress,
		IN LONG  UserDataLength,
		IN PVOID  UserData,
		IN LONG  OptionsLength,
		IN PVOID  Options,
		OUT CONNECTION_CONTEXT  *ConnectionContext,
		OUT PIRP  *AcceptIrp
		);

	static NTSTATUS NTAPI ClientEventError(
		IN PVOID  TdiEventContext,
		IN NTSTATUS  Status
		);

	static NTSTATUS NTAPI ClientEventDisconnect(
		IN PVOID  TdiEventContext,
		IN CONNECTION_CONTEXT  ConnectionContext,
		IN LONG  DisconnectDataLength,
		IN PVOID  DisconnectData,
		IN LONG  DisconnectInformationLength,
		IN PVOID  DisconnectInformation,
		IN ULONG  DisconnectFlags
		);

	static NTSTATUS NTAPI ClientEventReceive(
		IN PVOID  TdiEventContext,
		IN CONNECTION_CONTEXT ConnectionContext,
		IN ULONG  ReceiveFlags,
		IN ULONG  BytesIndicated,
		IN ULONG  BytesAvailable,
		OUT ULONG  *BytesTaken,
		IN PVOID  Tsdu,
		OUT PIRP  *IoRequestPacket
		);

	NTSTATUS SetEventHandler(LONG EventType, PVOID EventHandler);
	void StopListen(PLIST_ENTRY head);
protected:
public:

	void QueueIrp(ULONG Type, PIRP Irp, CTcpEndpointEvt* Endpoint);

	NTSTATUS InitEvents();

	NTSTATUS ClearEvents();

	void StopListen();

	CTdiAddressEvt()
	{
		InitializeListHead(this);
	}
};

class CTcpEndpointEvt : public CTcpEndpoint
{
public:
	NTSTATUS Listen();
	CTcpEndpointEvt(CTdiAddressEvt* pAddress) : CTcpEndpoint(pAddress){}
protected:
	virtual NTSTATUS Recve(PVOID Buffer, ULONG ReceiveLength);
private:
	void QueueIrp(ULONG Type, PIRP Irp)
	{
		static_cast<CTdiAddressEvt*>(m_pAddress)->QueueIrp(Type, Irp, this);
	}
};