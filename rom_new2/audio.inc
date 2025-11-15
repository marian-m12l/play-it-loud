; Original work by jbshelton: https://github.com/jbshelton/GBAudioPlayer

MACRO PlaybackAudioInit
	xor a
	ldh [rNR52], a	; Disable sound
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	ld a, $80
	ldh [rNR52], a	; Enable sound
	nop
	nop
	nop
	xor a
	ldh [rNR10], a	; Reset(?) channel 1 (Square)
	ld a, $11
	ldh [rNR51], a	; Enable stereo output
	xor a
	ldh [rNR13], a	; Reset(?) channel 1 (Square)
	ld a, $F0
	ldh [rNR12], a	; Max volume channel 1 (Square)
	ld a, $C0
	ldh [rNR11], a	; Max duty channel 1 (Square)
	ld a, $80
	ldh [rNR14], a	; Enable channel 1 (Square)
	ld a, $44
	ldh [rNR50], a	; L/R medium volume (4/7)
ENDM
