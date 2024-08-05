#pragma once

#define EXTEND64(Addr) ((ULONG64)(LONG_PTR)(Addr))

#define BREAKIN_PACKET_BYTE 'b'

#define PACKET_BYTE_COUNT 0x438

enum PACKET_KD_LEADER : DWORD {

	PACKET_LEADER = '0000',
	CONTROL_PACKET_LEADER = 'iiii'
};

enum PACKET_KD_TYPE : WORD
{
	PACKET_TYPE_KD_STATE_MANIPULATE=2,
	PACKET_TYPE_KD_DEBUG_IO,
	PACKET_TYPE_KD_ACKNOWLEDGE,
	PACKET_TYPE_KD_RESEND,
	PACKET_TYPE_KD_RESET,
	PACKET_TYPE_KD_STATE_CHANGE,
	PACKET_TYPE_KD_BREAKIN,
	PACKET_TYPE_KD_TRACE_IO,
	PACKET_TYPE_KD_CONTROL_REQUEST,
	PACKET_TYPE_KD_FILE_IO
};

struct KD_PACKET 
{
	PACKET_KD_LEADER PacketLeader;
	PACKET_KD_TYPE PacketType;
	WORD ByteCount;
	DWORD PacketId;
	DWORD Checksum;
};

enum DBGK_API
{
	DbgKdNoWait,
	//
	// Wait State Change Types
	//

	DbgKdExceptionStateChange = '00',
	DbgKdLoadSymbolsStateChange,
	DbgKdCommandStringStateChange,

	//
	// Manipulate Types
	//

	DbgKdReadVirtualMemoryApi = '10',
	DbgKdWriteVirtualMemoryApi,
	DbgKdGetContextApi,
	DbgKdSetContextApi,
	DbgKdWriteBreakPointApi,
	DbgKdRestoreBreakPointApi,
	DbgKdContinueApi,
	DbgKdReadControlSpaceApi,
	DbgKdWriteControlSpaceApi,
	DbgKdReadIoSpaceApi,
	DbgKdWriteIoSpaceApi,
	DbgKdRebootApi,
	DbgKdContinueApi2,
	DbgKdReadPhysicalMemoryApi,
	DbgKdWritePhysicalMemoryApi,
	DbgKdQuerySpecialCallsApi,
	DbgKdSetSpecialCallApi,
	DbgKdClearSpecialCallsApi,
	DbgKdSetInternalBreakPointApi,
	DbgKdGetInternalBreakPointApi,
	DbgKdReadIoSpaceExtendedApi,
	DbgKdWriteIoSpaceExtendedApi,
	DbgKdGetVersionApi,
	DbgKdWriteBreakPointExApi,
	DbgKdRestoreBreakPointExApi,
	DbgKdCauseBugCheckApi,
	DbgKdSwitchProcessor = 0x3150,
	DbgKdPageInApi,
	DbgKdReadMachineSpecificRegister,
	DbgKdWriteMachineSpecificRegister,
	OldVlm1,
	OldVlm2,
	DbgKdSearchMemoryApi,
	DbgKdGetBusDataApi,
	DbgKdSetBusDataApi,
	DbgKdCheckLowMemoryApi,
	DbgKdClearAllInternalBreakpointsApi,
	DbgKdFillMemoryApi,
	DbgKdQueryMemoryApi,
	DbgKdSwitchPartition,
	DbgKdWriteCustomBreakpointApi,
	DbgKdGetContextExApi,
	DbgKdSetContextExApi,
	DbgKdMaximumManipulate,

	//
	// Debug I/O Types
	//
	DbgKdPrintStringApi = '20',
	DbgKdGetStringApi,

	//
	// Trace I/O Types
	//
	DbgKdPrintTraceApi = '30',

	//
	// File I/O Types
	//
	DbgKdCreateFileApi = '40',
	DbgKdReadFileApi,
	DbgKdWriteFileApi,
	DbgKdCloseFileApi,
};
/************************************************************************/
/* 
*/
/************************************************************************/
struct DBGKD_HEADER 
{
	DBGK_API ApiNumber;
	union {
		struct{
			WORD ProcessorLevel;
			WORD Processor;
		};
		NTSTATUS Status;
	};
};

