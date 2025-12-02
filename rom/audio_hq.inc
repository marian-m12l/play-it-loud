; Original work by jbshelton: https://github.com/jbshelton/GBAudioPlayerV3/blob/main/src/ds_hq_mono.asm

MACRO PlaybackAudioInit

	xor a
	ldh [rNR52], a		; Disable sound
	cpl
	ldh [rNR52], a		; Enable sound

	ld e, $0f

	ld a, $e0
	ldh [rNR13], a		; Channel 1 period low ($e0)

	ld a, $c0
	ldh [rNR11], a		; Channel 1 length timer ($00) & duty cycle (75%)

	ld a, $0f
	ldh [rNR12], a		; Channel 1 volume ($00) & envelope (increasing volume, sweep $07)

	ld a, $87
	ldh [rDIV], a		; Reset divider register (writing any value resets it to $00) for... reasons ?!!
	ldh [rNR14], a		; Channel 1 period high ($07) & control (trigger: enables channel and resets timers)

	ldh a, [workVarCapabilities]
	bit 0, a
	jr z, .normalRateStallPulse

.doubleRateStallPulse:
	ldh a, [rDIV]
	cp 19				; rDIV is incremented at 32768 Hz in double speed --> 580 us since line 22 ?!
	jr nz, .doubleRateStallPulse
	jr .doneStallPulse

.normalRateStallPulse:
	ldh a, [rDIV]
	cp 9				; rDIV is incremented at 16384 Hz in normal speed --> 550 us since line 22 ?!
	jr nz, .normalRateStallPulse

.doneStallPulse:
	xor a
	ldh [rNR13], a		; Channel 1 period low ($00)

	ld a, $80
	ldh [rNR14], a		; Channel 1 period high ($00) & control (trigger: enables channel and resets timers)

	ld a, $11
	ldh [rNR51], a		; Sound panning (channel 1 to L/R)
	ld a, $77
	ldh [rNR50], a		; Master volume (L/R volume to max: 0x07) & VIN panning
ENDM
