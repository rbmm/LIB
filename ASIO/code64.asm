extern @?FastReferenceDll:proc
extern ?DereferenceDll@NT@@YAXXZ:proc

extern __imp_RtlNtStatusToDosError:QWORD

; void __cdecl NT::IO_IRP::OnIoComplete(void *,struct NT::_IO_STATUS_BLOCK *)
extern ?OnIoComplete@IO_IRP@NT@@QEAAXPEAXPEAU_IO_STATUS_BLOCK@2@@Z : PROC

; void __cdecl NT::NT_IRP::OnIoComplete(void *Context, IO_STATUS_BLOCK * iosb)
extern ?OnIoComplete@NT_IRP@NT@@AEAAXPEAXPEAU_IO_STATUS_BLOCK@2@@Z : PROC

; void __cdecl NT::RtlTimer::TimerCallback(void)
extern ?TimerCallback@RtlTimer@NT@@AEAAXXZ : PROC

; void __cdecl NT::RtlWait::WaitCallback(unsigned char)
extern ?WaitCallback@RtlWait@NT@@AEAAXE@Z : PROC

.CODE 

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;VOID NTAPI ApcRoutine (
;	PVOID ApcContext,
;	PIO_STATUS_BLOCK IoStatusBlock,
;	ULONG /*Reserved*/
;	)


ALIGN 16 ; must be 16 byte aligned !!! for not confuse with wow apc

?ApcRoutine@NT_IRP@NT@@SAXPEAXPEAU_IO_STATUS_BLOCK@2@K@Z proc
	call @?FastReferenceDll
	sub rsp,28h
	mov r8,rdx
	mov rdx,rcx
	mov rcx,r8
	jmp $@$
?ApcRoutine@NT_IRP@NT@@SAXPEAXPEAU_IO_STATUS_BLOCK@2@K@Z endp

; void __cdecl NT::NT_IRP::S_OnIoComplete(
;	TP_CALLBACK_INSTANCE *,
;	void *Context,
;	void *ApcContext,
;	IO_STATUS_BLOCK *,
;	TP_IO *)
	
?S_OnIoComplete@NT_IRP@NT@@SAXPEAU_TP_CALLBACK_INSTANCE@@PEAX1PEAU_IO_STATUS_BLOCK@2@PEAU_TP_IO@@@Z proc
	call @?FastReferenceDll
	sub rsp,28h
	mov rcx,r8
	mov r8,r9
$@$ LABEL PROC
	call ?OnIoComplete@NT_IRP@NT@@AEAAXPEAXPEAU_IO_STATUS_BLOCK@2@@Z
	add rsp,28h
	jmp ?DereferenceDll@NT@@YAXXZ

?S_OnIoComplete@NT_IRP@NT@@SAXPEAU_TP_CALLBACK_INSTANCE@@PEAX1PEAU_IO_STATUS_BLOCK@2@PEAU_TP_IO@@@Z endp

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; void __cdecl NT::IO_IRP::S_OnIoComplete(TP_CALLBACK_INSTANCE *,void *,void *,IO_STATUS_BLOCK *,TP_IO *)
?S_OnIoComplete@IO_IRP@NT@@SAXPEAU_TP_CALLBACK_INSTANCE@@PEAX1PEAU_IO_STATUS_BLOCK@2@PEAU_TP_IO@@@Z proc
	call @?FastReferenceDll
	sub rsp,28h
	mov [rsp + 30h],rdx
	mov [rsp + 38h],r8
	mov [rsp + 40h],r9
	mov rcx,[r9]
	call __imp_RtlNtStatusToDosError
	mov r8,[rsp + 40h]
	mov rcx,[rsp + 38h]
	mov rdx,[rsp + 30h]
	mov [r8],rax
	call ?OnIoComplete@IO_IRP@NT@@QEAAXPEAXPEAU_IO_STATUS_BLOCK@2@@Z
	add rsp,28h
	jmp ?DereferenceDll@NT@@YAXXZ
?S_OnIoComplete@IO_IRP@NT@@SAXPEAU_TP_CALLBACK_INSTANCE@@PEAX1PEAU_IO_STATUS_BLOCK@2@PEAU_TP_IO@@@Z endp

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