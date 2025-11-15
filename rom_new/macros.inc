;***************************************************************************
;*   hl - dest
;*   de - src
;*   bc - length
;***************************************************************************
MACRO CopyData
:
	ld a, [de]
	ldi [hl], a
	inc de
	dec bc
	; Check if bc is 0
	ld a, c
	or b
	jr nz, :-   ; Jump to the next byte
ENDM


;***************************************************************************
;*   hl - dest
;*   de - src
;*   bc - length
;***************************************************************************
MACRO CopyFont1bpp
:
	ld a, [de]
	ldi [hl], a
	ldi [hl], a     ; Font tiles are 1bpp --> copy each byte twice to convert to 2bpp
	inc de
	dec bc
	; Check if bc is 0
	ld a, c
	or b
	jr nz, :-   ; Jump to the next byte
ENDM