struct DBGKD_DEBUG_IO : public DBGKD_HEADER  
{
	DWORD LengthOfPromptString;
	DWORD LengthOfStringRead;
	char String[];
};//10

struct _X86_DBGKD_CONTROL_SET
{
	/*0000*/ULONG TraceFlag;
	/*0004*/ULONG Dr7;
	/*0008*/ULONG CurrentSymbolStart;
	/*000C*/ULONG CurrentSymbolEnd;
};

struct _IA64_DBGKD_CONTROL_SET
{
	/*0000*/ULONG Continue;
	/*0004*/ULONGLONG CurrentSymbolStart;
	/*000C*/ULONGLONG CurrentSymbolEnd;
};

struct _AMD64_DBGKD_CONTROL_SET
{
	/*0000*/ULONG TraceFlag;
	/*0004*/ULONGLONG Dr7;
	/*000C*/ULONGLONG CurrentSymbolStart;
	/*0014*/ULONGLONG CurrentSymbolEnd;
};

union _DBGKD_ANY_CONTROL_SET
{
	/*0000*/_X86_DBGKD_CONTROL_SET X86ControlSet;
	/*0000*/_IA64_DBGKD_CONTROL_SET IA64ControlSet;
	/*0000*/_AMD64_DBGKD_CONTROL_SET Amd64ControlSet;
};

struct _DBGKD_BREAKPOINTEX
{
	/*0000*/ULONG BreakPointCount;
	/*0004*/LONG ContinueStatus;
};

struct _DBGKD_CONTINUE
{
	/*0000*/LONG ContinueStatus;
};

struct _DBGKD_CONTINUE2
{
	/*0000*/LONG ContinueStatus;
	/*0004*/_DBGKD_ANY_CONTROL_SET AnyControlSet;
};

struct _DBGKD_FILL_MEMORY
{
	/*0000*/ULONGLONG Address;
	/*0008*/ULONG Length;
	/*000C*/USHORT Flags;
	/*000E*/USHORT PatternLength;
};

struct _DBGKD_GET_INTERNAL_BREAKPOINT
{
	/*0000*/ULONGLONG BreakpointAddress;
	/*0008*/ULONG Flags;
	/*000C*/ULONG Calls;
	/*0010*/ULONG MaxCallsPerPeriod;
	/*0014*/ULONG MinInstructions;
	/*0018*/ULONG MaxInstructions;
	/*001C*/ULONG TotalInstructions;
};

struct _DBGKD_GET_SET_BUS_DATA
{
	/*0000*/ULONG BusDataType;
	/*0004*/ULONG BusNumber;
	/*0008*/ULONG SlotNumber;
	/*000C*/ULONG Offset;
	/*0010*/ULONG Length;
};

struct _DBGKD_GET_VERSION
{
	/*0000*/USHORT MajorVersion;
	/*0002*/USHORT MinorVersion;
	/*0004*/UCHAR ProtocolVersion;
	/*0005*/UCHAR KdSecondaryVersion;
	/*0006*/USHORT Flags;
	/*0008*/USHORT MachineType;
	/*000A*/UCHAR MaxPacketType;
	/*000B*/UCHAR MaxStateChange;
	/*000C*/UCHAR MaxManipulate;
	/*000D*/UCHAR Simulation;
	/*000E*/USHORT Unused[0x1];
	/*0010*/ULONGLONG KernBase;
	/*0018*/ULONGLONG PsLoadedModuleList;
	/*0020*/ULONGLONG DebuggerDataList;
};

struct _DBGKD_QUERY_MEMORY
{
	/*0000*/ULONGLONG Address;
	/*0008*/ULONGLONG Reserved;
	/*0010*/ULONG AddressSpace;
	/*0014*/ULONG Flags;
};

struct _DBGKD_QUERY_SPECIAL_CALLS
{
	/*0000*/ULONG NumberOfSpecialCalls;
};

