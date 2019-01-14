_TEXT segment 'CODE'

strnchr proc
	jrcxz @@1
	push rdi
	mov al,r8b
	mov rdi,rdx
	repne scasb
	sete al
	movzx rax,al
	neg rax
	and rax,rdi
	pop rdi
	ret
@@1:
	xor rax,rax
	ret
strnchr endp

strnstr proc
	jrcxz @@3
	cmp rcx,r8
	jb @@3
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
	sete al
	movzx rax,al
	neg rax
	and rax,rdi
	pop rsi
	pop rdi
	ret
@@3:
	xor rax,rax
	ret
strnstr endp

wtrnchr proc
	jrcxz @@1
	push rdi
	mov ax,r8w
	mov rdi,rdx
	repne scasw
	sete al
	movzx rax,al
	neg rax
	and rax,rdi
	pop rdi
	ret
@@1:
	xor rax,rax
	ret
wtrnchr endp

wtrnstr proc
	jrcxz @@3
	cmp rcx,r8
	jb @@3
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
	sete al
	movzx rax,al
	neg rax
	and rax,rdi
	pop rsi
	pop rdi
	ret
@@3:
	xor rax,rax
	ret
wtrnstr endp

findWORD proc
	xchg rdi,rdx
	mov rax,r8
	repne scasw
	sete al
	movzx rax,al
	neg rax
	lea rdi,[rdi-2]
	and rax,rdi
	mov rdi,rdx
	ret
findWORD endp

findDWORD proc
	xchg rdi,rdx
	mov rax,r8
	repne scasd
	sete al
	movzx rax,al
	neg rax
	lea rdi,[rdi-4]
	and rax,rdi
	mov rdi,rdx
	ret
findDWORD endp

findPVOID proc
	xchg rdi,rdx
	mov rax,r8
	repne scasq
	sete al
	movzx rax,al
	neg rax
	lea rdi,[rdi-8]
	and rax,rdi
	mov rdi,rdx
	ret
findPVOID endp

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