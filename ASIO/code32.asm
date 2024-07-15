.686p

.MODEL FLAT

extern @?FastReferenceDll:proc
extern ?DereferenceDll@NT@@YGXXZ:proc
extern __imp__RtlNtStatusToDosError@4:DWORD

; void __thiscall NT::IO_IRP::OnIoComplete(void *,IO_STATUS_BLOCK *)
extern ?OnIoComplete@IO_IRP@NT@@QAEXPAXPAU_IO_STATUS_BLOCK@2@@Z : PROC

; void __thiscall NT::NT_IRP::OnIoComplete(void *,IO_STATUS_BLOCK *)
extern ?OnIoComplete@NT_IRP@NT@@AAEXPAXPAU_IO_STATUS_BLOCK@2@@Z : PROC

; void __fastcall NT::RtlWait::WaitCallback(unsigned char)
extern ?WaitCallback@RtlWait@NT@@AAIXE@Z : PROC

; void __thiscall NT::RtlTimer::TimerCallback(void)
extern ?TimerCallback@RtlTimer@NT@@AAEXXZ : PROC

.CODE

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; void __stdcall NT::NT_IRP::S_OnIoComplete(TP_CALLBACK_INSTANCE *,void *,void *,IO_STATUS_BLOCK *,TP_IO *)
?S_OnIoComplete@NT_IRP@NT@@SGXPAU_TP_CALLBACK_INSTANCE@@PAX1PAU_IO_STATUS_BLOCK@2@PAU_TP_IO@@@Z proc
	call @?FastReferenceDll
	mov ecx,[esp + 12]
	mov eax,[esp]
	mov [esp+20],eax
	mov eax,[esp + 8]
	mov [esp + 12],eax
	add esp,3*4
$@$ LABEL PROC
	call ?OnIoComplete@NT_IRP@NT@@AAEXPAXPAU_IO_STATUS_BLOCK@2@@Z
	jmp ?DereferenceDll@NT@@YGXXZ	

?S_OnIoComplete@NT_IRP@NT@@SGXPAU_TP_CALLBACK_INSTANCE@@PAX1PAU_IO_STATUS_BLOCK@2@PAU_TP_IO@@@Z endp

; void __stdcall NT::NT_IRP::ApcRoutine(void *,IO_STATUS_BLOCK *,unsigned long)

?ApcRoutine@NT_IRP@NT@@SGXPAXPAU_IO_STATUS_BLOCK@2@K@Z proc
	call @?FastReferenceDll
	pop eax
	mov ecx,[esp + 4]
	mov [esp+8],eax
	jmp $@$
?ApcRoutine@NT_IRP@NT@@SGXPAXPAU_IO_STATUS_BLOCK@2@K@Z endp

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; void __stdcall NT::IO_IRP::S_OnIoComplete(TP_CALLBACK_INSTANCE *,void *,void *,IO_STATUS_BLOCK *,TP_IO *)
?S_OnIoComplete@IO_IRP@NT@@SGXPAU_TP_CALLBACK_INSTANCE@@PAX1PAU_IO_STATUS_BLOCK@2@PAU_TP_IO@@@Z proc
	
	call @?FastReferenceDll
	
	mov eax,[esp+16]
	push dword ptr [eax]
	call __imp__RtlNtStatusToDosError@4
	mov edx,[esp+16]
	mov [edx],eax
	
	mov ecx,[esp + 12]
	mov eax,[esp]
	mov [esp+20],eax
	mov eax,[esp + 8]
	mov [esp + 12],eax
	add esp,3*4

	call ?OnIoComplete@IO_IRP@NT@@QAEXPAXPAU_IO_STATUS_BLOCK@2@@Z
	jmp ?DereferenceDll@NT@@YGXXZ	
	
?S_OnIoComplete@IO_IRP@NT@@SGXPAU_TP_CALLBACK_INSTANCE@@PAX1PAU_IO_STATUS_BLOCK@2@PAU_TP_IO@@@Z endp

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;VOID CALLBACK _TimerCallback(PVOID pTimer, BOOLEAN /*TimerOrWaitFired*/)
;{
;	static_cast<RtlTimer*>(pTimer)->TimerCallback();
;}

?_TimerCallback@RtlTimer@NT@@CGXPAXE@Z proc
	call @?FastReferenceDll
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
	call @?FastReferenceDll
	pop edx
	pop ecx
	xchg [esp],edx
	call ?WaitCallback@RtlWait@NT@@AAIXE@Z
	jmp ?DereferenceDll@NT@@YGXXZ
?_WaitCallback@RtlWait@NT@@CGXPAXE@Z endp

@?FastReferenceDllNopa proc
	ret
@?FastReferenceDllNopa endp

end