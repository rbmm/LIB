.686

_TEXT segment

@findPVOID@12:

public @findPVOID@12

@findDWORD@12 proc
	xchg edi,edx
	mov eax,[esp + 4]
	repne scasd
	sete al
	movzx eax,al
	neg eax
	lea edi,[edi-4]
	and eax,edi
	mov edi,edx
	ret 4
@findDWORD@12 endp

@findWORD@12 proc
	xchg edi,edx
	mov eax,[esp + 4]
	repne scasw
	sete al
	movzx eax,al
	neg eax
	lea edi,[edi-2]
	and eax,edi
	mov edi,edx
	ret 4
@findWORD@12 endp

@strnstr@16 proc
	jecxz @@3
	cmp ecx,[esp + 4]
	jb @@3
	push edi
	push esi
	push ebx
	push ebp
	mov ebx,[esp + 20]
	mov ebp,[esp + 24]
	mov edi,edx
	mov al,[ebp]
	inc ebp
	dec ebx
	sub ecx,ebx
@@1:
	repne scasb
	jne @@2
	mov esi,ebp
	mov edx,edi
	push ecx
	mov ecx,ebx
	test ecx,ecx
	repe cmpsb
	pop ecx
	je @@2
	mov edi,edx
	jmp @@1
@@2:
	sete al
	movzx eax,al
	neg eax
	and eax,edi
	pop ebp
	pop ebx
	pop esi
	pop edi
	ret 8
@@3:
	xor eax,eax
	ret 8
@strnstr@16 endp

@strnchr@12 proc
	jecxz @@1
	mov al,[esp + 4]
	push edi
	mov edi,edx
	repne scasb
	sete al
	movzx eax,al
	neg eax
	and eax,edi	
	pop edi
	ret 4
@@1:
	xor eax,eax
	ret 4
@strnchr@12 endp

@wtrnchr@12 proc
	jecxz @@1
	mov ax,[esp + 4]
	push edi
	mov edi,edx
	repne scasw
	sete al
	movzx eax,al
	neg eax
	and eax,edi	
	pop edi
	ret 4
@@1:
	xor eax,eax
	ret 4
@wtrnchr@12 endp

@wtrnstr@16 proc
	jecxz @@3
	cmp ecx,[esp + 4]
	jb @@3
	push edi
	push esi
	push ebx
	push ebp
	mov ebx,[esp + 20]
	mov ebp,[esp + 24]
	mov edi,edx
	mov ax,[ebp]
	inc ebp
	inc ebp
	dec ebx
	sub ecx,ebx
@@1:
	repne scasw
	jne @@2
	mov esi,ebp
	mov edx,edi
	push ecx
	mov ecx,ebx
	test ecx,ecx
	repe cmpsw
	pop ecx
	je @@2
	mov edi,edx
	jmp @@1
@@2:
	sete al
	movzx eax,al
	neg eax
	and eax,edi
	pop ebp
	pop ebx
	pop esi
	pop edi
	ret 8
@@3:
	xor eax,eax
	ret 8
@wtrnstr@16 endp

EXTERN ?_WindowProc@ZSubClass@NT@@AAEJPAUHWND__@@IIJ@Z:PROC ; NT::ZSubClass::_WindowProc

;;;;;;;;;;;;;;; NT::ZSubClass::__WindowProc  ;;;;;;;;;;;;;;;;;;;;;

?__WindowProc@ZSubClass@NT@@CGXXZ PROC
	pop ecx
	jmp	?_WindowProc@ZSubClass@NT@@AAEJPAUHWND__@@IIJ@Z
?__WindowProc@ZSubClass@NT@@CGXXZ ENDP

_TEXT ends
end