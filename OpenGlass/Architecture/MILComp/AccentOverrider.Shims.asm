OPTION CASEMAP:NONE

EXTERN MyCAccent__IsAcrylicBlurEnabledAndAllowed_Impl:PROC
PUBLIC MyCAccent__IsAcrylicBlurEnabledAndAllowed

.code

; 28000.3231 SetParentVisible retains this in R8 and visibility in R9B across
; the predicate, then passes R8 to SetDirtyFlags. Preserve both through the
; replacement and HookRundown, including the disabled-override/original path.
MyCAccent__IsAcrylicBlurEnabledAndAllowed PROC FRAME
	sub rsp, 38h
	.allocstack 38h
	.endprolog

	mov [rsp+20h], r8
	mov [rsp+28h], r9
	call MyCAccent__IsAcrylicBlurEnabledAndAllowed_Impl
	mov r9, [rsp+28h]
	mov r8, [rsp+20h]

	add rsp, 38h
	ret
MyCAccent__IsAcrylicBlurEnabledAndAllowed ENDP

END
