; Original work by jbshelton: https://github.com/jbshelton/GBAudioPlayerV3/blob/main/src/ds_hq_mono.asm

MACRO PlaybackTimerInit
ENDM


MACRO PlaybackSerialInit
	ldh a, [rIE]	; Read enabled interrupts
	or a, $08		; Enable serial interrupt
	ldh [rIE], a
	; Enable serial external clock / wait for incoming serial data
	ld a, $80
	ldh [rSC], a
	; Initialize counter
	ld bc, $0000
ENDM


MACRO PlaybackSerialISR
	; Read received data
	ldh a, [rSB]					;[3]

	; Trigger reset
	cp $ff							;[2]
	jp z, NewTrack					;[3|4]

	; Keep received data / sample
	ld h, a						    ;[1]

	; Re-enable rSC ASAP to not miss a clock pulse
	; Set constant serial output
	; Write back a counter of received samples ?
	; FIXME just write back 0x01 for each sample ? (for predictable checksum)
	ld a, c							;[1]
	ldh [rSB], a					;[3]
	; Wait for the next serial data (enable external clock)
	ld a, $80						;[2]
	ldh [rSC], a					;[3]

	inc bc							;[2]

; 20 cycles

	; Play HQ audio sample
	ld a, h							;[1]
	ld d, a							;[1]
	and $0f							;[2]
	ld e, a							;[1]
	swap a							;[2]
	or e							;[1]
	ld e, a							;[1]
	ld a, d							;[1]
	or $0f							;[1]
	ldh [rNR12], a					;[3]
	ld a, $80						;[2]
	ldh [rNR50], a					;[3]
	ldh [rNR14], a					;[3]
	ld a, e							;[1]
	ldh [rNR50], a					;[3]
; 26 cycles

	reti							;[4]
ENDM

; count cycles: isr overhead (5-9) + serial (20) + playback (26) + reti (4) = 59 cycles max (28 us at double-speed)
; time between isr and rSC ready: isr overhead (5-9) + serial (18) = 27 cycles max (13 us at double-speed)


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
