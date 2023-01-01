public ?help_begin@NT@@3PADA, ?help_end@NT@@3PADA

.CODE

?strnlen@NT@@YA_K_KPEBD@Z proc
	xor eax,eax
	jecxz @@2
	push rdi
	mov rdi,rdx
	repne scasb
	jne @@1
	dec rdi
@@1:
	sub rdi,rdx
	mov rax,rdi
	pop rdi
@@2:
	ret
?strnlen@NT@@YA_K_KPEBD@Z endp

_TEXT ENDS

.CONST

?help_begin@NT@@3PADA:
INCLUDE <js_help.asm>
?help_end@NT@@3PADA:

END