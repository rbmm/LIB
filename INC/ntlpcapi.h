#pragma once

#ifndef _NTLPCAPI_H
#define _NTLPCAPI_H

extern "C" {

#if defined(USE_LPC6432)

#define LPC_CLIENT_ID CLIENT_ID64
#define LPC_SIZE_T ULONGLONG
#define LPC_PVOID ULONGLONG
#define LPC_HANDLE ULONGLONG

#else

#define LPC_CLIENT_ID CLIENT_ID
#define LPC_SIZE_T SIZE_T
#define LPC_PVOID PVOID
#define LPC_HANDLE HANDLE

#endif
;
struct CLIENT_ID64 {
	ULONGLONG UniqueProcess;
	ULONGLONG UniqueThread;
};

typedef struct PORT_MESSAGE {
	union {
		struct {
			CSHORT DataLength;
			CSHORT TotalLength;
		} s1;
		ULONG Length;
	} u1;
	union {
		struct {
			CSHORT Type;
			CSHORT DataInfoOffset;
		} s2;
		ULONG ZeroInit;
	} u2;
	union {
		LPC_CLIENT_ID ClientId;
		double DoNotUseThisField;       // Force quadword alignment
	};
	ULONG MessageId;
	union {
		LPC_SIZE_T ClientViewSize;          // Only valid on LPC_CONNECTION_REQUEST message
		ULONG CallbackId;                   // Only valid on LPC_REQUEST message
	};
	//  UCHAR Data[];
} *PPORT_MESSAGE;

typedef struct PORT_MESSAGE64 {
	union {
		struct {
			CSHORT DataLength;
			CSHORT TotalLength;
		} s1;
		ULONG Length;
	} u1;
	union {
		struct {
			CSHORT Type;
			CSHORT DataInfoOffset;
		} s2;
		ULONG ZeroInit;
	} u2;
	union {
		CLIENT_ID64 ClientId;
		double DoNotUseThisField;       // Force quadword alignment
	};
	ULONG MessageId;
	union {
		ULONGLONG ClientViewSize;          // Only valid on LPC_CONNECTION_REQUEST message
		ULONG CallbackId;                   // Only valid on LPC_REQUEST message
	};
	//  UCHAR Data[];
} *PPORT_MESSAGE64;

//
// The following bit may be placed in the Type field of a message
// prior calling NtRequestPort or NtRequestWaitReplyPort.  If the
// previous mode is KernelMode, the bit it left as is and passed
// to the receiver of the message.  Otherwise the bit is clear.
//

#define LPC_KERNELMODE_MESSAGE      (CSHORT)((USHORT)0x8000)

typedef struct PORT_VIEW {
	ULONG Length;
	LPC_HANDLE SectionHandle;
	ULONG SectionOffset;
	LPC_SIZE_T ViewSize;
	LPC_PVOID ViewBase;
	LPC_PVOID ViewRemoteBase;
} *PPORT_VIEW;

typedef struct REMOTE_PORT_VIEW {
	ULONG Length;
	LPC_SIZE_T ViewSize;
	LPC_PVOID ViewBase;
} *PREMOTE_PORT_VIEW;

typedef struct PORT_DATA_ENTRY
{
    LPC_PVOID Base;
    ULONG Size;
} *PPORT_DATA_ENTRY;

typedef struct PORT_DATA_INFORMATION
{
    ULONG CountDataEntries;
    PORT_DATA_ENTRY DataEntries[1];
} *PPORT_DATA_INFORMATION;

typedef struct PORT_VIEW64 {
	ULONG Length;
	ULONGLONG SectionHandle;
	ULONG SectionOffset;
	ULONGLONG ViewSize;
	ULONGLONG ViewBase;
	ULONGLONG ViewRemoteBase;
} *PPORT_VIEW64;

typedef struct REMOTE_PORT_VIEW64 {
	ULONG Length;
	ULONGLONG ViewSize;
	ULONGLONG ViewBase;
} *PREMOTE_PORT_VIEW64;

typedef struct PORT_DATA_ENTRY64
{
	ULONGLONG Base;
	ULONG Size;
} *PPORT_DATA_ENTRY64;

enum LPC_TYPE
{
	LPC_NEW_MESSAGE,
	LPC_REQUEST,
	LPC_REPLY,
	LPC_DATAGRAM,
	LPC_LOST_REPLAY,
	LPC_PORT_CLOSED,
	LPC_CLIENT_DIED,
	LPC_EXCEPTION,
	LPC_DEBUG_EVENT,
	LPC_ERROR_EVENT,
	LPC_CONNECTION_REQUEST
};

#define LPC_KERNELMODE_MESSAGE (CSHORT)0x8000
#define LPC_NO_IMPERSONATE (CSHORT)0x4000

#define PORT_VALID_OBJECT_ATTRIBUTES OBJ_CASE_INSENSITIVE

#ifdef _M_IX86
#define PORT_MAXIMUM_MESSAGE_LENGTH 256
#else
#define PORT_MAXIMUM_MESSAGE_LENGTH 512
#endif

#define LPC_MAX_CONNECTION_INFO_SIZE (16 * sizeof(ULONG_PTR))

#define PORT_TOTAL_MAXIMUM_MESSAGE_LENGTH \
    ((PORT_MAXIMUM_MESSAGE_LENGTH + sizeof(PORT_MESSAGE) + LPC_MAX_CONNECTION_INFO_SIZE + 0xf) & ~0xf)

typedef struct _LPC_CLIENT_DIED_MSG
{
    PORT_MESSAGE PortMsg;
    LARGE_INTEGER CreateTime;
} LPC_CLIENT_DIED_MSG, *PLPC_CLIENT_DIED_MSG;

NTSYSCALLAPI
NTSTATUS
NTAPI
NtCreatePort(
    __out PHANDLE PortHandle,
    __in PCOBJECT_ATTRIBUTES ObjectAttributes,
    __in ULONG MaxConnectionInfoLength,
    __in ULONG MaxMessageLength,
    __in_opt ULONG MaxPoolUsage
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtCreateWaitablePort(
    __out PHANDLE PortHandle,
    __in PCOBJECT_ATTRIBUTES ObjectAttributes,
    __in ULONG MaxConnectionInfoLength,
    __in ULONG MaxMessageLength,
    __in_opt ULONG MaxPoolUsage
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtConnectPort(
    __out PHANDLE PortHandle,
    __in PCUNICODE_STRING PortName,
    __in const SECURITY_QUALITY_OF_SERVICE* SecurityQos,
    __inout_opt PPORT_VIEW ClientView,
    __inout_opt PREMOTE_PORT_VIEW ServerView,
    __out_opt PULONG MaxMessageLength,
    __inout_opt PVOID ConnectionInformation,
    __inout_opt PULONG ConnectionInformationLength
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtSecureConnectPort(
    __out PHANDLE PortHandle,
    __in PCUNICODE_STRING PortName,
    __in const SECURITY_QUALITY_OF_SERVICE* SecurityQos,
    __inout_opt PPORT_VIEW ClientView,
    __in_opt PSID RequiredServerSid,
    __inout_opt PREMOTE_PORT_VIEW ServerView,
    __out_opt PULONG MaxMessageLength,
    __inout_opt PVOID ConnectionInformation,
    __inout_opt PULONG ConnectionInformationLength
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
ZwCreatePort(
			 __out PHANDLE PortHandle,
			 __in PCOBJECT_ATTRIBUTES ObjectAttributes,
			 __in ULONG MaxConnectionInfoLength,
			 __in ULONG MaxMessageLength,
			 __in_opt ULONG MaxPoolUsage
			 );

NTSYSCALLAPI
NTSTATUS
NTAPI
ZwCreateWaitablePort(
					 __out PHANDLE PortHandle,
					 __in PCOBJECT_ATTRIBUTES ObjectAttributes,
					 __in ULONG MaxConnectionInfoLength,
					 __in ULONG MaxMessageLength,
					 __in_opt ULONG MaxPoolUsage
					 );

NTSYSCALLAPI
NTSTATUS
NTAPI
ZwConnectPort(
			  __out PHANDLE PortHandle,
			  __in PCUNICODE_STRING PortName,
			  __in const SECURITY_QUALITY_OF_SERVICE* SecurityQos,
			  __inout_opt PPORT_VIEW ClientView,
			  __inout_opt PREMOTE_PORT_VIEW ServerView,
			  __out_opt PULONG MaxMessageLength,
			  __inout_opt PVOID ConnectionInformation,
			  __inout_opt PULONG ConnectionInformationLength
			  );

NTSYSCALLAPI
NTSTATUS
NTAPI
ZwSecureConnectPort(
					__out PHANDLE PortHandle,
					__in PCUNICODE_STRING PortName,
					__in const SECURITY_QUALITY_OF_SERVICE* SecurityQos,
					__inout_opt PPORT_VIEW ClientView,
					__in_opt PSID RequiredServerSid,
					__inout_opt PREMOTE_PORT_VIEW ServerView,
					__out_opt PULONG MaxMessageLength,
					__inout_opt PVOID ConnectionInformation,
					__inout_opt PULONG ConnectionInformationLength
					);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAcceptConnectPort(
    __out PHANDLE PortHandle,
    __in_opt PVOID PortContext,
    __in PPORT_MESSAGE ConnectionRequest,
    __in BOOLEAN AcceptConnection,
    __inout_opt PPORT_VIEW ServerView,
    __out_opt PREMOTE_PORT_VIEW ClientView
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtCompleteConnectPort(
    __in HANDLE PortHandle
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
ZwAcceptConnectPort(
					__out PHANDLE PortHandle,
					__in_opt PVOID PortContext,
					__in PPORT_MESSAGE ConnectionRequest,
					__in BOOLEAN AcceptConnection,
					__inout_opt PPORT_VIEW ServerView,
					__out_opt PREMOTE_PORT_VIEW ClientView
					);

NTSYSCALLAPI
NTSTATUS
NTAPI
ZwCompleteConnectPort(
					  __in HANDLE PortHandle
					  );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtRequestPort(
			  __in HANDLE PortHandle,
			  __in PPORT_MESSAGE RequestMessage
			  );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtRequestWaitReplyPort(
					   __in HANDLE PortHandle,
					   __in PPORT_MESSAGE RequestMessage,
					   __out PPORT_MESSAGE ReplyMessage
					   );
NTSYSCALLAPI
NTSTATUS
NTAPI
ZwRequestPort(
			  __in HANDLE PortHandle,
			  __in PPORT_MESSAGE RequestMessage
			  );

NTSYSCALLAPI
NTSTATUS
NTAPI
ZwRequestWaitReplyPort(
					   __in HANDLE PortHandle,
					   __in PPORT_MESSAGE RequestMessage,
					   __out PPORT_MESSAGE ReplyMessage
					   );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtReplyPort(
			__in HANDLE PortHandle,
			__in PPORT_MESSAGE ReplyMessage
			);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtReplyWaitReplyPort(
					 __in HANDLE PortHandle,
					 __inout PPORT_MESSAGE ReplyMessage
					 );

NTSYSCALLAPI
NTSTATUS
NTAPI
ZwReplyPort(
			__in HANDLE PortHandle,
			__in PPORT_MESSAGE ReplyMessage
			);

NTSYSCALLAPI
NTSTATUS
NTAPI
ZwReplyWaitReplyPort(
					 __in HANDLE PortHandle,
					 __inout PPORT_MESSAGE ReplyMessage
					 );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtReplyWaitReceivePort(
					   __in HANDLE PortHandle,
					   __out_opt PVOID *PortContext,
					   __in_opt PPORT_MESSAGE ReplyMessage,
					   __out PPORT_MESSAGE ReceiveMessage
					   );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtReplyWaitReceivePortEx(
						 __in HANDLE PortHandle,
						 __out_opt PVOID *PortContext,
						 __in_opt PPORT_MESSAGE ReplyMessage,
						 __out PPORT_MESSAGE ReceiveMessage,
						 __in_opt PLARGE_INTEGER Timeout
						 );

NTSYSCALLAPI
NTSTATUS
NTAPI
ZwReplyWaitReceivePort(
					   __in HANDLE PortHandle,
					   __out_opt PVOID *PortContext,
					   __in_opt PPORT_MESSAGE ReplyMessage,
					   __out PPORT_MESSAGE ReceiveMessage
					   );

NTSYSCALLAPI
NTSTATUS
NTAPI
ZwReplyWaitReceivePortEx(
						 __in HANDLE PortHandle,
						 __out_opt PVOID *PortContext,
						 __in_opt PPORT_MESSAGE ReplyMessage,
						 __out PPORT_MESSAGE ReceiveMessage,
						 __in_opt PLARGE_INTEGER Timeout
						 );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtImpersonateClientOfPort(
						  __in HANDLE PortHandle,
						  __in PPORT_MESSAGE Message
						  );

NTSYSCALLAPI
NTSTATUS
NTAPI
ZwImpersonateClientOfPort(
						  __in HANDLE PortHandle,
						  __in PPORT_MESSAGE Message
						  );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtReadRequestData(
				  __in HANDLE PortHandle,
				  __in PPORT_MESSAGE Message,
				  __in ULONG DataEntryIndex,
				  __out_bcount(BufferSize) PVOID Buffer,
				  __in SIZE_T BufferSize,
				  __out_opt PSIZE_T NumberOfBytesRead
				  );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtWriteRequestData(
				   __in HANDLE PortHandle,
				   __in PPORT_MESSAGE Message,
				   __in ULONG DataEntryIndex,
				   __in_bcount(BufferSize) PVOID Buffer,
				   __in SIZE_T BufferSize,
				   __out_opt PSIZE_T NumberOfBytesWritten
				   );

NTSYSCALLAPI
NTSTATUS
NTAPI
ZwReadRequestData(
				  __in HANDLE PortHandle,
				  __in PPORT_MESSAGE Message,
				  __in ULONG DataEntryIndex,
				  __out_bcount(BufferSize) PVOID Buffer,
				  __in SIZE_T BufferSize,
				  __out_opt PSIZE_T NumberOfBytesRead
				  );

NTSYSCALLAPI
NTSTATUS
NTAPI
ZwWriteRequestData(
				   __in HANDLE PortHandle,
				   __in PPORT_MESSAGE Message,
				   __in ULONG DataEntryIndex,
				   __in_bcount(BufferSize) PVOID Buffer,
				   __in SIZE_T BufferSize,
				   __out_opt PSIZE_T NumberOfBytesWritten
				   );

typedef enum _PORT_INFORMATION_CLASS
{
    PortBasicInformation,
    PortDumpInformation
} PORT_INFORMATION_CLASS;

NTSYSCALLAPI
NTSTATUS
NTAPI
NtQueryInformationPort(
					   __in HANDLE PortHandle,
					   __in PORT_INFORMATION_CLASS PortInformationClass,
					   __out_bcount(Length) PVOID PortInformation,
					   __in ULONG Length,
					   __out_opt PULONG ReturnLength
					   );

NTSYSCALLAPI
NTSTATUS
NTAPI
ZwQueryInformationPort(
					   __in HANDLE PortHandle,
					   __in PORT_INFORMATION_CLASS PortInformationClass,
					   __out_bcount(Length) PVOID PortInformation,
					   __in ULONG Length,
					   __out_opt PULONG ReturnLength
					   );

NTSYSCALLAPI
NTSTATUS
NTAPI
LpcRequestWaitReplyPort
(
 IN PVOID Port,
 IN PPORT_MESSAGE RequestMessage,
 OUT PPORT_MESSAGE ReplyMessage
 );

NTSYSCALLAPI
NTSTATUS
NTAPI
LpcRequestPort
(
 IN PVOID Port,
 IN PPORT_MESSAGE RequestMessage
 );

// Asynchronous Local Inter-process Communication

// ALPC handles aren't NT object manager handles, and
// it seems traditional to use a typedef in these cases.
// rev
typedef PVOID ALPC_HANDLE, *PALPC_HANDLE;

#define ALPC_PORFLG_ALLOW_LPC_REQUESTS 0x20000 // rev
#define ALPC_PORFLG_WAITABLE_PORT 0x40000 // dbg
#define ALPC_PORFLG_SYSTEM_PROCESS 0x100000 // dbg

// symbols
typedef struct _ALPC_PORT_ATTRIBUTES
{
    ULONG Flags;
    SECURITY_QUALITY_OF_SERVICE SecurityQos;
    SIZE_T MaxMessageLength;
    SIZE_T MemoryBandwidth;
    SIZE_T MaxPoolUsage;
    SIZE_T MaxSectionSize;
    SIZE_T MaxViewSize;
    SIZE_T MaxTotalSectionSize;
    ULONG DupObjectTypes;
#ifdef _M_X64
    ULONG Reserved;
#endif
} ALPC_PORT_ATTRIBUTES, *PALPC_PORT_ATTRIBUTES;

// begin_rev
#define ALPC_MESSAGE_SECURITY_ATTRIBUTE 0x80000000
#define ALPC_MESSAGE_VIEW_ATTRIBUTE 0x40000000
#define ALPC_MESSAGE_CONTEXT_ATTRIBUTE 0x20000000
#define ALPC_MESSAGE_HANDLE_ATTRIBUTE 0x10000000
// end_rev

// symbols
typedef struct _ALPC_MESSAGE_ATTRIBUTES
{
    ULONG AllocatedAttributes;
    ULONG ValidAttributes;
} ALPC_MESSAGE_ATTRIBUTES, *PALPC_MESSAGE_ATTRIBUTES;

// symbols
typedef struct _ALPC_COMPLETION_LIST_STATE
{
    union
    {
        struct
        {
            ULONG64 Head : 24;
            ULONG64 Tail : 24;
            ULONG64 ActiveThreadCount : 16;
        } s1;
        ULONG64 Value;
    } u1;
} ALPC_COMPLETION_LIST_STATE, *PALPC_COMPLETION_LIST_STATE;

#define ALPC_COMPLETION_LIST_BUFFER_GRANULARITY_MASK 0x3f // dbg

// symbols
typedef struct DECLSPEC_ALIGN(128) _ALPC_COMPLETION_LIST_HEADER
{
    ULONG64 StartMagic;

    ULONG TotalSize;
    ULONG ListOffset;
    ULONG ListSize;
    ULONG BitmapOffset;
    ULONG BitmapSize;
    ULONG DataOffset;
    ULONG DataSize;
    ULONG AttributeFlags;
    ULONG AttributeSize;

    DECLSPEC_ALIGN(128) ALPC_COMPLETION_LIST_STATE State;
    ULONG LastMessageId;
    ULONG LastCallbackId;
    DECLSPEC_ALIGN(128) ULONG PostCount;
    DECLSPEC_ALIGN(128) ULONG ReturnCount;
    DECLSPEC_ALIGN(128) ULONG LogSequenceNumber;
    DECLSPEC_ALIGN(128) RTL_SRWLOCK UserLock;

    ULONG64 EndMagic;
} ALPC_COMPLETION_LIST_HEADER, *PALPC_COMPLETION_LIST_HEADER;

// private
typedef struct _ALPC_CONTEXT_ATTR
{
    PVOID PortContext;
    PVOID MessageContext;
    ULONG Sequence;
    ULONG MessageId;
    ULONG CallbackId;
} ALPC_CONTEXT_ATTR, *PALPC_CONTEXT_ATTR;

// begin_rev
#define ALPC_HANDLEFLG_DUPLICATE_SAME_ACCESS 0x10000
#define ALPC_HANDLEFLG_DUPLICATE_SAME_ATTRIBUTES 0x20000
#define ALPC_HANDLEFLG_DUPLICATE_INHERIT 0x80000
// end_rev

// private
typedef struct _ALPC_HANDLE_ATTR
{
    ULONG Flags;
    HANDLE Handle;
    ULONG ObjectType; // ObjectTypeCode, not ObjectTypeIndex
    ACCESS_MASK DesiredAccess;
} ALPC_HANDLE_ATTR, *PALPC_HANDLE_ATTR;

#define ALPC_SECFLG_CREATE_HANDLE 0x20000 // dbg

// name:private
// rev
typedef struct _ALPC_SECURITY_ATTR
{
    ULONG Flags;
    const SECURITY_QUALITY_OF_SERVICE* SecurityQos;
    ALPC_HANDLE ContextHandle; // dbg
    ULONG Reserved1;
    ULONG Reserved2;
} ALPC_SECURITY_ATTR, *PALPC_SECURITY_ATTR;

// begin_rev
#define ALPC_VIEWFLG_NOT_SECURE 0x40000
// end_rev

// private
typedef struct _ALPC_DATA_VIEW_ATTR
{
    ULONG Flags;
    ALPC_HANDLE SectionHandle;
    PVOID ViewBase; // must be zero on input
    SIZE_T ViewSize;
} ALPC_DATA_VIEW_ATTR, *PALPC_DATA_VIEW_ATTR;

// private
typedef enum _ALPC_PORT_INFORMATION_CLASS
{
    AlpcBasicInformation, // q: out ALPC_BASIC_INFORMATION
    AlpcPortInformation, // s: in ALPC_PORT_ATTRIBUTES
    AlpcAssociateCompletionPortInformation, // s: in ALPC_PORT_ASSOCIATE_COMPLETION_PORT
    AlpcConnectedSIDInformation, // q: in SID
    AlpcServerInformation, // q: inout ALPC_SERVER_INFORMATION
    AlpcMessageZoneInformation, // s: in ALPC_PORT_MESSAGE_ZONE_INFORMATION
    AlpcRegisterCompletionListInformation, // s: in ALPC_PORT_COMPLETION_LIST_INFORMATION
    AlpcUnregisterCompletionListInformation, // s: VOID
    AlpcAdjustCompletionListConcurrencyCountInformation, // s: in ULONG
    AlpcRegisterCallback, // kernel-mode only // rev
    AlpcDisableCompletionList, // s: VOID // rev
    MaxAlpcPortInfoClass
} ALPC_PORT_INFORMATION_CLASS;

// private
typedef struct _ALPC_BASIC_INFORMATION
{
    ULONG Flags;
    ULONG SequenceNo;
    PVOID PortContext;
} ALPC_BASIC_INFORMATION, *PALPC_BASIC_INFORMATION;

// private
typedef struct _ALPC_PORT_ASSOCIATE_COMPLETION_PORT
{
    PVOID CompletionKey;
    HANDLE CompletionPort;
} ALPC_PORT_ASSOCIATE_COMPLETION_PORT, *PALPC_PORT_ASSOCIATE_COMPLETION_PORT;

// private
typedef struct _ALPC_SERVER_INFORMATION
{
    union
    {
        struct
        {
            HANDLE ThreadHandle;
        } In;
        struct
        {
            BOOLEAN ThreadBlocked;
            HANDLE ConnectedProcessId;
            UNICODE_STRING ConnectionPortName;
        } Out;
    };
} ALPC_SERVER_INFORMATION, *PALPC_SERVER_INFORMATION;

// private
typedef struct _ALPC_PORT_MESSAGE_ZONE_INFORMATION
{
    PVOID Buffer;
    ULONG Size;
} ALPC_PORT_MESSAGE_ZONE_INFORMATION, *PALPC_PORT_MESSAGE_ZONE_INFORMATION;

// private
typedef struct _ALPC_PORT_COMPLETION_LIST_INFORMATION
{
    PVOID Buffer; // PALPC_COMPLETION_LIST_HEADER
    ULONG Size;
    ULONG ConcurrencyCount;
    ULONG AttributeFlags;
} ALPC_PORT_COMPLETION_LIST_INFORMATION, *PALPC_PORT_COMPLETION_LIST_INFORMATION;

// private
typedef enum _ALPC_MESSAGE_INFORMATION_CLASS
{
    AlpcMessageSidInformation, // q: out SID
    AlpcMessageTokenModifiedIdInformation,  // q: out LUID
    MaxAlpcMessageInfoClass
} ALPC_MESSAGE_INFORMATION_CLASS, *PALPC_MESSAGE_INFORMATION_CLASS;

// begin_private

// System calls

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcCreatePort(
    __out PHANDLE PortHandle,
    __in POBJECT_ATTRIBUTES ObjectAttributes,
    __in_opt PALPC_PORT_ATTRIBUTES PortAttributes
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcDisconnectPort(
    __in HANDLE PortHandle,
    __in ULONG Flags
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcQueryInformation(
    __in HANDLE PortHandle,
    __in ALPC_PORT_INFORMATION_CLASS PortInformationClass,
    __out_bcount(Length) PVOID PortInformation,
    __in ULONG Length,
    __out_opt PULONG ReturnLength
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcSetInformation(
    __in HANDLE PortHandle,
    __in ALPC_PORT_INFORMATION_CLASS PortInformationClass,
    __in_bcount(Length) PVOID PortInformation,
    __in ULONG Length
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcCreatePortSection(
    __in HANDLE PortHandle,
    __in ULONG Flags,
    __in_opt HANDLE SectionHandle,
    __in SIZE_T SectionSize,
    __out PALPC_HANDLE AlpcSectionHandle,
    __out PSIZE_T ActualSectionSize
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcDeletePortSection(
    __in HANDLE PortHandle,
    __reserved ULONG Flags,
    __in ALPC_HANDLE SectionHandle
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcCreateResourceReserve(
    __in HANDLE PortHandle,
    __reserved ULONG Flags,
    __in SIZE_T MessageSize,
    __out PALPC_HANDLE ResourceId
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcDeleteResourceReserve(
    __in HANDLE PortHandle,
    __reserved ULONG Flags,
    __in ALPC_HANDLE ResourceId
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcCreateSectionView(
    __in HANDLE PortHandle,
    __reserved ULONG Flags,
    __inout PALPC_DATA_VIEW_ATTR ViewAttributes
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcDeleteSectionView(
    __in HANDLE PortHandle,
    __reserved ULONG Flags,
    __in PVOID ViewBase
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcCreateSecurityContext(
    __in HANDLE PortHandle,
    __reserved ULONG Flags,
    __inout PALPC_SECURITY_ATTR SecurityAttribute
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcDeleteSecurityContext(
    __in HANDLE PortHandle,
    __reserved ULONG Flags,
    __in ALPC_HANDLE ContextHandle
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcRevokeSecurityContext(
    __in HANDLE PortHandle,
    __reserved ULONG Flags,
    __in ALPC_HANDLE ContextHandle
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcQueryInformationMessage(
    __in HANDLE PortHandle,
    __in PPORT_MESSAGE PortMessage,
    __in ALPC_MESSAGE_INFORMATION_CLASS MessageInformationClass,
    __out_bcount(Length) PVOID MessageInformation,
    __in ULONG Length,
    __out_opt PULONG ReturnLength
    );

#define ALPC_MSGFLG_REPLY_MESSAGE 0x1
#define ALPC_MSGFLG_LPC_MODE 0x2 // ?
#define ALPC_MSGFLG_RELEASE_MESSAGE 0x10000 // dbg
#define ALPC_MSGFLG_SYNC_REQUEST 0x20000 // dbg
#define ALPC_MSGFLG_WAIT_USER_MODE 0x100000
#define ALPC_MSGFLG_WAIT_ALERTABLE 0x200000
#define ALPC_MSGFLG_WOW64_CALL 0x80000000 // dbg

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcConnectPort(
    __out PHANDLE PortHandle,
    __in PCUNICODE_STRING PortName,
    __in POBJECT_ATTRIBUTES ObjectAttributes,
    __in_opt PALPC_PORT_ATTRIBUTES PortAttributes,
    __in ULONG Flags,
    __in_opt PSID RequiredServerSid,
    __inout PPORT_MESSAGE ConnectionMessage,
    __inout_opt PULONG BufferLength,
    __inout_opt PALPC_MESSAGE_ATTRIBUTES OutMessageAttributes,
    __inout_opt PALPC_MESSAGE_ATTRIBUTES InMessageAttributes,
    __in_opt PLARGE_INTEGER Timeout
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcAcceptConnectPort(
    __out PHANDLE PortHandle,
    __in HANDLE ConnectionPortHandle,
    __in ULONG Flags,
    __in POBJECT_ATTRIBUTES ObjectAttributes,
    __in PALPC_PORT_ATTRIBUTES PortAttributes,
    __in_opt PVOID PortContext,
    __in PPORT_MESSAGE ConnectionRequest,
    __inout_opt PALPC_MESSAGE_ATTRIBUTES ConnectionMessageAttributes,
    __in BOOLEAN AcceptConnection
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcSendWaitReceivePort(
    __in HANDLE PortHandle,
    __in ULONG Flags,
    __in_opt PPORT_MESSAGE SendMessage,
    __in_opt PALPC_MESSAGE_ATTRIBUTES SendMessageAttributes,
    __inout_opt PPORT_MESSAGE ReceiveMessage,
    __inout_opt PULONG BufferLength,
    __inout_opt PALPC_MESSAGE_ATTRIBUTES ReceiveMessageAttributes,
    __in_opt PLARGE_INTEGER Timeout
    );

#define ALPC_CANCELFLG_TRY_CANCEL 0x1 // dbg
#define ALPC_CANCELFLG_NO_CONTEXT_CHECK 0x8
#define ALPC_CANCELFLGP_FLUSH 0x10000 // dbg

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcCancelMessage(
    __in HANDLE PortHandle,
    __in ULONG Flags,
    __in PALPC_CONTEXT_ATTR MessageContext
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcImpersonateClientOfPort(
    __in HANDLE PortHandle,
    __in PPORT_MESSAGE PortMessage,
    __reserved PVOID Reserved
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcOpenSenderProcess(
    __out PHANDLE ProcessHandle,
    __in HANDLE PortHandle,
    __in PPORT_MESSAGE PortMessage,
    __reserved ULONG Flags,
    __in ACCESS_MASK DesiredAccess,
    __in POBJECT_ATTRIBUTES ObjectAttributes
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlpcOpenSenderThread(
    __out PHANDLE ThreadHandle,
    __in HANDLE PortHandle,
    __in PPORT_MESSAGE PortMessage,
    __reserved ULONG Flags,
    __in ACCESS_MASK DesiredAccess,
    __in POBJECT_ATTRIBUTES ObjectAttributes
    );

// Support functions

NTSYSAPI
ULONG
NTAPI
AlpcMaxAllowedMessageLength(
    VOID
    );

NTSYSAPI
ULONG
NTAPI
AlpcGetHeaderSize(
    __in ULONG Flags
    );

NTSYSAPI
NTSTATUS
NTAPI
AlpcInitializeMessageAttribute(
    __in ULONG AttributeFlags,
    __out_opt PALPC_MESSAGE_ATTRIBUTES Buffer,
    __in ULONG BufferSize,
    __out PULONG RequiredBufferSize
    );

NTSYSAPI
PVOID
NTAPI
AlpcGetMessageAttribute(
    __in PALPC_MESSAGE_ATTRIBUTES Buffer,
    __in ULONG AttributeFlag
    );

NTSYSAPI
NTSTATUS
NTAPI
AlpcRegisterCompletionList(
    __in HANDLE PortHandle,
    __out PALPC_COMPLETION_LIST_HEADER Buffer,
    __in ULONG Size,
    __in ULONG ConcurrencyCount,
    __in ULONG AttributeFlags
    );

NTSYSAPI
NTSTATUS
NTAPI
AlpcUnregisterCompletionList(
    __in HANDLE PortHandle
    );

// rev
NTSYSAPI
NTSTATUS
NTAPI
AlpcRundownCompletionList(
    __in HANDLE PortHandle
    );

NTSYSAPI
NTSTATUS
NTAPI
AlpcAdjustCompletionListConcurrencyCount(
    __in HANDLE PortHandle,
    __in ULONG ConcurrencyCount
    );

NTSYSAPI
BOOLEAN
NTAPI
AlpcRegisterCompletionListWorkerThread(
    __inout PVOID CompletionList
    );

NTSYSAPI
BOOLEAN
NTAPI
AlpcUnregisterCompletionListWorkerThread(
    __inout PVOID CompletionList
    );

NTSYSAPI
VOID
NTAPI
AlpcGetCompletionListLastMessageInformation(
    __in PVOID CompletionList,
    __out PULONG LastMessageId,
    __out PULONG LastCallbackId
    );

NTSYSAPI
ULONG
NTAPI
AlpcGetOutstandingCompletionListMessageCount(
    __in PVOID CompletionList
    );

NTSYSAPI
PPORT_MESSAGE
NTAPI
AlpcGetMessageFromCompletionList(
    __in PVOID CompletionList,
    __out_opt PALPC_MESSAGE_ATTRIBUTES *MessageAttributes
    );

NTSYSAPI
VOID
NTAPI
AlpcFreeCompletionListMessage(
    __inout PVOID CompletionList,
    __in PPORT_MESSAGE Message
    );

NTSYSAPI
PALPC_MESSAGE_ATTRIBUTES
NTAPI
AlpcGetCompletionListMessageAttributes(
    __in PVOID CompletionList,
    __in PPORT_MESSAGE Message
    );

}

#endif
