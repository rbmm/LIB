extern @?FastReferenceDll:proc
extern ?DereferenceDll@NT@@YAXXZ:proc

extern __imp_RtlNtStatusToDosError:QWORD

; void IO_IRP::IOCompletionRoutine(unsigned long,unsigned __int64)
extern ?IOCompletionRoutine@IO_IRP@NT@@QEAAXK_K@Z : PROC

; void NT_IRP::IOCompletionRoutine(long,unsigned __int64)
extern ?IOCompletionRoutine@NT_IRP@NT@@AEAAXJ_K@Z : PROC

; void __cdecl NT::RtlTimer::TimerCallback(void)
extern ?TimerCallback@RtlTimer@NT@@AEAAXXZ : PROC

; void __cdecl NT::RtlWait::WaitCallback(unsigned char)
extern ?WaitCallback@RtlWait@NT@@AEAAXE@Z : PROC

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

ALIGN 16 ; must be 16 byte aligned !!! for not confuse with wow apc

?ApcRoutine@NT_IRP@NT@@SAXPEAXPEAU_IO_STATUS_BLOCK@2@K@Z proc
	call @?FastReferenceDll
	sub rsp,28h
	mov rcx,rdx
	mov rdx,[rcx]
	mov r8,[rcx + 8]
	call ?IOCompletionRoutine@NT_IRP@NT@@AEAAXJ_K@Z
	add rsp,28h
	jmp ?DereferenceDll@NT@@YAXXZ
?ApcRoutine@NT_IRP@NT@@SAXPEAXPEAU_IO_STATUS_BLOCK@2@K@Z endp


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;VOID CALLBACK _IOCompletionRoutine(NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
;{
;	static_cast<IO_IRP*>(lpOverlapped)->IOCompletionRoutine(RtlNtStatusToDosError(status), dwNumberOfBytesTransfered);
;}

?_IOCompletionRoutine@IO_IRP@NT@@SAXJ_KPEAU_OVERLAPPED@@@Z proc
	call @?FastReferenceDll
	sub rsp,28h
	mov [rsp + 30h],rdx
	mov [rsp + 38h],r8
	call __imp_RtlNtStatusToDosError
	mov r8,[rsp + 30h]
	mov edx,eax
	mov rcx,[rsp + 38h]
	call ?IOCompletionRoutine@IO_IRP@NT@@QEAAXK_K@Z
	add rsp,28h
	jmp ?DereferenceDll@NT@@YAXXZ
?_IOCompletionRoutine@IO_IRP@NT@@SAXJ_KPEAU_OVERLAPPED@@@Z endp

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;VOID CALLBACK _IOCompletionRoutine(NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PVOID ApcContext)
;{
;	reinterpret_cast<NT_IRP*>(ApcContext)->IOCompletionRoutine(status, dwNumberOfBytesTransfered);
;}

?_IOCompletionRoutine@NT_IRP@NT@@SAXJ_KPEAX@Z proc
	call @?FastReferenceDll
	sub rsp,28h
	xchg r8,rcx
	xchg r8,rdx
	call ?IOCompletionRoutine@NT_IRP@NT@@AEAAXJ_K@Z
	add rsp,28h
	jmp ?DereferenceDll@NT@@YAXXZ
?_IOCompletionRoutine@NT_IRP@NT@@SAXJ_KPEAX@Z endp

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;VOID CALLBACK _TimerCallback(PVOID pTimer, BOOLEAN /*TimerOrWaitFired*/)
;{
;	static_cast<RtlTimer*>(pTimer)->TimerCallback();
;}
		
?_TimerCallback@RtlTimer@NT@@CAXPEAXE@Z proc
	call @?FastReferenceDll
	sub rsp,28h
	call ?TimerCallback@RtlTimer@NT@@AEAAXXZ
	add rsp,28h
	jmp ?DereferenceDll@NT@@YAXXZ
?_TimerCallback@RtlTimer@NT@@CAXPEAXE@Z endp


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;static VOID CALLBACK _WaitCallback(PVOID pTimer, BOOLEAN TimerOrWaitFired)
;{
;	static_cast<RtlWait*>(pTimer)->TimerCallback(TimerOrWaitFired);
;}

?_WaitCallback@RtlWait@NT@@CAXPEAXE@Z proc
	call @?FastReferenceDll
	sub rsp,28h
	call ?WaitCallback@RtlWait@NT@@AEAAXE@Z
	add rsp,28h
	jmp ?DereferenceDll@NT@@YAXXZ
?_WaitCallback@RtlWait@NT@@CAXPEAXE@Z endp

@?FastReferenceDllNopa proc
	ret
@?FastReferenceDllNopa endp

end