#pragma once
//
// Shutdown types for NtShutdownSystem
//
typedef enum _SHUTDOWN_ACTION
{
    ShutdownNoReboot,
    ShutdownReboot,
    ShutdownPowerOff
} SHUTDOWN_ACTION;

//
// Responses for NtRaiseHardError
//
typedef enum _HARDERROR_RESPONSE_OPTION
{
    OptionAbortRetryIgnore,
    OptionOk,
    OptionOkCancel,
    OptionRetryCancel,
    OptionYesNo,
    OptionYesNoCancel,
    OptionShutdownSystem
} HARDERROR_RESPONSE_OPTION, *PHARDERROR_RESPONSE_OPTION;

typedef enum _HARDERROR_RESPONSE
{
    ResponseReturnToCaller,
    ResponseNotHandled,
    ResponseAbort,
    ResponseCancel,
    ResponseIgnore,
    ResponseNo,
    ResponseOk,
    ResponseRetry,
    ResponseYes,
    ResponseTryAgain,
    ResponseContinue
} HARDERROR_RESPONSE, *PHARDERROR_RESPONSE;

typedef struct _EVENT_BASIC_INFORMATION
{
	EVENT_TYPE EventType;
	LONG EventState;
} EVENT_BASIC_INFORMATION, *PEVENT_BASIC_INFORMATION;

enum EVENT_INFORMATION_CLASS
{
	EventBasicInformation
};

struct DIRECTORY_BASIC_INFORMATION
{
	UNICODE_STRING ObjectName;
	UNICODE_STRING ObjectTypeName;
};

//
//  System Information Classes for NtQuerySystemInformation
//
typedef enum _SYSTEM_INFORMATION_CLASS
{
	SystemBasicInformation = 0x0,
	SystemProcessorInformation = 0x1,
	SystemPerformanceInformation = 0x2,
	SystemTimeOfDayInformation = 0x3,
	SystemPathInformation = 0x4,
	SystemProcessInformation = 0x5,
	SystemCallCountInformation = 0x6,
	SystemDeviceInformation = 0x7,
	SystemProcessorPerformanceInformation = 0x8,
	SystemFlagsInformation = 0x9,
	SystemCallTimeInformation = 0xa,
	SystemModuleInformation = 0xb,
	SystemLocksInformation = 0xc,
	SystemStackTraceInformation = 0xd,
	SystemPagedPoolInformation = 0xe,
	SystemNonPagedPoolInformation = 0xf,
	SystemHandleInformation = 0x10,
	SystemObjectInformation = 0x11,
	SystemPageFileInformation = 0x12,
	SystemVdmInstemulInformation = 0x13,
	SystemVdmBopInformation = 0x14,
	SystemFileCacheInformation = 0x15,
	SystemPoolTagInformation = 0x16,
	SystemInterruptInformation = 0x17,
	SystemDpcBehaviorInformation = 0x18,
	SystemFullMemoryInformation = 0x19,
	SystemLoadGdiDriverInformation = 0x1a,
	SystemUnloadGdiDriverInformation = 0x1b,
	SystemTimeAdjustmentInformation = 0x1c,
	SystemSummaryMemoryInformation = 0x1d,
	SystemMirrorMemoryInformation = 0x1e,
	SystemPerformanceTraceInformation = 0x1f,
	SystemObsolete0 = 0x20,
	SystemExceptionInformation = 0x21,
	SystemCrashDumpStateInformation = 0x22,
	SystemKernelDebuggerInformation = 0x23,
	SystemContextSwitchInformation = 0x24,
	SystemRegistryQuotaInformation = 0x25,
	SystemExtendServiceTableInformation = 0x26,
	SystemPrioritySeperation = 0x27,
	SystemVerifierAddDriverInformation = 0x28,
	SystemVerifierRemoveDriverInformation = 0x29,
	SystemProcessorIdleInformation = 0x2a,
	SystemLegacyDriverInformation = 0x2b,
	SystemCurrentTimeZoneInformation = 0x2c,
	SystemLookasideInformation = 0x2d,
	SystemTimeSlipNotification = 0x2e,
	SystemSessionCreate = 0x2f,
	SystemSessionDetach = 0x30,
	SystemSessionInformation = 0x31,
	SystemRangeStartInformation = 0x32,
	SystemVerifierInformation = 0x33,
	SystemVerifierThunkExtend = 0x34,
	SystemSessionProcessInformation = 0x35,
	SystemLoadGdiDriverInSystemSpace = 0x36,
	SystemNumaProcessorMap = 0x37,
	SystemPrefetcherInformation = 0x38,
	SystemExtendedProcessInformation = 0x39,
	SystemRecommendedSharedDataAlignment = 0x3a,
	SystemComPlusPackage = 0x3b,
	SystemNumaAvailableMemory = 0x3c,
	SystemProcessorPowerInformation = 0x3d,
	SystemEmulationBasicInformation = 0x3e,
	SystemEmulationProcessorInformation = 0x3f,
	SystemExtendedHandleInformation = 0x40,
	SystemLostDelayedWriteInformation = 0x41,
	SystemBigPoolInformation = 0x42,
	SystemSessionPoolTagInformation = 0x43,
	SystemSessionMappedViewInformation = 0x44,
	SystemHotpatchInformation = 0x45,
	SystemObjectSecurityMode = 0x46,
	SystemWatchdogTimerHandler = 0x47,
	SystemWatchdogTimerInformation = 0x48,
	SystemLogicalProcessorInformation = 0x49,
	SystemWow64SharedInformationObsolete = 0x4a,
	SystemRegisterFirmwareTableInformationHandler = 0x4b,
	SystemFirmwareTableInformation = 0x4c,
	SystemModuleInformationEx = 0x4d,
	SystemVerifierTriageInformation = 0x4e,
	SystemSuperfetchInformation = 0x4f,
	SystemMemoryListInformation = 0x50,
	SystemFileCacheInformationEx = 0x51,
	SystemThreadPriorityClientIdInformation = 0x52,
	SystemProcessorIdleCycleTimeInformation = 0x53,
	SystemVerifierCancellationInformation = 0x54,
	SystemProcessorPowerInformationEx = 0x55,
	SystemRefTraceInformation = 0x56,
	SystemSpecialPoolInformation = 0x57,
	SystemProcessIdInformation = 0x58,
	SystemErrorPortInformation = 0x59,
	SystemBootEnvironmentInformation = 0x5a,
	SystemHypervisorInformation = 0x5b,
	SystemVerifierInformationEx = 0x5c,
	SystemTimeZoneInformation = 0x5d,
	SystemImageFileExecutionOptionsInformation = 0x5e,
	SystemCoverageInformation = 0x5f,
	SystemPrefetchPatchInformation = 0x60,
	SystemVerifierFaultsInformation = 0x61,
	SystemSystemPartitionInformation = 0x62,
	SystemSystemDiskInformation = 0x63,
	SystemProcessorPerformanceDistribution = 0x64,
	SystemNumaProximityNodeInformation = 0x65,
	SystemDynamicTimeZoneInformation = 0x66,
	SystemCodeIntegrityInformation = 0x67,
	SystemProcessorMicrocodeUpdateInformation = 0x68,
	SystemProcessorBrandString = 0x69,
	SystemVirtualAddressInformation = 0x6a,
	SystemLogicalProcessorAndGroupInformation = 0x6b,
	SystemProcessorCycleTimeInformation = 0x6c,
	SystemStoreInformation = 0x6d,
	SystemRegistryAppendString = 0x6e,
	SystemAitSamplingValue = 0x6f,
	SystemVhdBootInformation = 0x70,
	SystemCpuQuotaInformation = 0x71,
	SystemNativeBasicInformation = 0x72,
	SystemErrorPortTimeouts = 0x73,
	SystemLowPriorityIoInformation = 0x74,
	SystemBootEntropyInformation = 0x75,
	SystemVerifierCountersInformation = 0x76,
	SystemPagedPoolInformationEx = 0x77,
	SystemSystemPtesInformationEx = 0x78,
	SystemNodeDistanceInformation = 0x79,
	SystemAcpiAuditInformation = 0x7a,
	SystemBasicPerformanceInformation = 0x7b,
	SystemQueryPerformanceCounterInformation = 0x7c,
	SystemSessionBigPoolInformation = 0x7d,
	SystemBootGraphicsInformation = 0x7e,
	SystemScrubPhysicalMemoryInformation = 0x7f,
	SystemBadPageInformation = 0x80,
	SystemProcessorProfileControlArea = 0x81,
	SystemCombinePhysicalMemoryInformation = 0x82,
	SystemEntropyInterruptTimingInformation = 0x83,
	SystemConsoleInformation = 0x84,
	SystemPlatformBinaryInformation = 0x85,
	SystemThrottleNotificationInformation = 0x86,
	SystemHypervisorProcessorCountInformation = 0x87,
	SystemDeviceDataInformation = 0x88,
	SystemDeviceDataEnumerationInformation = 0x89,
	SystemMemoryTopologyInformation = 0x8a,
	SystemMemoryChannelInformation = 0x8b,
	SystemBootLogoInformation = 0x8c,
	SystemProcessorPerformanceInformationEx = 0x8d,
	SystemSpare0 = 0x8e,
	SystemSecureBootPolicyInformation = 0x8f,
	SystemPageFileInformationEx = 0x90,
	SystemSecureBootInformation = 0x91,
	SystemEntropyInterruptTimingRawInformation = 0x92,
	SystemPortableWorkspaceEfiLauncherInformation = 0x93,
	SystemFullProcessInformation = 0x94,
	SystemKernelDebuggerInformationEx = 0x95,
	SystemBootMetadataInformation = 0x96,
	SystemSoftRebootInformation = 0x97,
	SystemElamCertificateInformation = 0x98,
	SystemOfflineDumpConfigInformation = 0x99,
	SystemProcessorFeaturesInformation = 0x9a,
	SystemRegistryReconciliationInformation = 0x9b,
	MaxSystemInfoClass = 0x9c
} SYSTEM_INFORMATION_CLASS;

