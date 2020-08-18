.686

_TEXT segment

?findPVOID@NT@@YIPAPAXKPAPAXPAX@Z proc
?findDWORD@NT@@YIPAKKPAKK@Z proc
	jecxz @retz4
	xchg edi,edx
	mov eax,[esp + 4]
	repne scasd
	lea eax,[edi-4]
	cmovne eax,ecx
	mov edi,edx
	ret 4
?findDWORD@NT@@YIPAKKPAKK@Z endp
?findPVOID@NT@@YIPAPAXKPAPAXPAX@Z endp

?findWORD@NT@@YIPAGKPAGG@Z proc
	jecxz @retz4
	xchg edi,edx
	mov eax,[esp + 4]
	repne scasw
	lea eax,[edi-2]
	cmovne eax,ecx
	mov edi,edx
	ret 4
?findWORD@NT@@YIPAGKPAGG@Z endp

@retz4 proc
	xor eax,eax
	ret 4
@retz4 endp

?strnchr@NT@@YIPADKPBXD@Z proc
	jecxz @retz4
	mov al,[esp + 4]
	xchg edi,edx
	repne scasb
	mov eax,edi
	cmovne eax,ecx
	mov edi,edx
	ret 4
?strnchr@NT@@YIPADKPBXD@Z endp

?wtrnchr@NT@@YIPA_WKPBX_W@Z proc
	jecxz @retz4
	mov ax,[esp + 4]
	xchg edi,edx
	repne scasw
	mov eax,edi
	cmovne eax,ecx
	mov edi,edx
	ret 4
?wtrnchr@NT@@YIPA_WKPBX_W@Z endp

?strnstr@NT@@YIPADKPBXK0@Z proc
	jecxz @retz8
	cmp ecx,[esp + 4]
	jb @retz8
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
	mov eax,edi
	cmovne eax,ecx
	pop ebp
	pop ebx
	pop esi
	pop edi
	ret 8
?strnstr@NT@@YIPADKPBXK0@Z endp

@retz8 proc
	xor eax,eax
	ret 8
@retz8 endp

?wtrnstr@NT@@YIPA_WKPBXK0@Z proc
	jecxz @retz8
	cmp ecx,[esp + 4]
	jb @retz8
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
	mov eax,edi
	cmovne eax,ecx
	pop ebp
	pop ebx
	pop esi
	pop edi
	ret 8
?wtrnstr@NT@@YIPA_WKPBXK0@Z endp

EXTERN ?_WindowProc@ZSubClass@NT@@AAEJPAUHWND__@@IIJ@Z:PROC ; NT::ZSubClass::_WindowProc

;;;;;;;;;;;;;;; NT::ZSubClass::__WindowProc  ;;;;;;;;;;;;;;;;;;;;;

?__WindowProc@ZSubClass@NT@@CGXXZ PROC
	pop ecx
	jmp	?_WindowProc@ZSubClass@NT@@AAEJPAUHWND__@@IIJ@Z
?__WindowProc@ZSubClass@NT@@CGXXZ ENDP

_TEXT ends
end