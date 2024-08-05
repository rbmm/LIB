public ?help_begin@NT@@3PADA, ?help_end@NT@@3PADA

public g_ProxyVtable

; void __cdecl NT::RET_DATA::OnRet(void **)
extern ?OnRet@RET_DATA@NT@@QEAAXPEAPEAX@Z : PROC

; void __cdecl NT::FProxy::ClientCall(void **)
extern ?ClientCall@FProxy@NT@@AEAAXPEAPEAX@Z : PROC

.CODE

CommonEntryI proc
	
	REPT 16
	int 3
	int 3
	int 3
	call @@0
	ENDM
	
@@0:
	pop rax
	mov r10,offset CommonEntryI
	sub rax,r10
	shr eax,3
	dec eax
	mov [rsp+32],r9                             ; arg3
	mov [rsp+24],r8                             ; arg2
	mov [rsp+16],rdx                            ; arg1
	mov rdx,rsp
	push rax                                    ; interface index
	sub rsp,20h                                 ; #1A: ( client thread, client stack)
	call ?ClientCall@FProxy@NT@@AEAAXPEAPEAX@Z
	add rsp,20h                                 ; #4A: ( server thread, client stack)
	pop rax
	mov rcx,[rsp+8]                             ; this
	mov rdx,[rsp+16]                            ; agr1
	mov r8,[rsp+24]                             ; arg2
	mov r9,[rsp+32]                             ; arg3
	mov r10,[rcx]
	jmp qword ptr [r10 + 8*rax]                 ; jmp to original method
	
CommonEntryI endp

; void __cdecl NT::RET_DATA::OnRetStub(void)

?OnRetStub@RET_DATA@NT@@SAXXZ proc
	mov rcx,[rsp]                               ; #5A: ( server thread, client stack - return from method )
	mov rdx,rsp
	push rax                                    ; save return value
	sub rsp,20h
	call ?OnRet@RET_DATA@NT@@QEAAXPEAPEAX@Z     ; goto #5C
	add rsp,20h                                 ; #8A: (client thread, client stack)
	pop rax                                     ; restore return value
	ret
?OnRetStub@RET_DATA@NT@@SAXXZ endp

; void __cdecl NT::SwitchToStack(void *,void **)
?SwitchToStack@NT@@YAXPEAXPEAPEAX@Z proc
	push rbx
	push rdi
	push rsi
	push rbp
	push r12
	push r13
	push r14
	push r15
	mov [rdx],rsp
	mov rsp,rcx
	pop r15
	pop r14
	pop r13
	pop r12
	pop rbp
	pop rsi
	pop rdi
	pop rbx
	mov rcx,[rsp + 10h]
	ret
?SwitchToStack@NT@@YAXPEAXPEAPEAX@Z endp

?strnlen@NT@@YA_K_KPEBD@Z proc
	xor eax,eax
	jecxz @@2
	push rdi
	mov rdi,rdx
	repne scasb
	jne @@1
	dec rdi
@@1:
	sub rdi,rdx
	mov rax,rdi
	pop rdi
@@2:
	ret
?strnlen@NT@@YA_K_KPEBD@Z endp

_TEXT ENDS

; long __cdecl NT::FProxy::QueryInterface_I(const struct _GUID &,void **)
extern ?QueryInterface_I@FProxy@NT@@AEAAJAEBU_GUID@@PEAPEAX@Z : PROC
; unsigned long __cdecl NT::FProxy::AddRef_I(void)
extern ?AddRef_I@FProxy@NT@@AEAAKXZ : PROC
; unsigned long __cdecl NT::FProxy::Release_I(void)
extern ?Release_I@FProxy@NT@@AEAAKXZ : PROC

.CONST

	ALIGN 8
g_ProxyVtable: 
	DQ ?QueryInterface_I@FProxy@NT@@AEAAJAEBU_GUID@@PEAPEAX@Z
	DQ ?AddRef_I@FProxy@NT@@AEAAKXZ
	DQ ?Release_I@FProxy@NT@@AEAAKXZ
	N = 3 + offset CommonEntryI + 3 * SIZEOF QWORD
	REPT 16 - 3
	DQ N
	N = N + SIZEOF QWORD
	ENDM

?help_begin@NT@@3PADA:
INCLUDE <js_help.asm>
?help_end@NT@@3PADA:

END