//
// Information Structures for NtQuerySystemInformation
//
typedef struct _SYSTEM_BASIC_INFORMATION
{
	ULONG Reserved;
	ULONG TimerResolution;
	ULONG PageSize;
	ULONG NumberOfPhysicalPages;
	ULONG LowestPhysicalPageNumber;
	ULONG HighestPhysicalPageNumber;
	ULONG AllocationGranularity;
	ULONG_PTR MinimumUserModeAddress;
	ULONG_PTR MaximumUserModeAddress;
	ULONG_PTR ActiveProcessorsAffinityMask;
	CCHAR NumberOfProcessors;
} SYSTEM_BASIC_INFORMATION, *PSYSTEM_BASIC_INFORMATION;

// Class 1
typedef struct _SYSTEM_PROCESSOR_INFORMATION
{
	USHORT ProcessorArchitecture;
	USHORT ProcessorLevel;
	USHORT ProcessorRevision;
	USHORT Reserved;
	ULONG ProcessorFeatureBits;
} SYSTEM_PROCESSOR_INFORMATION, *PSYSTEM_PROCESSOR_INFORMATION;

// Class 2
typedef struct _SYSTEM_PERFORMANCE_INFORMATION
{
	LARGE_INTEGER IdleProcessTime;
	LARGE_INTEGER IoReadTransferCount;
	LARGE_INTEGER IoWriteTransferCount;
	LARGE_INTEGER IoOtherTransferCount;
	ULONG IoReadOperationCount;
	ULONG IoWriteOperationCount;
	ULONG IoOtherOperationCount;
	ULONG AvailablePages;
	ULONG CommittedPages;
	ULONG CommitLimit;
	ULONG PeakCommitment;
	ULONG PageFaultCount;
	ULONG CopyOnWriteCount;
	ULONG TransitionCount;
	ULONG CacheTransitionCount;
	ULONG DemandZeroCount;
	ULONG PageReadCount;
	ULONG PageReadIoCount;
	ULONG CacheReadCount;
	ULONG CacheIoCount;
	ULONG DirtyPagesWriteCount;
	ULONG DirtyWriteIoCount;
	ULONG MappedPagesWriteCount;
	ULONG MappedWriteIoCount;
	ULONG PagedPoolPages;
	ULONG NonPagedPoolPages;
	ULONG PagedPoolAllocs;
	ULONG PagedPoolFrees;
	ULONG NonPagedPoolAllocs;
	ULONG NonPagedPoolFrees;
	ULONG FreeSystemPtes;
	ULONG ResidentSystemCodePage;
	ULONG TotalSystemDriverPages;
	ULONG TotalSystemCodePages;
	ULONG NonPagedPoolLookasideHits;
	ULONG PagedPoolLookasideHits;
	ULONG Spare3Count;
	ULONG ResidentSystemCachePage;
	ULONG ResidentPagedPoolPage;
	ULONG ResidentSystemDriverPage;
	ULONG CcFastReadNoWait;
	ULONG CcFastReadWait;
	ULONG CcFastReadResourceMiss;
	ULONG CcFastReadNotPossible;
	ULONG CcFastMdlReadNoWait;
	ULONG CcFastMdlReadWait;
	ULONG CcFastMdlReadResourceMiss;
	ULONG CcFastMdlReadNotPossible;
	ULONG CcMapDataNoWait;
	ULONG CcMapDataWait;
	ULONG CcMapDataNoWaitMiss;
	ULONG CcMapDataWaitMiss;
	ULONG CcPinMappedDataCount;
	ULONG CcPinReadNoWait;
	ULONG CcPinReadWait;
	ULONG CcPinReadNoWaitMiss;
	ULONG CcPinReadWaitMiss;
	ULONG CcCopyReadNoWait;
	ULONG CcCopyReadWait;
	ULONG CcCopyReadNoWaitMiss;
	ULONG CcCopyReadWaitMiss;
	ULONG CcMdlReadNoWait;
	ULONG CcMdlReadWait;
	ULONG CcMdlReadNoWaitMiss;
	ULONG CcMdlReadWaitMiss;
	ULONG CcReadAheadIos;
	ULONG CcLazyWriteIos;
	ULONG CcLazyWritePages;
	ULONG CcDataFlushes;
	ULONG CcDataPages;
	ULONG ContextSwitches;
	ULONG FirstLevelTbFills;
	ULONG SecondLevelTbFills;
	ULONG SystemCalls;
} SYSTEM_PERFORMANCE_INFORMATION, *PSYSTEM_PERFORMANCE_INFORMATION;

