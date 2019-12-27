extern __imp_LdrUnloadDll:QWORD
extern __imp_LdrAddRefDll:QWORD
extern __ImageBase:BYTE
extern __imp_RtlNtStatusToDosError:QWORD

; void IO_IRP::IOCompletionRoutine(unsigned long,unsigned __int64)
extern ?IOCompletionRoutine@IO_IRP@NT@@AEAAXK_K@Z : PROC

; void NT_IRP::IOCompletionRoutine(long,unsigned __int64)
extern ?IOCompletionRoutine@NT_IRP@NT@@AEAAXJ_K@Z : PROC

.DATA?
@?UsageCount DD ?

.CODE 

?ReferenceDll@NT@@YAXXZ proc
	mov eax,1
	lock xadd[@?UsageCount],eax
	test eax,eax
	jnz @@0
	lea rdx, __ImageBase
	xor ecx,ecx
	jmp __imp_LdrAddRefDll
@@0:
	ret
?ReferenceDll@NT@@YAXXZ endp

?CommonCbRet? proc
	add rsp,28h
?DereferenceDll@NT@@YAXXZ proc
	lock dec[@?UsageCount]
	jnz @@0
	lea rcx, __ImageBase
	jmp __imp_LdrUnloadDll
@@0:
	ret
?DereferenceDll@NT@@YAXXZ endp
?CommonCbRet? endp

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;VOID CALLBACK _IOCompletionRoutine(NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
;{
;	static_cast<IO_IRP*>(lpOverlapped)->IOCompletionRoutine(RtlNtStatusToDosError(status), dwNumberOfBytesTransfered);
;}

?_IOCompletionRoutine@IO_IRP@NT@@SAXJ_KPEAU_OVERLAPPED@@@Z proc
	sub rsp,28h
	mov [rsp + 30h],rdx
	mov [rsp + 38h],r8
	call __imp_RtlNtStatusToDosError
	mov r8,[rsp + 30h]
	mov edx,eax
	mov rcx,[rsp + 38h]
	call ?IOCompletionRoutine@IO_IRP@NT@@AEAAXK_K@Z
	jmp ?CommonCbRet?
?_IOCompletionRoutine@IO_IRP@NT@@SAXJ_KPEAU_OVERLAPPED@@@Z endp


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;VOID CALLBACK _IOCompletionRoutine(NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PVOID ApcContext)
;{
;	reinterpret_cast<NT_IRP*>(ApcContext)->IOCompletionRoutine(status, dwNumberOfBytesTransfered);
;}

?_IOCompletionRoutine@NT_IRP@NT@@SAXJ_KPEAX@Z proc
	sub rsp,28h
	xchg r8,rcx
	xchg r8,rdx
	call ?IOCompletionRoutine@NT_IRP@NT@@AEAAXJ_K@Z
	jmp ?CommonCbRet?
?_IOCompletionRoutine@NT_IRP@NT@@SAXJ_KPEAX@Z endp


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;VOID NTAPI ApcRoutine (
;	PVOID /*ApcContext*/,
;	PIO_STATUS_BLOCK IoStatusBlock,
;	ULONG /*Reserved*/
;	)
;{
;	static_cast<NT_IRP*>(IoStatusBlock)->IOCompletionRoutine(IoStatusBlock->Status, IoStatusBlock->Information);
;}


?ApcRoutine@NT_IRP@NT@@SAXPEAXPEAU_IO_STATUS_BLOCK@2@K@Z proc
	sub rsp,28h
	mov rcx,rdx
	mov rdx,[rcx]
	mov r8,[rcx + 8]
	call ?IOCompletionRoutine@NT_IRP@NT@@AEAAXJ_K@Z
	jmp ?CommonCbRet?
?ApcRoutine@NT_IRP@NT@@SAXPEAXPEAU_IO_STATUS_BLOCK@2@K@Z endp


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;VOID CALLBACK _TimerCallback(PVOID pTimer, BOOLEAN /*TimerOrWaitFired*/)
;{
;	static_cast<RtlTimer*>(pTimer)->TimerCallback();
;}
		
?_TimerCallback@RtlTimer@NT@@CAXPEAXE@Z proc
	sub rsp,28h
	mov rax,[rcx]
	call qword ptr [rax]
	jmp ?CommonCbRet?
?_TimerCallback@RtlTimer@NT@@CAXPEAXE@Z endp

end