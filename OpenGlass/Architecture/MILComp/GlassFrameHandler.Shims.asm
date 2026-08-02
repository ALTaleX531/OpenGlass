OPTION CASEMAP:NONE

EXTERN MyCLegacyNonClientBackground_HasSomethingToRender_Impl:PROC
PUBLIC MyCLegacyNonClientBackground_HasSomethingToRender

.code

; CloneVisualTreeForLivePreview is privately optimized across the original
; HasSomethingToRender call and reuses RCX and DL afterward. Preserve both
; volatile registers while the C++ implementation performs the map lookup.
MyCLegacyNonClientBackground_HasSomethingToRender PROC FRAME
	sub rsp, 38h
	.allocstack 38h
	.endprolog

	mov [rsp+20h], rcx
	mov [rsp+28h], rdx
	call MyCLegacyNonClientBackground_HasSomethingToRender_Impl
	mov rdx, [rsp+28h]
	mov rcx, [rsp+20h]

	add rsp, 38h
	ret
MyCLegacyNonClientBackground_HasSomethingToRender ENDP

END