// Class 3
typedef struct _SYSTEM_TIMEOFDAY_INFORMATION
{
	LARGE_INTEGER BootTime;
	LARGE_INTEGER CurrentTime;
	LARGE_INTEGER TimeZoneBias;
	ULONG TimeZoneId;
	ULONG Reserved;
	LARGE_INTEGER BootTimeBias;
	LARGE_INTEGER SleepTimeBias;
} SYSTEM_TIMEOFDAY_INFORMATION, *PSYSTEM_TIMEOFDAY_INFORMATION;

// Class 4
// This class is obsolete, please use KUSER_SHARED_DATA instead

// Class 5
enum THREAD_STATE
{
	StateInitialized,
	StateReady,
	StateRunning,
	StateStandby,
	StateTerminated,
	StateWait,
	StateTransition
};


typedef struct _SYSTEM_THREAD_INFORMATION
{
	LARGE_INTEGER KernelTime;
	LARGE_INTEGER UserTime;
	LARGE_INTEGER CreateTime;
	ULONG WaitTime;
	PVOID StartAddress;
	CLIENT_ID ClientId;
	KPRIORITY Priority;
	LONG BasePriority;
	ULONG ContextSwitches;
	THREAD_STATE ThreadState;
	KWAIT_REASON WaitReason;
} SYSTEM_THREAD_INFORMATION, *PSYSTEM_THREAD_INFORMATION;

typedef struct SYSTEM_EXTENDED_THREAD_INFORMATION : SYSTEM_THREAD_INFORMATION
{
	PVOID StackBase;
	PVOID StackLimit;
	PVOID Win32StartAddress;
	PVOID TebAddress;
	PVOID Reserved[3];
} *PSYSTEM_EXTENDED_THREAD_INFORMATION;

typedef struct _SYSTEM_PROCESS_INFORMATION
{
	ULONG NextEntryOffset;
	ULONG NumberOfThreads;
	LARGE_INTEGER SpareLi1;
	LARGE_INTEGER SpareLi2;
	LARGE_INTEGER SpareLi3;
	LARGE_INTEGER CreateTime;
	LARGE_INTEGER UserTime;
	LARGE_INTEGER KernelTime;
	UNICODE_STRING ImageName;
	KPRIORITY BasePriority;
	HANDLE UniqueProcessId;
	HANDLE InheritedFromUniqueProcessId;
	ULONG HandleCount;
	ULONG SessionId;
	ULONG_PTR PageDirectoryBase;

	//
	// This part corresponds to VM_COUNTERS_EX.
	// NOTE: *NOT* THE SAME AS VM_COUNTERS!
	//
	SIZE_T PeakVirtualSize;
	SIZE_T VirtualSize;
	ULONG PageFaultCount;
	SIZE_T PeakWorkingSetSize;
	SIZE_T WorkingSetSize;
	SIZE_T QuotaPeakPagedPoolUsage;
	SIZE_T QuotaPagedPoolUsage;
	SIZE_T QuotaPeakNonPagedPoolUsage;
	SIZE_T QuotaNonPagedPoolUsage;
	SIZE_T PagefileUsage;
	SIZE_T PeakPagefileUsage;
	SIZE_T PrivatePageCount;

	//
	// This part corresponds to IO_COUNTERS
	//
	LARGE_INTEGER ReadOperationCount;
	LARGE_INTEGER WriteOperationCount;
	LARGE_INTEGER OtherOperationCount;
	LARGE_INTEGER ReadTransferCount;
	LARGE_INTEGER WriteTransferCount;
	LARGE_INTEGER OtherTransferCount;

	SYSTEM_EXTENDED_THREAD_INFORMATION TH[];
} SYSTEM_PROCESS_INFORMATION, *PSYSTEM_PROCESS_INFORMATION;

// Class 6
typedef struct _SYSTEM_CALL_COUNT_INFORMATION
{
	ULONG Length;
	ULONG NumberOfTables;
} SYSTEM_CALL_COUNT_INFORMATION, *PSYSTEM_CALL_COUNT_INFORMATION;

// Class 7
typedef struct _SYSTEM_DEVICE_INFORMATION
{
	ULONG NumberOfDisks;
	ULONG NumberOfFloppies;
	ULONG NumberOfCdRoms;
	ULONG NumberOfTapes;
	ULONG NumberOfSerialPorts;
	ULONG NumberOfParallelPorts;
} SYSTEM_DEVICE_INFORMATION, *PSYSTEM_DEVICE_INFORMATION;

// Class 8
typedef struct _SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION
{
	LARGE_INTEGER IdleTime;
	LARGE_INTEGER KernelTime;
	LARGE_INTEGER UserTime;
	LARGE_INTEGER DpcTime;
	LARGE_INTEGER InterruptTime;
	ULONG InterruptCount;
} SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION, *PSYSTEM_PROCESSOR_PERFORMANCE_INFORMATION;

// Class 9
typedef struct _SYSTEM_FLAGS_INFORMATION
{
	ULONG Flags;
} SYSTEM_FLAGS_INFORMATION, *PSYSTEM_FLAGS_INFORMATION;

// Class 10
typedef struct _SYSTEM_CALL_TIME_INFORMATION
{
	ULONG Length;
	ULONG TotalCalls;
	LARGE_INTEGER TimeOfCalls[];
} SYSTEM_CALL_TIME_INFORMATION, *PSYSTEM_CALL_TIME_INFORMATION;

typedef struct RTL_PROCESS_MODULE_INFORMATION {
	HANDLE Section;                 // Not filled in
	PVOID MappedBase;
	PVOID ImageBase;
	ULONG ImageSize;
	ULONG Flags;
	USHORT LoadOrderIndex;
	USHORT InitOrderIndex;
	USHORT LoadCount;
	USHORT OffsetToFileName;
	CHAR  FullPathName[ 256 ];
} *PRTL_PROCESS_MODULE_INFORMATION;

typedef struct RTL_PROCESS_MODULES {
	ULONG NumberOfModules;
	RTL_PROCESS_MODULE_INFORMATION Modules[];
} *PRTL_PROCESS_MODULES;

typedef struct RTL_PROCESS_MODULE_INFORMATION32 {
	ULONG Section;                 // Not filled in
	ULONG MappedBase;
	ULONG ImageBase;
	ULONG ImageSize;
	ULONG Flags;
	USHORT LoadOrderIndex;
	USHORT InitOrderIndex;
	USHORT LoadCount;
	USHORT OffsetToFileName;
	CHAR  FullPathName[ 256 ];
} *PRTL_PROCESS_MODULE_INFORMATION32;

