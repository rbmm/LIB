.686p

.model flat

extern _g_DriverObject:DWORD
extern __imp_@ObfDereferenceObject@4:DWORD

extern ?OnWorkItem@RtlTimer@IO_OBJECT_TIMEOUT@NT@@CGXPAX@Z:PROC

_TEXT segment

?_OnWorkItem@RtlTimer@IO_OBJECT_TIMEOUT@NT@@CGXPAX@Z proc
	push [esp + 4]
	call ?OnWorkItem@RtlTimer@IO_OBJECT_TIMEOUT@NT@@CGXPAX@Z
	mov ecx,_g_DriverObject
	jmp __imp_@ObfDereferenceObject@4
?_OnWorkItem@RtlTimer@IO_OBJECT_TIMEOUT@NT@@CGXPAX@Z endp

_ThreadStartThunk@4 proc
		pop eax
		xchg eax,[esp]
		call eax
		mov ecx,_g_DriverObject
		jmp __imp_@ObfDereferenceObject@4
_ThreadStartThunk@4 endp

_TEXT ends
end