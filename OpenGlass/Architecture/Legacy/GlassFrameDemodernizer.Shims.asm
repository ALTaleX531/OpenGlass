OPTION CASEMAP:NONE

EXTERN MySetMargin_Impl:PROC
PUBLIC MySetMargin

.code

; Build 26100+ UpdateMarginsDependentOnStyle privately reuses RCX after its
; three-argument SetMargin wrapper returns. Preserving RCX is ABI-compatible on
; earlier builds, so use this physical dispatcher across all supported versions
; and restore RCX only after replacement, original call, and rundown release.
MySetMargin PROC FRAME
	sub rsp, 38h
	.allocstack 38h
	.endprolog

	; Forward the original fifth and sixth arguments across this extra call frame.
	mov eax, dword ptr [rsp+60h]
	mov dword ptr [rsp+20h], eax
	mov rax, qword ptr [rsp+68h]
	mov qword ptr [rsp+28h], rax
	mov qword ptr [rsp+30h], rcx

	call MySetMargin_Impl
	mov rcx, qword ptr [rsp+30h]

	add rsp, 38h
	ret
MySetMargin ENDP

END