typedef struct RTL_PROCESS_MODULES32 {
	ULONG NumberOfModules;
	RTL_PROCESS_MODULE_INFORMATION32 Modules[];
} *PRTL_PROCESS_MODULES32;

typedef struct _RTL_PROCESS_LOCK_INFORMATION {
	PVOID Address;
	USHORT Type;
	USHORT CreatorBackTraceIndex;

	HANDLE OwningThread;        // from the thread's ClientId->UniqueThread
	LONG LockCount;
	ULONG ContentionCount;
	ULONG EntryCount;

	//
	// The following fields are only valid for Type == RTL_CRITSECT_TYPE
	//

	LONG RecursionCount;

	//
	// The following fields are only valid for Type == RTL_RESOURCE_TYPE
	//

	ULONG NumberOfWaitingShared;
	ULONG NumberOfWaitingExclusive;
} RTL_PROCESS_LOCK_INFORMATION, *PRTL_PROCESS_LOCK_INFORMATION;


typedef struct _RTL_PROCESS_LOCKS {
	ULONG NumberOfLocks;
	RTL_PROCESS_LOCK_INFORMATION Locks[ 1 ];
} RTL_PROCESS_LOCKS, *PRTL_PROCESS_LOCKS;

#define MAX_STACK_DEPTH 32

typedef struct _RTL_PROCESS_BACKTRACE_INFORMATION {
	PCHAR SymbolicBackTrace;        // Not filled in
	ULONG TraceCount;
	USHORT Index;
	USHORT Depth;
	PVOID BackTrace[ MAX_STACK_DEPTH ];
} RTL_PROCESS_BACKTRACE_INFORMATION, *PRTL_PROCESS_BACKTRACE_INFORMATION;

typedef struct _RTL_PROCESS_BACKTRACES {
	ULONG CommittedMemory;
	ULONG ReservedMemory;
	ULONG NumberOfBackTraceLookups;
	ULONG NumberOfBackTraces;
	RTL_PROCESS_BACKTRACE_INFORMATION BackTraces[ 1 ];
} RTL_PROCESS_BACKTRACES, *PRTL_PROCESS_BACKTRACES;

typedef struct _RTL_HEAP_ENTRY {
	SIZE_T Size;
	USHORT Flags;
	USHORT AllocatorBackTraceIndex;
	union {
		struct {
			SIZE_T Settable;
			ULONG Tag;
		} s1;   // All other heap entries
		struct {
			SIZE_T CommittedSize;
			PVOID FirstBlock;
		} s2;   // RTL_SEGMENT
	} u;
} RTL_HEAP_ENTRY, *PRTL_HEAP_ENTRY;

#define RTL_HEAP_BUSY               (USHORT)0x0001
#define RTL_HEAP_SEGMENT            (USHORT)0x0002
#define RTL_HEAP_SETTABLE_VALUE     (USHORT)0x0010
#define RTL_HEAP_SETTABLE_FLAG1     (USHORT)0x0020
#define RTL_HEAP_SETTABLE_FLAG2     (USHORT)0x0040
#define RTL_HEAP_SETTABLE_FLAG3     (USHORT)0x0080
#define RTL_HEAP_SETTABLE_FLAGS     (USHORT)0x00E0
#define RTL_HEAP_UNCOMMITTED_RANGE  (USHORT)0x0100
#define RTL_HEAP_PROTECTED_ENTRY    (USHORT)0x0200

typedef struct _RTL_HEAP_TAG {
	ULONG NumberOfAllocations;
	ULONG NumberOfFrees;
	SIZE_T BytesAllocated;
	USHORT TagIndex;
	USHORT CreatorBackTraceIndex;
	WCHAR TagName[ 24 ];
} RTL_HEAP_TAG, *PRTL_HEAP_TAG;

typedef struct _RTL_HEAP_INFORMATION {
	PVOID BaseAddress;
	ULONG Flags;
	USHORT EntryOverhead;
	USHORT CreatorBackTraceIndex;
	SIZE_T BytesAllocated;
	SIZE_T BytesCommitted;
	ULONG NumberOfTags;
	ULONG NumberOfEntries;
	ULONG NumberOfPseudoTags;
	ULONG PseudoTagGranularity;
	ULONG Reserved[ 5 ];
	PRTL_HEAP_TAG Tags;
	PRTL_HEAP_ENTRY Entries;
} RTL_HEAP_INFORMATION, *PRTL_HEAP_INFORMATION;

typedef struct _RTL_PROCESS_HEAPS {
	ULONG NumberOfHeaps;
	RTL_HEAP_INFORMATION Heaps[ 1 ];
} RTL_PROCESS_HEAPS, *PRTL_PROCESS_HEAPS;

//
// Debugging support
//

typedef struct _RTL_DEBUG_INFORMATION {
	HANDLE SectionHandleClient;
	PVOID ViewBaseClient;
	PVOID ViewBaseTarget;
	ULONG_PTR ViewBaseDelta;
	HANDLE EventPairClient;
	HANDLE EventPairTarget;
	HANDLE TargetProcessId;
	HANDLE TargetThreadHandle;
	ULONG Flags;
	SIZE_T OffsetFree;
	SIZE_T CommitSize;
	SIZE_T ViewSize;
	PRTL_PROCESS_MODULES Modules;
	PRTL_PROCESS_BACKTRACES BackTraces;
	PRTL_PROCESS_HEAPS Heaps;
	PRTL_PROCESS_LOCKS Locks;
	PVOID SpecificHeap;
	HANDLE TargetProcessHandle;
	PVOID Reserved[ 6 ];
} RTL_DEBUG_INFORMATION, *PRTL_DEBUG_INFORMATION;

#define RTL_QUERY_PROCESS_MODULES       0x00000001
#define RTL_QUERY_PROCESS_BACKTRACES    0x00000002
#define RTL_QUERY_PROCESS_HEAP_SUMMARY  0x00000004
#define RTL_QUERY_PROCESS_HEAP_TAGS     0x00000008
#define RTL_QUERY_PROCESS_HEAP_ENTRIES  0x00000010
#define RTL_QUERY_PROCESS_LOCKS         0x00000020
#define RTL_QUERY_PROCESS_MODULES32     0x00000040
#define RTL_QUERY_PROCESS_NONINVASIVE   0x80000000

struct MEMORY_WORKING_SET_LIST
{
	ULONG	NumberOfPages;
	ULONG_PTR	WorkingSetList[0];
};

#define  LDRP_STATIC_LINK				0x00000002
#define  LDRP_IMAGE_DLL					0x00000004
#define  LDRP_LOAD_IN_PROGRESS			0x00001000
#define  LDRP_UNLOAD_IN_PROGRES			0x00002000
#define  LDRP_ENTRY_PROCESSED			0x00004000
#define  LDRP_ENTRY_INSERTED			0x00008000
#define  LDRP_CURRENT_LOAD				0x00010000
#define  LDRP_FAIL_BUILTIN_LOAD			0x00020000
#define  LDRP_DONT_CALL_FOR_THREADS		0x00040000
#define  LDRP_PROCESS_ATTACH_CALLED		0x00080000
#define  LDRP_DEBUG_SYMBOL_LOADED		0x00100000
#define	 LDRP_IMAGE_NOT_AT_BASE			0x00200000
#define	 LDRP_IMAGE_CLR					0x00400000
#define	 LDRP_IMAGE_REDIRECTED			0x10000000

