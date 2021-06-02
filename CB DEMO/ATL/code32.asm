.686

.MODEL flat

extern __imp__LdrAddRefDll@8:DWORD
extern __imp__LdrUnloadDll@4:DWORD
extern ___ImageBase:BYTE

extern ?WindowProc@?$CWindowImplBaseT@VCWindow@ATL@@V?$CWinTraits@$0FGAAAAAA@$0A@@2@@ATL@@SGJPAUHWND__@@IIJ@Z : PROC

.DATA?

	align 4
@@UsageCount	DD ?
	
.CODE

@@FastReferenceDll proc
	lock inc[@@UsageCount]
	ret
@@FastReferenceDll endp

?ReferenceDll@@YGXXZ proc
	mov eax,1
	lock xadd[@@UsageCount],eax
	test eax,eax
	jnz @@nop
	lea eax, ___ImageBase
	push eax
	xor eax,eax
	push eax
	call __imp__LdrAddRefDll@8
@@nop:
	ret
?ReferenceDll@@YGXXZ endp

?DereferenceDll@@YGXXZ proc
	lock dec[@@UsageCount]
	jz @@UnloadDll
	ret
@@UnloadDll:
	lea eax, ___ImageBase
	xchg [esp],eax
	push eax
	jmp __imp__LdrUnloadDll@4
?DereferenceDll@@YGXXZ endp

?StubWindowProc@MySubClassBaseT@@CGJPAUHWND__@@IIJ@Z proc
	mov eax,[esp]
	xchg [esp+4*4],eax
	xchg [esp+3*4],eax
	xchg [esp+2*4],eax
	xchg [esp+1*4],eax
	mov [esp],eax
	call @@FastReferenceDll
	call ?WindowProc@?$CWindowImplBaseT@VCWindow@ATL@@V?$CWinTraits@$0FGAAAAAA@$0A@@2@@ATL@@SGJPAUHWND__@@IIJ@Z
	jmp ?DereferenceDll@@YGXXZ
?StubWindowProc@MySubClassBaseT@@CGJPAUHWND__@@IIJ@Z endp

end