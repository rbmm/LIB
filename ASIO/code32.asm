.686p

.MODEL FLAT

extern __imp__LdrUnloadDll@4:DWORD
extern __imp__LdrAddRefDll@8:DWORD
extern ___ImageBase:BYTE
extern __imp__RtlNtStatusToDosError@4:DWORD

; void IO_IRP::IOCompletionRoutine(unsigned long,unsigned long)
extern ?IOCompletionRoutine@IO_IRP@NT@@QAEXKK@Z : PROC

; void NT_IRP::IOCompletionRoutine(long,unsigned long)
extern ?IOCompletionRoutine@NT_IRP@NT@@AAEXJK@Z : PROC 

; void __fastcall NT::RtlWait::WaitCallback(unsigned char)
extern ?WaitCallback@RtlWait@NT@@AAIXE@Z : PROC

; void __thiscall NT::RtlTimer::TimerCallback(void)
extern ?TimerCallback@RtlTimer@NT@@AAEXXZ : PROC

.DATA?
@?UsageCount DD ?

.CODE

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;VOID NTAPI ApcRoutine (
;	PVOID /*ApcContext*/,
;	PIO_STATUS_BLOCK IoStatusBlock,
;	ULONG /*Reserved*/
;	)
;{
;	static_cast<NT_IRP*>(IoStatusBlock)->IOCompletionRoutine(IoStatusBlock->Status, IoStatusBlock->Information);
;}

?ApcRoutine@NT_IRP@NT@@SGXPAXPAU_IO_STATUS_BLOCK@2@K@Z proc
	lock inc[@?UsageCount]
	pop ecx
	mov [esp + 8],ecx
	mov ecx,[esp + 4]
	mov eax,[ecx]
	mov [esp],eax
	mov eax,[ecx + 4]
	mov [esp + 4],eax
	call ?IOCompletionRoutine@NT_IRP@NT@@AAEXJK@Z
	jmp ?DereferenceDll@NT@@YGXXZ	
?ApcRoutine@NT_IRP@NT@@SGXPAXPAU_IO_STATUS_BLOCK@2@K@Z endp

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;VOID CALLBACK _IOCompletionRoutine(NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PVOID ApcContext)
;{
;	reinterpret_cast<NT_IRP*>(ApcContext)->IOCompletionRoutine(status, dwNumberOfBytesTransfered);
;}

?_IOCompletionRoutine@NT_IRP@NT@@SGXJKPAX@Z proc
	lock inc[@?UsageCount]
	pop ecx
	xchg ecx,[esp + 8]
	call ?IOCompletionRoutine@NT_IRP@NT@@AAEXJK@Z
	jmp ?DereferenceDll@NT@@YGXXZ	
?_IOCompletionRoutine@NT_IRP@NT@@SGXJKPAX@Z endp

?InitDllReference@NT@@YGXXZ proc
	mov @?UsageCount,0
	ret
?InitDllReference@NT@@YGXXZ endp

?ReferenceDll@NT@@YGXXZ proc
	mov eax,1
	lock xadd[@?UsageCount],eax
	test eax,eax
	jnz @@0
	lea eax, ___ImageBase
	push eax
	push 0
	call __imp__LdrAddRefDll@8
@@0:
	ret
?ReferenceDll@NT@@YGXXZ endp

?DereferenceDll@NT@@YGXXZ proc
	lock dec[@?UsageCount]
	jz @@0
	ret
@@0:
	lea eax, ___ImageBase
	xchg [esp],eax
	push eax
	jmp __imp__LdrUnloadDll@4
?DereferenceDll@NT@@YGXXZ endp

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;VOID CALLBACK _IOCompletionRoutine(NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
;{
;	static_cast<IO_IRP*>(lpOverlapped)->IOCompletionRoutine(RtlNtStatusToDosError(status), dwNumberOfBytesTransfered);
;}

?_IOCompletionRoutine@IO_IRP@NT@@SGXJKPAU_OVERLAPPED@@@Z proc
	lock inc[@?UsageCount]
	push [esp + 4]
	call __imp__RtlNtStatusToDosError@4
	pop ecx
	xchg ecx,[esp + 8]
	mov [esp],eax
	call ?IOCompletionRoutine@IO_IRP@NT@@QAEXKK@Z
	jmp ?DereferenceDll@NT@@YGXXZ	
?_IOCompletionRoutine@IO_IRP@NT@@SGXJKPAU_OVERLAPPED@@@Z endp

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;VOID CALLBACK _TimerCallback(PVOID pTimer, BOOLEAN /*TimerOrWaitFired*/)
;{
;	static_cast<RtlTimer*>(pTimer)->TimerCallback();
;}

?_TimerCallback@RtlTimer@NT@@CGXPAXE@Z proc
	lock inc[@?UsageCount]
	pop eax
	pop ecx
	mov [esp],eax
	call ?TimerCallback@RtlTimer@NT@@AAEXXZ
	jmp ?DereferenceDll@NT@@YGXXZ
?_TimerCallback@RtlTimer@NT@@CGXPAXE@Z endp

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;static VOID CALLBACK _WaitCallback(PVOID pTimer, BOOLEAN TimerOrWaitFired)
;{
;	static_cast<RtlWait*>(pTimer)->TimerCallback(TimerOrWaitFired);
;}

?_WaitCallback@RtlWait@NT@@CGXPAXE@Z proc
	lock inc[@?UsageCount]
	pop edx
	pop ecx
	xchg [esp],edx
	call ?WaitCallback@RtlWait@NT@@AAIXE@Z
	jmp ?DereferenceDll@NT@@YGXXZ
?_WaitCallback@RtlWait@NT@@CGXPAXE@Z endp

end