.686

.MODEL flat

extern __imp__LdrAddRefDll@8:DWORD
extern __imp__LdrUnloadDll@4:DWORD
extern ___ImageBase:BYTE

.DATA?

	align 4
@?UsageCount	DD ?
	
.CODE

?ReferenceDll@NT@@YGXXZ proc
	mov eax,1
	lock xadd[@?UsageCount],eax
	test eax,eax
	jnz @@nop
	lea eax, ___ImageBase
	push eax
	xor eax,eax
	push eax
	call __imp__LdrAddRefDll@8
@@nop:
	ret
?ReferenceDll@NT@@YGXXZ endp

@?FastReferenceDll proc
	lock inc[@?UsageCount]
	ret
@?FastReferenceDll endp

?DereferenceDll@NT@@YGXXZ proc
	lock dec[@?UsageCount]
	jz @@UnloadDll
	ret
@@UnloadDll:
	lea eax, ___ImageBase
	xchg [esp],eax
	push eax
	jmp __imp__LdrUnloadDll@4

?DereferenceDll@NT@@YGXXZ endp


end