struct _DBGKD_READ_WRITE_MEMORY
{
	/*0000*/ULONGLONG TargetBaseAddress;
	/*0008*/ULONG TransferCount;
	/*000C*/ULONG ActualTransferCount;
};

struct _DBGKD_READ_WRITE_IO
{
	/*0000*/ULONGLONG IoAddress;
	/*0008*/ULONG DataSize;
	/*000C*/ULONG DataValue;
};

struct _DBGKD_READ_WRITE_IO_EXTENDED
{
	/*0000*/ULONG DataSize;
	/*0004*/ULONG InterfaceType;
	/*0008*/ULONG BusNumber;
	/*000C*/ULONG AddressSpace;
	/*0010*/ULONGLONG IoAddress;
	/*0018*/ULONG DataValue;
};

struct _DBGKD_READ_WRITE_MSR
{
	/*0000*/ULONG Msr;
	/*0004*/ULONG DataValueLow;
	/*0008*/ULONG DataValueHigh;
};

struct _DBGKD_RESTORE_BREAKPOINT
{
	/*0000*/ULONG BreakPointHandle;
};

struct _DBGKD_SEARCH_MEMORY
{
	/*0000*/ULONGLONG SearchAddress;
	/*0000*/ULONGLONG FoundAddress;
	/*0008*/ULONGLONG SearchLength;
	/*0010*/ULONG PatternLength;
};

struct _DBGKD_SET_CONTEXT
{
	/*0000*/ULONG ContextFlags;
};

struct _DBGKD_SET_INTERNAL_BREAKPOINT
{
	/*0000*/ULONGLONG BreakpointAddress;
	/*0008*/ULONG Flags;
};

struct _DBGKD_SET_SPECIAL_CALL
{
	/*0000*/ULONGLONG SpecialCall;
};

struct _DBGKD_SWITCH_PARTITION
{
	/*0000*/ULONG Partition;
};

struct _DBGKD_WRITE_BREAKPOINT
{
	/*0000*/ULONGLONG BreakPointAddress;
	/*0008*/ULONG BreakPointHandle;
};

struct _DBGKD_CONTEXT_EX {
	/*0000*/ ULONG Offset;
	/*0004*/ ULONG ByteCount;
	/*0008*/ ULONG BytesCopied;
	/*000c*/
};

struct DESCRIPTOR64 { 
	/*0000*/ USHORT Pad[0x3];
	/*0006*/ USHORT Limit;
	/*0008*/ void * Base;
	/*0010*/
}; 

struct KSPECIAL_REGISTERS_X64 {
	/*0000*/ ULONGLONG Cr0;
	/*0008*/ ULONGLONG Cr2;
	/*0010*/ ULONGLONG Cr3;
	/*0018*/ ULONGLONG Cr4;
	/*0020*/ ULONGLONG KernelDr0;
	/*0028*/ ULONGLONG KernelDr1;
	/*0030*/ ULONGLONG KernelDr2;
	/*0038*/ ULONGLONG KernelDr3;
	/*0040*/ ULONGLONG KernelDr6;
	/*0048*/ ULONGLONG KernelDr7;
	/*0050*/ DESCRIPTOR64 Gdtr;
	/*0060*/ DESCRIPTOR64 Idtr;
	/*0070*/ USHORT Tr;
	/*0072*/ USHORT Ldtr;
	/*0074*/ ULONG MxCsr;
	/*0078*/ ULONGLONG DebugControl;
	/*0080*/ ULONGLONG LastBranchToRip;
	/*0088*/ ULONGLONG LastBranchFromRip;
	/*0090*/ ULONGLONG LastExceptionToRip;
	/*0098*/ ULONGLONG LastExceptionFromRip;
	/*00a0*/ ULONGLONG Cr8;
	/*00a8*/ ULONGLONG MsrGsBase;
	/*00b0*/ ULONGLONG MsrGsSwap;
	/*00b8*/ ULONGLONG MsrStar;
	/*00c0*/ ULONGLONG MsrLStar;
	/*00c8*/ ULONGLONG MsrCStar;
	/*00d0*/ ULONGLONG MsrSyscallMask;
	/*00d8*/ ULONGLONG Xcr0;
	/*00e0*/ ULONGLONG MsrFsBase;
	/*00e8*/ ULONGLONG SpecialPadding0;
	/*00f0*/
};

