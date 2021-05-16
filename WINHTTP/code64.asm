; int __cdecl NT::CWinhttpEndpoint::StatusCallback(void *,unsigned long,void *,unsigned long)
extern ?StatusCallback@CWinhttpEndpoint@NT@@AEAAHPEAXK0K@Z : PROC

extern @?FastReferenceDll:proc
extern ?DereferenceDll@NT@@YAXXZ:proc

.CODE

?_StatusCallback@CWinhttpEndpoint@NT@@CAXPEAX_KK0K@Z proc
	call @?FastReferenceDll
	xchg rcx,rdx
	mov rax,[rsp + 28h]
	sub rsp,38h
	mov [rsp + 20h],rax
	call ?StatusCallback@CWinhttpEndpoint@NT@@AEAAHPEAXK0K@Z
	add rsp,38h
	jmp ?DereferenceDll@NT@@YAXXZ
?_StatusCallback@CWinhttpEndpoint@NT@@CAXPEAX_KK0K@Z endp

end