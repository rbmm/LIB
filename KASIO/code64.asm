extern g_DriverObject:QWORD
extern __imp_ObfDereferenceObject:QWORD

extern ?OnWorkItem@RtlTimer@IO_OBJECT_TIMEOUT@NT@@CAXPEAX@Z:PROC

_TEXT SEGMENT

ThreadStartThunk proc
		sub rsp,40
		call rcx
		add rsp,40
		mov rcx,g_DriverObject
		jmp __imp_ObfDereferenceObject
ThreadStartThunk endp

?_OnWorkItem@RtlTimer@IO_OBJECT_TIMEOUT@NT@@CAXPEAX@Z proc
	sub rsp,40
	call ?OnWorkItem@RtlTimer@IO_OBJECT_TIMEOUT@NT@@CAXPEAX@Z
	add rsp,40
	mov rcx,g_DriverObject
	jmp __imp_ObfDereferenceObject
?_OnWorkItem@RtlTimer@IO_OBJECT_TIMEOUT@NT@@CAXPEAX@Z endp

_TEXT ENDS

END