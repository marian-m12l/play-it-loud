; Original work by jbshelton: https://github.com/jbshelton/GBAudioPlayer

;FIXME replace with registers h and l !!!

DEF workVar1 EQU $FFF8
DEF workVar0 EQU $FFF7

MACRO PlaybackTimerInit
	ld a, $80
	ldh [workVar1], a	; Reset variable (in High RAM) used in PCM player ???
	ldh [workVar0], a	; Reset variable (in High RAM) used in PCM player ???
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
	cp $f1							;[2]
	jp z, NewTrack					;[3|4]

	; Keep received data / sample
	; FIXME store in h/l register (not yet used) to free up register bc as a counter ?
	;ld b, a						    ;[1]
	ld h, a						    ;[1]

	; Re-enable rSC ASAP to not miss a clock pulse
	; FIXME worst-case delay between byte being send over serial and rSC being armed again ? (e.g. is just started playback audio ISR ??)
	; Set constant serial output
	; FIXME write back a counter of received samples ? (inc bc ??)
	;ld a, $f2						;[2]
	ld a, c							;[1]
	ldh [rSB], a					;[3]
	; Wait for the next serial data (enable external clock)
	ld a, $80						;[2]
	ldh [rSC], a					;[3]
;19 cycles

	inc bc							;[2]

	; TODO Play audio sample

	;ld a, b  						;[1]
	ld a, h  						;[1]
	ld e, a     					;[1]
	and $F0     					;[2]
	ld d, a     					;[1]
; 24 cycles
	jr nz, .regPulse1   			;[2|3]
	ld a, $1F   					;[2]
	ldh [rNR12], a  				;[3]
	ld a, $40   					;[2]
	ldh [workVar1], a    			;[3]
	jr .pcm1    					;[2]
; 38 cycles

.regPulse1:
	ld a, $C0   					;[2]
	ldh [workVar1], a    			;[3]
	ld a, d     					;[1]
	or $0F      					;[2]
	ldh [rNR12], a  				;[3]
; 38 cycles

.pcm1:
	ld a, e     					;[1]
	and $07     					;[2]
	ld d, a     					;[1]
	swap a      					;[2]
	or d        					;[1]
	ldh [workVar0], a    			;[3]
	ld a, e     					;[1]
	and $08     					;[2]
; 51 cycles
	jr z, .noVol    				;[2|3]
	ldh a, [workVar0]    			;[3]
	ldh [rNR50], a  				;[3]

	ldh a, [workVar1]    			;[3]
	ldh [rNR11], a  				;[3]
	
	ld a, $80       				;[2]
	ldh [rNR14], a  				;[3]

	jr .endPCM  					;[2]
; 72 cycles

.noVol:	
	ldh a, [workVar1]    			;[3]
	ldh [rNR11], a  				;[3]
	
	ld a, $80   					;[2]
	ldh [rNR14], a  				;[3]

	ldh a, [workVar0]    			;[3]
	ldh [rNR50], a  				;[3]
; 71 cycles

.endPCM:
	reti							;[4]
ENDM


; TODO count cycles
; MAX 75 cycles ? --> 72 us @ single speed --> 36 us @ double speed
; TODO Add ISR call (5 cycles)
; TODO Add ISR jump (4 cycles)
; TODO Add worst-case end-of-instruction (halt is 4 cycles -> remaining 3 cycles at max)
; TOTAL 87 cycles !! --> 83 us / 42 us


; FIXME replace workVar0/1 with registers h/l --> save 8 cycles ?


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