enum WSLE
{
	WSLE_READONLY = 1,
	WSLE_EXECUTE,
	WSLE_EXECUTE_READ,
	WSLE_READWRITE,
	WSLE_WRITECOPY,
	WSLE_EXECUTE_READWRITE,
	WSLE_EXECUTE_WRITECOPY,
	WSLE_PROTECT_MASK = 7,
	WSLE_SHARE_COUNT_MASK = 0xe0,
	WSLE_SHARE_COUNT_SHIFT = 5,
	WSLE_SHAREABLE = 0x100
};

// Class 12 - See RTL_PROCESS_LOCKS

// Class 13 - See RTL_PROCESS_BACKTRACES

// Class 14 - 15
typedef struct _SYSTEM_POOL_ENTRY
{
	BOOLEAN Allocated;
	BOOLEAN Spare0;
	USHORT AllocatorBackTraceIndex;
	ULONG Size;
	union
	{
		UCHAR Tag[4];
		ULONG TagUlong;
		PVOID ProcessChargedQuota;
	};
} SYSTEM_POOL_ENTRY, *PSYSTEM_POOL_ENTRY;

typedef struct _SYSTEM_POOL_INFORMATION
{
	ULONG TotalSize;
	PVOID FirstEntry;
	USHORT EntryOverhead;
	BOOLEAN PoolTagPresent;
	BOOLEAN Spare0;
	ULONG NumberOfEntries;
	SYSTEM_POOL_ENTRY Entries[];
} SYSTEM_POOL_INFORMATION, *PSYSTEM_POOL_INFORMATION;

// Class 16
typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO
{
	USHORT UniqueProcessId;
	USHORT CreatorBackTraceIndex;
	UCHAR ObjectTypeIndex;
	UCHAR HandleAttributes;
	USHORT HandleValue;
	PVOID Object;
	ULONG GrantedAccess;
} SYSTEM_HANDLE_TABLE_ENTRY_INFO, *PSYSTEM_HANDLE_TABLE_ENTRY_INFO;

typedef struct _SYSTEM_HANDLE_INFORMATION
{
	ULONG NumberOfHandles;
	SYSTEM_HANDLE_TABLE_ENTRY_INFO Handles[];
} SYSTEM_HANDLE_INFORMATION, *PSYSTEM_HANDLE_INFORMATION;

typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX {
	PVOID Object;
	ULONG_PTR UniqueProcessId;
	ULONG_PTR HandleValue;
	ULONG GrantedAccess;
	USHORT CreatorBackTraceIndex;
	USHORT ObjectTypeIndex;
	ULONG  HandleAttributes;
	ULONG  Reserved;
} SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX, *PSYSTEM_HANDLE_TABLE_ENTRY_INFO_EX;

typedef struct _SYSTEM_HANDLE_INFORMATION_EX {
	ULONG_PTR NumberOfHandles;
	ULONG_PTR Reserved;
	SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX Handles[  ];
} SYSTEM_HANDLE_INFORMATION_EX, *PSYSTEM_HANDLE_INFORMATION_EX;
// Class 17
typedef struct _SYSTEM_OBJECTTYPE_INFORMATION
{
	ULONG NextEntryOffset;
	ULONG NumberOfObjects;
	ULONG NumberOfHandles;
	ULONG TypeIndex;
	ULONG InvalidAttributes;
	GENERIC_MAPPING GenericMapping;
	ULONG ValidAccessMask;
	ULONG PoolType;
	BOOLEAN SecurityRequired;
	BOOLEAN WaitableObject;
	UNICODE_STRING TypeName;
} SYSTEM_OBJECTTYPE_INFORMATION, *PSYSTEM_OBJECTTYPE_INFORMATION;

typedef struct _SYSTEM_OBJECT_INFORMATION
{
	ULONG NextEntryOffset;
	PVOID Object;
	HANDLE CreatorUniqueProcess;
	USHORT CreatorBackTraceIndex;
	USHORT Flags;
	LONG PointerCount;
	LONG HandleCount;
	ULONG PagedPoolCharge;
	ULONG NonPagedPoolCharge;
	HANDLE ExclusiveProcessId;
	PVOID SecurityDescriptor;
	OBJECT_NAME_INFORMATION NameInfo;
} SYSTEM_OBJECT_INFORMATION, *PSYSTEM_OBJECT_INFORMATION;

// Class 18
typedef struct _SYSTEM_PAGEFILE_INFORMATION
{
	ULONG NextEntryOffset;
	ULONG TotalSize;
	ULONG TotalInUse;
	ULONG PeakUsage;
	UNICODE_STRING PageFileName;
} SYSTEM_PAGEFILE_INFORMATION, *PSYSTEM_PAGEFILE_INFORMATION;

// Class 19
typedef struct _SYSTEM_VDM_INSTEMUL_INFO
{
	ULONG SegmentNotPresent;
	ULONG VdmOpcode0F;
	ULONG OpcodeESPrefix;
	ULONG OpcodeCSPrefix;
	ULONG OpcodeSSPrefix;
	ULONG OpcodeDSPrefix;
	ULONG OpcodeFSPrefix;
	ULONG OpcodeGSPrefix;
	ULONG OpcodeOPER32Prefix;
	ULONG OpcodeADDR32Prefix;
	ULONG OpcodeINSB;
	ULONG OpcodeINSW;
	ULONG OpcodeOUTSB;
	ULONG OpcodeOUTSW;
	ULONG OpcodePUSHF;
	ULONG OpcodePOPF;
	ULONG OpcodeINTnn;
	ULONG OpcodeINTO;
	ULONG OpcodeIRET;
	ULONG OpcodeINBimm;
	ULONG OpcodeINWimm;
	ULONG OpcodeOUTBimm;
	ULONG OpcodeOUTWimm ;
	ULONG OpcodeINB;
	ULONG OpcodeINW;
	ULONG OpcodeOUTB;
	ULONG OpcodeOUTW;
	ULONG OpcodeLOCKPrefix;
	ULONG OpcodeREPNEPrefix;
	ULONG OpcodeREPPrefix;
	ULONG OpcodeHLT;
	ULONG OpcodeCLI;
	ULONG OpcodeSTI;
	ULONG BopCount;
} SYSTEM_VDM_INSTEMUL_INFO, *PSYSTEM_VDM_INSTEMUL_INFO;

// Class 20 - ULONG VDMBOPINFO

