.686p

.MODEL flat

extern @?FastReferenceDll:proc
extern ?DereferenceDll@NT@@YGXXZ:proc

; int __thiscall NT::CWinhttpEndpoint::StatusCallback(void *,unsigned long,void *,unsigned long)
extern ?StatusCallback@CWinhttpEndpoint@NT@@AAEHPAXK0K@Z : PROC

.CODE

?_StatusCallback@CWinhttpEndpoint@NT@@CGXPAXKK0K@Z proc
	call @?FastReferenceDll
	pop ecx
	xchg ecx,[esp+4*4]
	xchg ecx,[esp+4*3]
	xchg ecx,[esp+4*2]
	xchg ecx,[esp+4*1]
	call ?StatusCallback@CWinhttpEndpoint@NT@@AAEHPAXK0K@Z
	jmp ?DereferenceDll@NT@@YGXXZ
?_StatusCallback@CWinhttpEndpoint@NT@@CGXPAXKK0K@Z endp

end