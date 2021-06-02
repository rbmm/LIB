extern __imp_LdrUnloadDll:QWORD
extern __imp_LdrAddRefDll:QWORD
extern __ImageBase:BYTE

extern ?WindowProc@?$CWindowImplBaseT@VCWindow@ATL@@V?$CWinTraits@$0FGAAAAAA@$0A@@2@@ATL@@SA_JPEAUHWND__@@I_K_J@Z : PROC

.DATA?

	align 4
@@UsageCount	DD ?

.code

@@FastReferenceDll proc
	lock inc[@@UsageCount]
	ret
@@FastReferenceDll endp

?ReferenceDll@@YAXXZ proc
	mov eax,1
	lock xadd[@@UsageCount],eax
	test eax,eax
	jz @@AddRefDll
	ret
@@AddRefDll:
	lea rdx, __ImageBase
	xor ecx,ecx
	jmp __imp_LdrAddRefDll
?ReferenceDll@@YAXXZ endp

?DereferenceDll@@YAXXZ proc
	lock dec[@@UsageCount]
	jz @@UnloadDll
	ret
@@UnloadDll:
	lea rcx, __ImageBase
	jmp __imp_LdrUnloadDll
?DereferenceDll@@YAXXZ endp

?StubWindowProc@MySubClassBaseT@@CA_JPEAUHWND__@@I_K_J@Z proc
	call @@FastReferenceDll
	sub rsp,28h
	call ?WindowProc@?$CWindowImplBaseT@VCWindow@ATL@@V?$CWinTraits@$0FGAAAAAA@$0A@@2@@ATL@@SA_JPEAUHWND__@@I_K_J@Z
	add rsp,28h
	jmp ?DereferenceDll@@YAXXZ
?StubWindowProc@MySubClassBaseT@@CA_JPEAUHWND__@@I_K_J@Z endp

end