// Class 21
typedef struct _SYSTEM_FILECACHE_INFORMATION
{
	ULONG CurrentSize;
	ULONG PeakSize;
	ULONG PageFaultCount;
	ULONG MinimumWorkingSet;
	ULONG MaximumWorkingSet;
	ULONG CurrentSizeIncludingTransitionInPages;
	ULONG PeakSizeIncludingTransitionInPages;
	ULONG TransitionRePurposeCount;
	ULONG Flags;
} SYSTEM_FILECACHE_INFORMATION, *PSYSTEM_FILECACHE_INFORMATION;

// Class 22
typedef struct _SYSTEM_POOLTAG
{
	union
	{
		UCHAR Tag[4];
		ULONG TagUlong;
	};
	ULONG PagedAllocs;
	ULONG PagedFrees;
	ULONG PagedUsed;
	ULONG NonPagedAllocs;
	ULONG NonPagedFrees;
	ULONG NonPagedUsed;
} SYSTEM_POOLTAG, *PSYSTEM_POOLTAG;
typedef struct _SYSTEM_POOLTAG_INFORMATION
{
	ULONG Count;
	SYSTEM_POOLTAG TagInfo[];
} SYSTEM_POOLTAG_INFORMATION, *PSYSTEM_POOLTAG_INFORMATION;

// Class 23
typedef struct _SYSTEM_INTERRUPT_INFORMATION
{
	ULONG ContextSwitches;
	ULONG DpcCount;
	ULONG DpcRate;
	ULONG TimeIncrement;
	ULONG DpcBypassCount;
	ULONG ApcBypassCount;
} SYSTEM_INTERRUPT_INFORMATION, *PSYSTEM_INTERRUPT_INFORMATION;

// Class 24
typedef struct _SYSTEM_DPC_BEHAVIOR_INFORMATION
{
	ULONG Spare;
	ULONG DpcQueueDepth;
	ULONG MinimumDpcRate;
	ULONG AdjustDpcThreshold;
	ULONG IdealDpcRate;
} SYSTEM_DPC_BEHAVIOR_INFORMATION, *PSYSTEM_DPC_BEHAVIOR_INFORMATION;

// Class 25
typedef struct _SYSTEM_MEMORY_INFO
{
	PUCHAR StringOffset;
	USHORT ValidCount;
	USHORT TransitionCount;
	USHORT ModifiedCount;
	USHORT PageTableCount;
} SYSTEM_MEMORY_INFO, *PSYSTEM_MEMORY_INFO;

typedef struct _SYSTEM_MEMORY_INFORMATION
{
	ULONG InfoSize;
	ULONG StringStart;
	SYSTEM_MEMORY_INFO Memory[];
} SYSTEM_MEMORY_INFORMATION, *PSYSTEM_MEMORY_INFORMATION;

// Class 26
typedef struct _SYSTEM_GDI_DRIVER_INFORMATION
{
	UNICODE_STRING DriverName;
	PVOID ImageAddress;
	PVOID SectionPointer;
	PVOID EntryPoint;
	PIMAGE_EXPORT_DIRECTORY ExportSectionPointer;
	ULONG ImageLength;
} SYSTEM_GDI_DRIVER_INFORMATION, *PSYSTEM_GDI_DRIVER_INFORMATION;

// Class 27
// Not an actually class, simply a PVOID to the SectionPointer

// Class 28
typedef struct _SYSTEM_QUERY_TIME_ADJUST_INFORMATION
{
	ULONG TimeAdjustment;
	ULONG TimeIncrement;
	BOOLEAN Enable;
} SYSTEM_QUERY_TIME_ADJUST_INFORMATION, *PSYSTEM_QUERY_TIME_ADJUST_INFORMATION;

typedef struct _SYSTEM_SET_TIME_ADJUST_INFORMATION
{
	ULONG TimeAdjustment;
	BOOLEAN Enable;
} SYSTEM_SET_TIME_ADJUST_INFORMATION, *PSYSTEM_SET_TIME_ADJUST_INFORMATION;

// Class 29 - Same as 25

// FIXME: Class 30

// Class 31
typedef struct _SYSTEM_REF_TRACE_INFORMATION
{
	UCHAR TraceEnable;
	UCHAR TracePermanent;
	UNICODE_STRING TraceProcessName;
	UNICODE_STRING TracePoolTags;
} SYSTEM_REF_TRACE_INFORMATION, *PSYSTEM_REF_TRACE_INFORMATION;

// Class 32 - OBSOLETE

// Class 33
typedef struct _SYSTEM_EXCEPTION_INFORMATION
{
	ULONG AlignmentFixupCount;
	ULONG ExceptionDispatchCount;
	ULONG FloatingEmulationCount;
	ULONG ByteWordEmulationCount;
} SYSTEM_EXCEPTION_INFORMATION, *PSYSTEM_EXCEPTION_INFORMATION;

// Class 34
typedef struct _SYSTEM_CRASH_STATE_INFORMATION
{
	ULONG ValidCrashDump;
} SYSTEM_CRASH_STATE_INFORMATION, *PSYSTEM_CRASH_STATE_INFORMATION;

// Class 35
typedef struct _SYSTEM_KERNEL_DEBUGGER_INFORMATION
{
	BOOLEAN KernelDebuggerEnabled;
	BOOLEAN KernelDebuggerNotPresent;
} SYSTEM_KERNEL_DEBUGGER_INFORMATION, *PSYSTEM_KERNEL_DEBUGGER_INFORMATION;

// Class 36
typedef struct _SYSTEM_CONTEXT_SWITCH_INFORMATION
{
	ULONG ContextSwitches;
	ULONG FindAny;
	ULONG FindLast;
	ULONG FindIdeal;
	ULONG IdleAny;
	ULONG IdleCurrent;
	ULONG IdleLast;
	ULONG IdleIdeal;
	ULONG PreemptAny;
	ULONG PreemptCurrent;
	ULONG PreemptLast;
	ULONG SwitchToIdle;
} SYSTEM_CONTEXT_SWITCH_INFORMATION, *PSYSTEM_CONTEXT_SWITCH_INFORMATION;

// Class 37
typedef struct _SYSTEM_REGISTRY_QUOTA_INFORMATION
{
	ULONG RegistryQuotaAllowed;
	ULONG RegistryQuotaUsed;
	ULONG PagedPoolSize;
} SYSTEM_REGISTRY_QUOTA_INFORMATION, *PSYSTEM_REGISTRY_QUOTA_INFORMATION;

// Class 38
// Not a structure, simply send the UNICODE_STRING

// Class 39
// Not a structure, simply send a ULONG containing the new separation

// Class 40
//typedef struct _SYSTEM_PLUGPLAY_BUS_INFORMATION
//{
//	ULONG BusCount;
//	PLUGPLAY_BUS_INSTANCE BusInstance[];
//} SYSTEM_PLUGPLAY_BUS_INFORMATION, *PSYSTEM_PLUGPLAY_BUS_INFORMATION;

// Class 41
//typedef struct _SYSTEM_DOCK_INFORMATION
//{
//	SYSTEM_DOCK_STATE DockState;
//	INTERFACE_TYPE DeviceBusType;
//	ULONG DeviceBusNumber;
//	ULONG SlotNumber;
//} SYSTEM_DOCK_INFORMATION, *PSYSTEM_DOCK_INFORMATION;

