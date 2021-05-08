extern @?FastReferenceDll:proc
extern ?DereferenceDll@NT@@YAXXZ:proc
extern __imp_GetWindowLongPtrW:QWORD

; __int64 __cdecl NT::ZWnd::WrapperWindowProc(struct HWND__ *,unsigned int,unsigned __int64,__int64)
extern ?WrapperWindowProc@ZWnd@NT@@AEAA_JPEAUHWND__@@I_K_J@Z : PROC

; __int64 __cdecl NT::ZDlg::WrapperDialogProc(struct HWND__ *,unsigned int,unsigned __int64,__int64)
extern ?WrapperDialogProc@ZDlg@NT@@AEAA_JPEAUHWND__@@I_K_J@Z : PROC

; __int64 __cdecl NT::ZSubClass::WrapperWindowProc(struct HWND__ *,unsigned int,unsigned __int64,__int64)
extern ?WrapperWindowProc@ZSubClass@NT@@AEAA_JPEAUHWND__@@I_K_J@Z : PROC

_TEXT segment 'CODE'

@?FastReferenceDllNop proc
	ret
@?FastReferenceDllNop endp

?SubClassProc@ZSubClass@NT@@CA_JPEAUHWND__@@I_K_J11@Z proc
	call @?FastReferenceDll 
	sub rsp,38h
	mov [rsp+20h],r9
	mov r9,r8
	mov r8,rdx
	mov rdx,rcx
	mov rcx,[rsp + 68h]
	call ?WrapperWindowProc@ZSubClass@NT@@AEAA_JPEAUHWND__@@I_K_J@Z
	add rsp,38h
	jmp ?DereferenceDll@NT@@YAXXZ
?SubClassProc@ZSubClass@NT@@CA_JPEAUHWND__@@I_K_J11@Z endp

?_WindowProc@ZWnd@NT@@CA_JPEAUHWND__@@I_K_J@Z proc
	call @?FastReferenceDll
	mov [rsp+8],rcx
	mov [rsp+16],rdx
	mov [rsp+24],r8
	mov [rsp+32],r9
	sub rsp,28h
	mov edx,-21 ; GWLP_USERDATA
	call __imp_GetWindowLongPtrW
	mov rcx,rax
	mov rdx,[rsp+48]
	mov r8,[rsp+56]
	mov r9,[rsp+64]
	mov rax,[rsp+72]
	mov [rsp+32],rax
	call ?WrapperWindowProc@ZWnd@NT@@AEAA_JPEAUHWND__@@I_K_J@Z
	add rsp,28h
	jmp ?DereferenceDll@NT@@YAXXZ
?_WindowProc@ZWnd@NT@@CA_JPEAUHWND__@@I_K_J@Z endp

?_DialogProc@ZDlg@NT@@CA_JPEAUHWND__@@I_K_J@Z proc
	call @?FastReferenceDll
	mov [rsp+8],rcx
	mov [rsp+16],rdx
	mov [rsp+24],r8
	mov [rsp+32],r9
	sub rsp,28h
	mov edx,16 ; DWLP_USER
	call __imp_GetWindowLongPtrW
	mov rcx,rax
	mov rdx,[rsp+48]
	mov r8,[rsp+56]
	mov r9,[rsp+64]
	mov rax,[rsp+72]
	mov [rsp+32],rax
	call ?WrapperDialogProc@ZDlg@NT@@AEAA_JPEAUHWND__@@I_K_J@Z
	add rsp,28h
	jmp ?DereferenceDll@NT@@YAXXZ
?_DialogProc@ZDlg@NT@@CA_JPEAUHWND__@@I_K_J@Z endp

_TEXT ENDS
END