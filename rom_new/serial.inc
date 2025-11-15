;***************************************************************************
;*   hl - dest
;*   bc - length
;***************************************************************************
MACRO CopyDataFromSerial
:
	; Enable serial external clock / wait for incoming serial data
	ld a, $80
	ldh [rSC], a

	; Actively wait for incoming data rSC bit 7 is cleared when data is received
:
	ld a, [rSC]
	and $80
	jr nz, :-

	; Read received data
	ldh a, [rSB]
	
	; Write to VRAM
	ldi [hl], a
	
	; Loop
	dec bc
	; Check if bc is 0
	ld a, c
	or b
	jr nz, :--   ; Jump to the next byte
ENDM