// Class 42
typedef struct _SYSTEM_POWER_INFORMATION_NATIVE
{
	BOOLEAN SystemSuspendSupported;
	BOOLEAN SystemHibernateSupported;
	BOOLEAN ResumeTimerSupportsSuspend;
	BOOLEAN ResumeTimerSupportsHibernate;
	BOOLEAN LidSupported;
	BOOLEAN TurboSettingSupported;
	BOOLEAN TurboMode;
	BOOLEAN SystemAcOrDc;
	BOOLEAN PowerDownDisabled;
	LARGE_INTEGER SpindownDrives;
} SYSTEM_POWER_INFORMATION_NATIVE, *PSYSTEM_POWER_INFORMATION_NATIVE;

// Class 43
//typedef struct _SYSTEM_LEGACY_DRIVER_INFORMATION
//{
//	PNP_VETO_TYPE VetoType;
//	UNICODE_STRING VetoDriver;
//	// CHAR Buffer[0];
//} SYSTEM_LEGACY_DRIVER_INFORMATION, *PSYSTEM_LEGACY_DRIVER_INFORMATION;

// Class 44
//typedef struct _TIME_ZONE_INFORMATION RTL_TIME_ZONE_INFORMATION;

// Class 45
typedef struct _SYSTEM_LOOKASIDE_INFORMATION
{
	USHORT CurrentDepth;
	USHORT MaximumDepth;
	ULONG TotalAllocates;
	ULONG AllocateMisses;
	ULONG TotalFrees;
	ULONG FreeMisses;
	ULONG Type;
	ULONG Tag;
	ULONG Size;
} SYSTEM_LOOKASIDE_INFORMATION, *PSYSTEM_LOOKASIDE_INFORMATION;

// Class 46
// Not a structure. Only a HANDLE for the SlipEvent;

// Class 47
// Not a structure. Only a ULONG for the SessionId;

// Class 48
// Not a structure. Only a ULONG for the SessionId;

// FIXME: Class 49

// Class 50
// Not a structure. Only a ULONG_PTR for the SystemRangeStart

// Class 51
typedef struct _SYSTEM_VERIFIER_INFORMATION
{
	ULONG NextEntryOffset;
	ULONG Level;
	UNICODE_STRING DriverName;
	ULONG RaiseIrqls;
	ULONG AcquireSpinLocks;
	ULONG SynchronizeExecutions;
	ULONG AllocationsAttempted;
	ULONG AllocationsSucceeded;
	ULONG AllocationsSucceededSpecialPool;
	ULONG AllocationsWithNoTag;
	ULONG TrimRequests;
	ULONG Trims;
	ULONG AllocationsFailed;
	ULONG AllocationsFailedDeliberately;
	ULONG Loads;
	ULONG Unloads;
	ULONG UnTrackedPool;
	ULONG CurrentPagedPoolAllocations;
	ULONG CurrentNonPagedPoolAllocations;
	ULONG PeakPagedPoolAllocations;
	ULONG PeakNonPagedPoolAllocations;
	ULONG PagedPoolUsageInBytes;
	ULONG NonPagedPoolUsageInBytes;
	ULONG PeakPagedPoolUsageInBytes;
	ULONG PeakNonPagedPoolUsageInBytes;
} SYSTEM_VERIFIER_INFORMATION, *PSYSTEM_VERIFIER_INFORMATION;

// FIXME: Class 52

// Class 53
typedef struct _SYSTEM_SESSION_PROCESS_INFORMATION
{
	ULONG SessionId;
	ULONG SizeOfBuf;
	PVOID Buffer; // Same format as in SystemProcessInformation
} SYSTEM_SESSION_PROCESS_INFORMATION, *PSYSTEM_SESSION_PROCESS_INFORMATION;

// FIXME: Class 54-97

//
// Hotpatch flags
//
#define RTL_HOTPATCH_SUPPORTED_FLAG         0x01
#define RTL_HOTPATCH_SWAP_OBJECT_NAMES      0x08 << 24
#define RTL_HOTPATCH_SYNC_RENAME_FILES      0x10 << 24
#define RTL_HOTPATCH_PATCH_USER_MODE        0x20 << 24
#define RTL_HOTPATCH_REMAP_SYSTEM_DLL       0x40 << 24
#define RTL_HOTPATCH_PATCH_KERNEL_MODE      0x80 << 24


// Class 69
typedef struct _SYSTEM_HOTPATCH_CODE_INFORMATION
{
	ULONG Flags;
	ULONG InfoSize;
	union
	{
		struct
		{
			ULONG Foo;
		} CodeInfo;
		struct
		{
			USHORT NameOffset;
			USHORT NameLength;
		} KernelInfo;
		struct
		{
			USHORT NameOffset;
			USHORT NameLength;
			USHORT TargetNameOffset;
			USHORT TargetNameLength;
			UCHAR PatchingFinished;
		} UserModeInfo;
		struct
		{
			USHORT NameOffset;
			USHORT NameLength;
			USHORT TargetNameOffset;
			USHORT TargetNameLength;
			UCHAR PatchingFinished;
			NTSTATUS ReturnCode;
			HANDLE TargetProcess;
		} InjectionInfo;
		struct
		{
			HANDLE FileHandle1;
			PIO_STATUS_BLOCK IoStatusBlock1;
			PVOID RenameInformation1;
			PVOID RenameInformationLength1;
			HANDLE FileHandle2;
			PIO_STATUS_BLOCK IoStatusBlock2;
			PVOID RenameInformation2;
			PVOID RenameInformationLength2;
		} RenameInfo;
		struct
		{
			HANDLE ParentDirectory;
			HANDLE ObjectHandle1;
			HANDLE ObjectHandle2;
		} AtomicSwap;
	};
} SYSTEM_HOTPATCH_CODE_INFORMATION, *PSYSTEM_HOTPATCH_CODE_INFORMATION;

//enum _MEMORY_INFORMATION_CLASS
//{
//	MemoryBasicInformation,
//	MemoryWorkingSetList,
//	MemorySectionName,
//	MemoryBasicVlmInformation
//};

// ++ntobjapi.h

typedef struct _OBJECT_BASIC_INFORMATION
{
	ULONG Attributes;
	ACCESS_MASK GrantedAccess;
	ULONG HandleCount;
	ULONG PointerCount;
	ULONG PagedPoolCharge;
	ULONG NonPagedPoolCharge;
	ULONG Reserved[3];
	ULONG NameInfoSize;
	ULONG TypeInfoSize;
	ULONG SecurityDescriptorSize;
	LARGE_INTEGER CreationTime;
} OBJECT_BASIC_INFORMATION, *POBJECT_BASIC_INFORMATION;

