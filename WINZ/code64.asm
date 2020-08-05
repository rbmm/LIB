_TEXT segment 'CODE'

?findWORD@NT@@YAPEAG_KPEAGG@Z proc
	jrcxz @retz
	xchg rdi,rdx
	mov rax,r8
	repne scasw
	lea rax, [rdi-2]
	cmovne rax, rcx
	mov rdi,rdx
	ret
?findWORD@NT@@YAPEAG_KPEAGG@Z endp

?findDWORD@NT@@YAPEAK_KPEAKK@Z proc
	jrcxz @retz
	xchg rdi,rdx
	mov rax,r8
	repne scasd
	lea rax, [rdi-4]
	cmovne rax, rcx
	mov rdi,rdx
	ret
?findDWORD@NT@@YAPEAK_KPEAKK@Z endp

?findPVOID@NT@@YAPEAPEAX_KPEAPEAXPEAX@Z proc
	jrcxz @retz
	xchg rdi,rdx
	mov rax,r8
	repne scasq
	lea rax, [rdi-8]
	cmovne rax, rcx
	mov rdi,rdx
	ret
?findPVOID@NT@@YAPEAPEAX_KPEAPEAXPEAX@Z endp

?wtrnchr@NT@@YAPEA_W_KPEBX_W@Z proc
	jrcxz @retz
	xchg rdi,rdx
	mov rax,r8
	repne scasw
	mov rax,rdi
	cmovne rax,rcx
	mov rdi,rdx
	ret
?wtrnchr@NT@@YAPEA_W_KPEBX_W@Z endp

?strnchr@NT@@YAPEAD_KPEBXD@Z proc
	jrcxz @retz
	mov rax,r8
	xchg rdi,rdx
	repne scasb
	mov rax,rdi
	mov rdi,rdx
	cmovne rax,rcx
	ret
?strnchr@NT@@YAPEAD_KPEBXD@Z endp

@retz proc
	xor eax,eax
	ret
@retz endp

?strnstr@NT@@YAPEAD_KPEBX01@Z proc
	jrcxz @retz
	cmp rcx,r8
	jb @retz
	push rdi
	push rsi
	mov rdi,rdx
	mov al,[r9]
	inc r9
	dec r8
	sub rcx,r8
@@1:
	repne scasb
	jne @@2
	mov rsi,r9
	mov rdx,rdi
	mov r10,rcx
	mov rcx,r8
	test ecx,ecx
	repe cmpsb
	je @@2
	mov rcx,r10
	mov rdi,rdx
	jmp @@1
@@2:
	mov rax,rdi
	cmovne rax,rcx
	pop rsi
	pop rdi
	ret
?strnstr@NT@@YAPEAD_KPEBX01@Z endp

?wtrnstr@NT@@YAPEA_W_KPEBX01@Z proc
	jrcxz @retz
	cmp rcx,r8
	jb @retz
	push rdi
	push rsi
	mov rdi,rdx
	mov ax,[r9]
	inc r9
	inc r9
	dec r8
	sub rcx,r8
@@1:
	repne scasw
	jne @@2
	mov rsi,r9
	mov rdx,rdi
	mov r10,rcx
	mov rcx,r8
	test ecx,ecx
	repe cmpsw
	je @@2
	mov rcx,r10
	mov rdi,rdx
	jmp @@1
@@2:
	mov rax,rdi
	cmovne rax,rcx
	pop rsi
	pop rdi
	ret
?wtrnstr@NT@@YAPEA_W_KPEBX01@Z endp

EXTERN ?_WindowProc@ZSubClass@NT@@AEAA_JPEAUHWND__@@I_K_J@Z:PROC

;;;;;;;;;;;;;;;;;;; NT::ZSubClass::__WindowProc ;;;;;;;;;;;;;;;;;;;

?__WindowProc@ZSubClass@NT@@CAXXZ PROC
	xchg [rsp],r9
	xchg r9,r8
	xchg r8,rdx
	xchg rdx,rcx
	sub rsp,4*8
	call ?_WindowProc@ZSubClass@NT@@AEAA_JPEAUHWND__@@I_K_J@Z ; NT::ZSubClass::_WindowProc
	add rsp,5*8
	ret
?__WindowProc@ZSubClass@NT@@CAXXZ ENDP

_TEXT ENDS
END