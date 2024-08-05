.686

.model flat

public ?help_begin@NT@@3QBDB, ?help_end@NT@@3QBDB
public ?kdd_begin@NT@@3QBDB, ?kdd_end@NT@@3QBDB

.code

?strnlen@NT@@YIIIPBD@Z proc
	xor eax,eax
	jecxz @@2
	push edi
	mov edi,edx
	repne scasb
	jne @@1
	dec edi
@@1:
	sub edi,edx
	mov eax,edi
	pop edi
@@2:
	ret
?strnlen@NT@@YIIIPBD@Z endp


CONST segment

?help_begin@NT@@3QBDB:
INCLUDE <js_help.asm>
?help_end@NT@@3QBDB:

?kdd_begin@NT@@3QBDB:
INCLUDE <kdd.asm>
?kdd_end@NT@@3QBDB:
CONST ends

END