struct KPROCESSOR_STATE_X64 {
	/*0000*/ KSPECIAL_REGISTERS_X64 SpecialRegisters;
	/*00f0*/ CONTEXT ContextFrame;
	/*05c0*/
};

struct DESCRIPTOR {
	/*0000*/ USHORT Pad;
	/*0002*/ USHORT Limit;
	/*0004*/ ULONG Base;
	/*0008*/
};

struct KSPECIAL_REGISTERS_X86 {
	/*0000*/ ULONG Cr0;
	/*0004*/ ULONG Cr2;
	/*0008*/ ULONG Cr3;
	/*000c*/ ULONG Cr4;
	/*0010*/ ULONG KernelDr0;
	/*0014*/ ULONG KernelDr1;
	/*0018*/ ULONG KernelDr2;
	/*001c*/ ULONG KernelDr3;
	/*0020*/ ULONG KernelDr6;
	/*0024*/ ULONG KernelDr7;
	/*0028*/ DESCRIPTOR Gdtr;
	/*0030*/ DESCRIPTOR Idtr;
	/*0038*/ USHORT Tr;
	/*003a*/ USHORT Ldtr;
	/*003c*/ ULONG Reserved[0x6];
	/*0054*/
};

struct KPROCESSOR_STATE_X86 {
	/*0000*/ WOW64_CONTEXT ContextFrame;
	/*02cc*/ KSPECIAL_REGISTERS_X86 SpecialRegisters;
};

enum CONTROL_SPACE_TYPE {
	AMD64_DEBUG_CONTROL_SPACE_KPCR,
	AMD64_DEBUG_CONTROL_SPACE_KPRCB,
	AMD64_DEBUG_CONTROL_SPACE_KSPECIAL,
	AMD64_DEBUG_CONTROL_SPACE_KTHREAD,
	X86_DEBUG_CONTROL_SPACE_KSPECIAL = offsetof(KPROCESSOR_STATE_X86, SpecialRegisters),
};

struct _DBGKD_READ_CONTROL_SPACE
{
	CONTROL_SPACE_TYPE Type;
	ULONG Pad;
	ULONG ByteCount;
	ULONG BytesCopied;
};

struct DBGKD_MANIPULATE_STATE : public DBGKD_HEADER
{
	/*0008*/LONG ReturnStatus;
	/*000C*/LONG pad;
	/*0010*/
	union 
	{
		/*0000*/_DBGKD_READ_WRITE_MEMORY ReadWriteMemory;
		/*0010*/_DBGKD_CONTEXT_EX GetContextEx;
		/*0010*/_DBGKD_CONTEXT_EX SetContextEx;
		/*0000*/_DBGKD_WRITE_BREAKPOINT WriteBreakPoint;
		/*0000*/_DBGKD_RESTORE_BREAKPOINT RestoreBreakPoint;
		/*0000*/_DBGKD_CONTINUE Continue;
		/*0000*/_DBGKD_CONTINUE2 Continue2;
		/*0000*/_DBGKD_READ_WRITE_IO ReadWriteIo;
		/*0000*/_DBGKD_READ_WRITE_IO_EXTENDED ReadWriteIoExtended;
		/*0000*/_DBGKD_QUERY_SPECIAL_CALLS QuerySpecialCalls;
		/*0000*/_DBGKD_SET_SPECIAL_CALL SetSpecialCall;
		/*0000*/_DBGKD_SET_INTERNAL_BREAKPOINT SetInternalBreakpoint;
		/*0000*/_DBGKD_GET_INTERNAL_BREAKPOINT GetInternalBreakpoint;
		/*0000*/_DBGKD_GET_VERSION GetVersion;
		/*0000*/_DBGKD_BREAKPOINTEX BreakPointEx;
		/*0000*/_DBGKD_READ_WRITE_MSR ReadWriteMsr;
		/*0000*/_DBGKD_SEARCH_MEMORY SearchMemory;
		/*0000*/_DBGKD_GET_SET_BUS_DATA GetSetBusData;
		/*0000*/_DBGKD_FILL_MEMORY FillMemory;
		/*0000*/_DBGKD_QUERY_MEMORY QueryMemory;
		/*0000*/_DBGKD_SWITCH_PARTITION SwitchPartition;
		/*0000*/_DBGKD_READ_CONTROL_SPACE ControlSpace;
	};
};

