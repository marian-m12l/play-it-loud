MACRO PlaybackSerialInit
	ldh a, [rIE]	; Read enabled interrupts
	or a, $08		; Enable serial interrupt
	ldh [rIE], a
	; Enable serial external clock / wait for incoming serial data
	ld a, $80
	ldh [rSC], a
ENDM

MACRO ReadySerial
	; Init bc register with write position
	ld bc, wBuffer1
ENDM

MACRO PlaybackSerialISR
	; bc contains write position / destination data address
	; Read received data
	ldh a, [rSB]					;[3]

	; Trigger reset
	cp $f1
	jp z, NewTrack

	; Copy to destination
	ld [bc], a						;[2]
	; Increment destination
	inc bc							;[2]
	; Handle end of buffer --> loop
	ld a, b							;[1]	; Load high byte of BC register to compare with end of buffer?
	cp HIGH(wBuffersEnd)			;[2]
	jr nz, .noLoopback				;[2]
	ld a, HIGH(wBuffer0)			;[2]	; back to buffer 0
	ld b, a							;[1]	; Reset high byte of BC register to loop back

.noLoopback:
	; Wait for the next serial data (enable external clock)
	ld a, $80				;[2]
	ldh [rSC], a			;[3]
	reti					;[4]
ENDM


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
