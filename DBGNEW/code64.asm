public ?help_begin@NT@@3QBDB, ?help_end@NT@@3QBDB
public ?kdd_begin@NT@@3QBDB, ?kdd_end@NT@@3QBDB

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

.CONST

?help_begin@NT@@3QBDB:
INCLUDE <js_help.asm>
?help_end@NT@@3QBDB:

?kdd_begin@NT@@3QBDB:
INCLUDE <kdd.asm>
?kdd_end@NT@@3QBDB:

END