C_ASSERT(sizeof(DBGKD_MANIPULATE_STATE)==0x38);

struct _DBGKD_EXCEPTION
{
	/*0000*/_EXCEPTION_RECORD64 ExceptionRecord;
	/*0098*/ULONG FirstChance;
	/*009C*/ULONG pad;
};

struct _DBGKD_LOAD_SYMBOLS
{
	/*0000*/ULONG PathNameLength;
	/*0008*/ULONG64 BaseOfDll;
	/*0010*/ULONG64 ProcessId;
	/*0018*/ULONG CheckSum;
	/*001C*/ULONG SizeOfImage;
	/*0020*/BOOLEAN UnloadSymbols;
};

struct DBGKD_WAIT_STATE_CHANGE : public DBGKD_HEADER 
{
	DWORD NumberProcessors;//8
	ULONG64 Thread;//10
	ULONG64 ProgramCounter;//18 eip,rip
	union
	{
		_DBGKD_EXCEPTION Exception;
		_DBGKD_LOAD_SYMBOLS LoadSymbols;
	};
	/*00c0*/ULONG KernelDr6;
	/*00c4*/ULONG KernelDr7;
	/*00c8*/WORD wc8;
	/*00ca*/WORD wca;
	/*00cc*/BYTE InstructionStream[16];
	/*00DC*/WORD SegCs;
	/*00DE*/WORD SegDs;
	/*00E0*/WORD SegEs;
	/*00E2*/WORD SegFs;
	/*00E4*/ULONG EFlags;
	/*00e8*/DWORD de8;
	/*00ec*/DWORD dec;
	/*00f0*/CHAR Name[ANY_SIZE];
};

//
// File I/O Structure
//
struct DBGKD_CREATE_FILE
{
	ULONG DesiredAccess;
	ULONG FileAttributes;
	ULONG ShareAccess;
	ULONG CreateDisposition;
	ULONG CreateOptions;
	ULONG reserved[9];
	WCHAR Name[];
};

struct DBGKD_READ_FILE
{
	ULONG64 Handle;
	ULONG64 Offset;
	ULONG Length;
};

struct DBGKD_WRITE_FILE
{
	ULONG64 Handle;
	ULONG64 Offset;
	ULONG Length;
};

struct DBGKD_CLOSE_FILE
{
	ULONG64 Handle;
};

struct DBGKD_FILE_IO : DBGKD_HEADER
{
	union
	{
		DBGKD_CREATE_FILE CreateFile;
		DBGKD_READ_FILE ReadFile;
		DBGKD_WRITE_FILE WriteFile;
		DBGKD_CLOSE_FILE CloseFile;
	};
};

struct KD_PACKET_EX : public KD_PACKET 
{
	union
	{
		BYTE m_buffer[PACKET_BYTE_COUNT];
		union
		{
			DBGKD_HEADER m_hd;
			DBGKD_WAIT_STATE_CHANGE m_ws;
			DBGKD_MANIPULATE_STATE m_ms;
			DBGKD_DEBUG_IO m_io;
			DBGKD_FILE_IO m_fi;
		};
	};
};

C_ASSERT(sizeof(KD_PACKET_EX)==PACKET_BYTE_COUNT+sizeof(KD_PACKET));

ULONG KdpComputeChecksum (PUCHAR Buffer, ULONG Length);

