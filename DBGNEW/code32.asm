.686

.model flat
public ?help_begin@NT@@3PADA, ?help_end@NT@@3PADA

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

?help_begin@NT@@3PADA:
INCLUDE <js_help.asm>
?help_end@NT@@3PADA:
CONST ends
END