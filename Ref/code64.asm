extern __imp_LdrUnloadDll:QWORD
extern __imp_LdrAddRefDll:QWORD
extern __ImageBase:BYTE

.DATA?

	align 4
@?UsageCount	DD ?

.CODE

?ReferenceDll@NT@@YAXXZ proc
	mov eax,1
	lock xadd[@?UsageCount],eax
	test eax,eax
	jnz @@nop
	lea rdx, __ImageBase
	xor ecx,ecx
	jmp __imp_LdrAddRefDll
@@nop:
	ret
?ReferenceDll@NT@@YAXXZ endp

@?FastReferenceDll proc
	lock inc[@?UsageCount]
	ret
@?FastReferenceDll endp

?DereferenceDll@NT@@YAXXZ proc
	lock dec[@?UsageCount]
	jz @@UnloadDll
	ret
@@UnloadDll:
	lea rcx, __ImageBase
	jmp __imp_LdrUnloadDll
?DereferenceDll@NT@@YAXXZ endp

	
end
