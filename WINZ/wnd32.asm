.686

.MODEL flat

extern @?FastReferenceDll:proc
extern ?DereferenceDll@NT@@YGXXZ:proc
extern __imp__GetWindowLongW@8:DWORD

; long __thiscall NT::ZWnd::WrapperWindowProc(struct HWND__ *,unsigned int,unsigned int,long)
extern ?WrapperWindowProc@ZWnd@NT@@AAEJPAUHWND__@@IIJ@Z : PROC

; int __thiscall NT::ZDlg::WrapperDialogProc(struct HWND__ *,unsigned int,unsigned int,long)
extern ?WrapperDialogProc@ZDlg@NT@@AAEHPAUHWND__@@IIJ@Z : PROC

; long __thiscall NT::ZSubClass::WrapperWindowProc(struct HWND__ *,unsigned int,unsigned int,long)
extern ?WrapperWindowProc@ZSubClass@NT@@AAEJPAUHWND__@@IIJ@Z : PROC

.CODE

@?FastReferenceDllNop proc
	ret
@?FastReferenceDllNop endp

?_WindowProc@ZWnd@NT@@CGJPAUHWND__@@IIJ@Z proc
	mov eax,[esp]
	xchg [esp+4*4],eax
	xchg [esp+3*4],eax
	xchg [esp+2*4],eax
	xchg [esp+1*4],eax
	mov [esp],eax
	push -21 ; GWLP_USERDATA
	push eax ; hwnd
	call __imp__GetWindowLongW@8
	mov ecx,eax
	call @?FastReferenceDll
	call ?WrapperWindowProc@ZWnd@NT@@AAEJPAUHWND__@@IIJ@Z
	jmp ?DereferenceDll@NT@@YGXXZ
?_WindowProc@ZWnd@NT@@CGJPAUHWND__@@IIJ@Z endp

?_DialogProc@ZDlg@NT@@CGHPAUHWND__@@IIJ@Z proc
	mov eax,[esp]
	xchg [esp+4*4],eax
	xchg [esp+3*4],eax
	xchg [esp+2*4],eax
	xchg [esp+1*4],eax
	mov [esp],eax
	push 8 ; DWLP_USER
	push eax ; hwnd
	call __imp__GetWindowLongW@8
	mov ecx,eax
	call @?FastReferenceDll
	call ?WrapperDialogProc@ZDlg@NT@@AAEHPAUHWND__@@IIJ@Z
	jmp ?DereferenceDll@NT@@YGXXZ
?_DialogProc@ZDlg@NT@@CGHPAUHWND__@@IIJ@Z endp

?SubClassProc@ZSubClass@NT@@CGJPAUHWND__@@IIJIK@Z proc
	pop ecx
	xchg ecx,[esp + 14h]
	call @?FastReferenceDll
	call ?WrapperWindowProc@ZSubClass@NT@@AAEJPAUHWND__@@IIJ@Z
	pop ecx
	jmp ?DereferenceDll@NT@@YGXXZ
?SubClassProc@ZSubClass@NT@@CGJPAUHWND__@@IIJIK@Z endp

end