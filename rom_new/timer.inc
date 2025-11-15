; Original work by jbshelton: https://github.com/jbshelton/GBAudioPlayer

MACRO PlaybackTimerInit
    ld a, $80       ; Reset work registers used in PCM player
	ld h, a
	ld l, a
	ld a, $E0		; Timer / playback rate 8192Hz (262144 / 32) / 16384Hz (524288 / 32)
	ldh [rTMA], a	; Timer modulo --> When TIMA overflows, it is reset to the value in this register and an interrupt is requested. Example of use: if TMA is set to $FF, an interrupt is requested at the clock frequency selected in TAC (because every increment is an overflow). However, if TMA is set to $FE, an interrupt is only requested every two increments, which effectively divides the selected clock by two. Setting TMA to $FD would divide the clock by three, and so on.
	ld a, %00000101
	ldh [rTAC], a	; Enable timer @ CPU clock / 16 (Normal Speed 262144 Hz / CGB Double Speed --> 524288 Hz)
	ldh a, [rIE]	; Read enabled interrupts
	or a, $04		; Enable timer interrupt
	ldh [rIE], a
ENDM

MACRO PlaybackTimerISR
	; Read received data
	ldh a, [rSB]					;[3]

	; Trigger reset
	cp $f1							;[2]
	jp z, NewTrack					;[3|4]

	; Keep received data / sample
	ld b, a						    ;[1]

	; Re-enable rSC ASAP to not miss a clock pulse
	; Set constant serial output
	ld a, $f2						;[2]
	ldh [rSB], a					;[3]
	; Wait for the next serial data (enable external clock)
	ld a, $80				        ;[2]
	ldh [rSC], a			        ;[3]

    ; Process sample
	ld a, b                         ;[1]
	ld e, a                         ;[1]
	and $F0                         ;[2]
	ld d, a                         ;[1]    -- 24
	jr nz, .regPulse1               ;[2|3]
	ld a, $1F                       ;[2]
	ldh [rNR12], a                  ;[3]
	ld a, $40                       ;[2]
	ld h, a                         ;[1]
	jr .pcm1                        ;[3]

.regPulse1:
	ld a, $C0                       ;[2]
    ld h, a                         ;[1]
	ld a, d                         ;[1]
	or $0F                          ;[2]
	ldh [rNR12], a                  ;[3]

.pcm1:
	ld a, e                         ;[1]
	and $07                         ;[2]
	ld d, a                         ;[1]
	swap a                          ;[2]
	or d                            ;[1]
    ld l, a                         ;[1]
	ld a, e                         ;[1]
	and $08                         ;[2]
	jr z, .noVol                    ;[2|3]
    ld a, l                         ;[1]
	ldh [rNR50], a                  ;[3]

    ld a, h                         ;[1]
	ldh [rNR11], a                  ;[3]
	
	ld a, $80                       ;[2]
	ldh [rNR14], a                  ;[3]

	jr .endPCM                      ;[2]

.noVol:	
	ld a, h                         ;[1]
	ldh [rNR11], a                  ;[3]
	
	ld a, $80                       ;[2]
	ldh [rNR14], a                  ;[3]

    ld a, l                         ;[1]
	ldh [rNR50], a                  ;[3]
.endPCM:
	reti                            ;[4]
ENDM

; TODO Max cycles ? (including 5 + 4 cycles of ISR handling + jp)
; no branch: 69 cycles
; 1st branch; 70 cycles
; second branch: 68 cycles
; both branches: 67 cycles

; MAX TOTAL: 79 cycles --> 75 us / 38 us