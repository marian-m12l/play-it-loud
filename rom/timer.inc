// Original work by jbshelton: https://github.com/jbshelton/GBAudioPlayer

DEF workVar1 EQU $81
DEF workVar0 EQU $80

MACRO PlaybackTimerInit
	; FIXME Playback high RAM variables ($F8, $F7) should defined!!
	xor a
	ldh [workVar1], a	; Reset variable (in High RAM) used in PCM player ???
	ldh [workVar0], a	; Reset variable (in High RAM) used in PCM player ???
	ld a, $D6		; Timer / playback rate 6241.52380952Hz (262144 / 42)
	ldh [rTMA], a	; Timer modulo --> When TIMA overflows, it is reset to the value in this register and an interrupt is requested. Example of use: if TMA is set to $FF, an interrupt is requested at the clock frequency selected in TAC (because every increment is an overflow). However, if TMA is set to $FE, an interrupt is only requested every two increments, which effectively divides the selected clock by two. Setting TMA to $FD would divide the clock by three, and so on.
	ld a, %00000101
	ldh [rTAC], a	; Enable timer @ CPU clock / 16 (Normal Speed 262144 Hz)
	ldh a, [rIE]	; Read enabled interrupts
	or a, $04		; Enable timer interrupt
	ldh [rIE], a
ENDM

MACRO ReadyTimer
	; Init hl register with read position
	ld hl, wBuffer0
ENDM

MACRO PlaybackTimerISR
	; hl contains read position / source data address
	ld a, [hl]  ;[2]
	ld e, a     ;[1]
	and $F0     ;[2]
	ld d, a     ;[1]
	jr nz, .regPulse1   ;[2]
	ld a, $1F   ;[2]
	ldh [rNR12], a  ;[3]
	ld a, $40   ;[2]
	ldh [workVar1], a    ;[3]
	jr .pcm1    ;[2]

.regPulse1:
	ld a, $C0   ;[2]
	ldh [workVar1], a    ;[3]
	ld a, d     ;[1]
	or $0F      ;[2]
	ldh [rNR12], a  ;[3]

.pcm1:
	ld a, e     ;[1]
	and $07     ;[2]
	ld d, a     ;[1]
	swap a      ;[2]
	or d        ;[1]
	ldh [workVar0], a    ;[3]
	ld a, e     ;[1]
	and $08     ;[2]
	jr z, .noVol    ;[2]
	ldh a, [workVar0]    ;[3]
	ldh [rNR50], a  ;[3]

	ldh a, [workVar1]    ;[3]
	ldh [rNR11], a  ;[3]
	
	ld a, $80       ;[2]
	ldh [rNR14], a  ;[3]

	jr .endPCM  ;[2]

.noVol:	
	ldh a, [workVar1]    ;[3]
	ldh [rNR11], a  ;[3]
	
	ld a, $80   ;[2]
	ldh [rNR14], a  ;[3]

	ldh a, [workVar0]    ;[3]
	ldh [rNR50], a  ;[3]
.endPCM:
	inc hl	    ;[2]
	; Handle end of buffer --> loop
	ld a, h							;[1]	; Load high byte of HL register to compare with end of buffer
	cp HIGH(wBuffersEnd)			;[2]
	jr nz, .sampleEnd				;[2]	; 4 cycles if path taken, 3 cycles otherwise
	ld a, HIGH(wBuffer0)			;[2]	; back to buffer 0
	ld h, a							;[1]	; Reset high byte of HL register to loop back
.sampleEnd:
	reti    ;[4]
ENDM

