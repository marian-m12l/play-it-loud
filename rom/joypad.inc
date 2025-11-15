MACRO JoypadInit
	; Select both buttons and d-pad to interrupt on any buton press FIXME possible ??
	ld a, $00
	; FIXME Listen to button press only
	;ld a, $10
	ldh [rP1], a

	ldh a, [rIE]	; Read enabled interrupts
	or a, $10		; Enable joypad interrupt
	ldh [rIE], a
ENDM

MACRO ReadyJoypad
	; No-op ??
	nop
ENDM

MACRO DoJoypadISR
	; TODO re-read buttons ??? Do something ?

	; TODO 1: stop audio immediately?
	; TODO 2: Set [rP1] to read buttons only ($10), then read multiple times to get pressed button
	; TODO 3: Set [rP1] to read dpad only ($20), then read multiple times to get pressed direction
	; TODO 4: React accordingly

	; FIXME should still return from interrupt ??? remove return address from stack ? 
	jp NewTrack

	reti					;[4]
ENDM
