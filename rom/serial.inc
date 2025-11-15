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
	cp $f1							;[2]
	; FIXME should still return from interrupt ??? remove return address from stack ? 
	jp z, NewTrack					;[2]

	; Copy to destination
	ld [bc], a						;[2]

	; Re-enable rSC ASAP to not miss a clock pulse
	; FIXME worst-case delay between byte being send over serial and rSC being armed again ? (e.g. is just started playback audio ISR ??)
	; Set constant serial output
	ld a, $f2						;[2]
	ldh [rSB], a					;[3]
	; Wait for the next serial data (enable external clock)
	ld a, $80				;[2]
	ldh [rSC], a			;[3]

	; Increment destination
	inc bc							;[2]
	; Handle end of buffer --> loop
	ld a, b							;[1]	; Load high byte of BC register to compare with end of buffer?
	cp HIGH(wBuffersEnd)			;[2]
	jr nz, .noLoopback				;[2]
	ld a, HIGH(wBuffer0)			;[2]	; back to buffer 0
	ld b, a							;[1]	; Reset high byte of BC register to loop back

.noLoopback:
	; FIXME Re-enable rSC ASAP to not miss a clock pulse !!!
	; FIXME worst-case delay between byte being send over serial and rSC being armed again ? (e.g. is just started playback audio ISR ??)
	; Wait for the next serial data (enable external clock)
	;ld a, $80				;[2]
	;ldh [rSC], a			;[3]
	; increment received samples count (used by playback)
	;inc sp					;[2]

	; FIXME set a flag to signal timer ISR that there is a sample to play
	; Set carry flag
	;scf

	; FIXME Set 'de' register to invalid value ? --> playback should abort if the value is valid

	reti					;[4]
ENDM

; MAX 33 cycles ? --> 31 us @ single speed --> 16 us @ double speed
; TODO Add ISR call (5 cycles)
; TODO Add ISR jump (4 cycles)
; TODO Add worst-case end-of-instruction (halt is 4 cycles -> remaining 3 cycles at max)
; TOTAL 45 cycles !! --> 43 us / 22 us


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