typedef struct _OBJECT_TYPE_INFORMATION
{
	UNICODE_STRING TypeName;
	ULONG TotalNumberOfObjects;
	ULONG TotalNumberOfHandles;
	ULONG TotalPagedPoolUsage;
	ULONG TotalNonPagedPoolUsage;
	ULONG TotalNamePoolUsage;
	ULONG TotalHandleTableUsage;
	ULONG HighWaterNumberOfObjects;
	ULONG HighWaterNumberOfHandles;
	ULONG HighWaterPagedPoolUsage;
	ULONG HighWaterNonPagedPoolUsage;
	ULONG HighWaterNamePoolUsage;
	ULONG HighWaterHandleTableUsage;
	ULONG InvalidAttributes;
	GENERIC_MAPPING GenericMapping;
	ULONG ValidAccessMask;
	BOOLEAN SecurityRequired;
	BOOLEAN MaintainHandleCount;
	UCHAR TypeIndex; // since WINBLUE
	CHAR ReservedByte;
	ULONG PoolType;
	ULONG DefaultPagedPoolCharge;
	ULONG DefaultNonPagedPoolCharge;
} OBJECT_TYPE_INFORMATION, *POBJECT_TYPE_INFORMATION;

typedef struct _OBJECT_TYPES_INFORMATION
{
	ULONG NumberOfTypes;
	OBJECT_TYPE_INFORMATION TypeInformation[];
} OBJECT_TYPES_INFORMATION, *POBJECT_TYPES_INFORMATION;

typedef struct _OBJECT_HANDLE_FLAG_INFORMATION
{
	BOOLEAN Inherit;
	BOOLEAN ProtectFromClose;
} OBJECT_HANDLE_FLAG_INFORMATION, *POBJECT_HANDLE_FLAG_INFORMATION;

typedef struct _OBJECT_DIRECTORY_INFORMATION
{
	UNICODE_STRING Name;
	UNICODE_STRING TypeName;
} OBJECT_DIRECTORY_INFORMATION, *POBJECT_DIRECTORY_INFORMATION;

// --ntobjapi.h

enum SECTION_INFORMATION_CLASS
{
	SectionBasicInformation,
	SectionImageInformation
};

struct SECTION_BASIC_INFORMATION
{
	LPVOID BaseAddress;
	ULONG Attributes;
	LARGE_INTEGER Size;
};

struct SECTION_IMAGE_INFORMATION
{
	PVOID TransferAddress;
	ULONG ZeroBits;
	SIZE_T MaximumStackSize;
	SIZE_T CommittedStackSize;
	ULONG SubSystemType;
	union
	{
		struct
		{
			USHORT SubSystemMinorVersion;
			USHORT SubSystemMajorVersion;
		};
		ULONG SubSystemVersion;
	};
	ULONG GpValue;
	USHORT ImageCharacteristics;
	USHORT DllCharacteristics;
	USHORT Machine;
	BOOLEAN ImageContainsCode;
	union
	{
		UCHAR ImageFlags;
		struct
		{
			UCHAR ComPlusNativeReady : 1;
			UCHAR ComPlusILOnly : 1;
			UCHAR ImageDynamicallyRelocated : 1;
			UCHAR ImageMappedFlat : 1;
			UCHAR BaseBelow4gb : 1;
			UCHAR Reserved : 3;
		};
	};
	ULONG LoaderFlags;
	ULONG ImageFileSize;
	ULONG CheckSum;
};

// Thread information structures

typedef struct _THREAD_BASIC_INFORMATION
{
	NTSTATUS ExitStatus;
	PTEB TebBaseAddress;
	CLIENT_ID ClientId;
	ULONG_PTR AffinityMask;
	KPRIORITY Priority;
	LONG BasePriority;
} THREAD_BASIC_INFORMATION, *PTHREAD_BASIC_INFORMATION;

// private
typedef struct _THREAD_LAST_SYSCALL_INFORMATION
{
	PVOID FirstArgument;
	USHORT SystemCallNumber;
} THREAD_LAST_SYSCALL_INFORMATION, *PTHREAD_LAST_SYSCALL_INFORMATION;

// private
typedef struct _THREAD_CYCLE_TIME_INFORMATION
{
	ULONGLONG AccumulatedCycles;
	ULONGLONG CurrentCycleCount;
} THREAD_CYCLE_TIME_INFORMATION, *PTHREAD_CYCLE_TIME_INFORMATION;

// private
typedef struct _THREAD_TEB_INFORMATION
{
	PVOID TebInformation; // buffer to place data in
	ULONG TebOffset; // offset in TEB to begin reading from
	ULONG BytesToRead; // number of bytes to read
} THREAD_TEB_INFORMATION, *PTHREAD_TEB_INFORMATION;

// symbols
typedef struct _COUNTER_READING
{
	HARDWARE_COUNTER_TYPE Type;
	ULONG Index;
	ULONG64 Start;
	ULONG64 Total;
} COUNTER_READING, *PCOUNTER_READING;

// symbols
typedef struct _THREAD_PERFORMANCE_DATA
{
	USHORT Size;
	USHORT Version;
	PROCESSOR_NUMBER ProcessorNumber;
	ULONG ContextSwitches;
	ULONG HwCountersCount;
	ULONG64 UpdateCount;
	ULONG64 WaitReasonBitMap;
	ULONG64 HardwareCounters;
	COUNTER_READING CycleTime;
	COUNTER_READING HwCounters[MAX_HW_COUNTERS];
} THREAD_PERFORMANCE_DATA, *PTHREAD_PERFORMANCE_DATA;

// private
typedef struct _THREAD_PROFILING_INFORMATION
{
	ULONG64 HardwareCounters;
	ULONG Flags;
	ULONG Enable;
	PTHREAD_PERFORMANCE_DATA PerformanceData;
} THREAD_PROFILING_INFORMATION, *PTHREAD_PROFILING_INFORMATION;

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

enum CHANCE : char
{ 
	FirstChance, 
	LastChance
};

enum EVENT 
{
	ExceptionEvent, 
	DebugEvent
};

enum DEBUG_SERVICE_CODE
{
	DebugPrint = 1,
	DebugPromt,
	DebugLoadImageSymbols,
	DebugUnLoadImageSymbols
};

//////////////////////////////////////////////////////////////////////////
// ps
typedef enum _PS_PROTECTED_TYPE
{
	PsProtectedTypeNone,
	PsProtectedTypeProtectedLight,
	PsProtectedTypeProtected,
	PsProtectedTypeMax
} PS_PROTECTED_TYPE;

typedef enum _PS_PROTECTED_SIGNER
{
	PsProtectedSignerNone,
	PsProtectedSignerAuthenticode,
	PsProtectedSignerCodeGen,
	PsProtectedSignerAntimalware,
	PsProtectedSignerLsa,
	PsProtectedSignerWindows,
	PsProtectedSignerWinTcb,
	PsProtectedSignerWinSystem,
	PsProtectedSignerMax
} PS_PROTECTED_SIGNER;

typedef struct _PS_PROTECTION
{
	union
	{
		UCHAR Level;
		struct
		{
			UCHAR Type : 3;
			UCHAR Audit : 1;
			UCHAR Signer : 4;
		};
	};
} PS_PROTECTION, *PPS_PROTECTION;
