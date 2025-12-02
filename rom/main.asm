INCLUDE "hardware.inc"
INCLUDE "ibmpc1.inc"
INCLUDE "macros.inc"
INCLUDE "variables.inc"

IF __DISABLE_HQ_ENCODING__ == 0
INCLUDE "audio_hq.inc"
INCLUDE "serial_hq.inc"
ELSE
INCLUDE "audio.inc"
INCLUDE "serial.inc"
ENDC


SECTION "VBlank", ROM0 [$40]
	reti

SECTION "Serial", ROM0 [$58]
	jp SerialISR

SECTION "Header", ROM0[$100]

EntryPoint:
	di
	jp Start

REPT $150 - $104
	db 0
ENDR


SECTION "Game code", ROM0[$150]

Start:

IF __DISABLE_DOUBLE_SPEED__ == 0
	; Detect CGB and switch to double speed
	cp $11
	jr nz, .skipDoubleSpeed

	; Switch to double speed
	ld a, $30
	ldh [rP1], a
	ld a, $01
	ldh [rKEY1], a
	stop
	nop
	nop

	; Store current speed
	ld a, $01
	ldh [workVarCapabilities], a
	jr .skipNormalSpeed
ENDC

.skipDoubleSpeed:
	; Store current speed
	xor a
	ldh [workVarCapabilities], a

.skipNormalSpeed:
	ldh a, [workVarCapabilities]
IF __DISABLE_HQ_ENCODING__ == 0
	or $fa
ELSE
	or $f8
ENDC
	ldh [workVarCapabilities], a

	xor a
	ldh [rIE], a	; All interrupts OFF
	ld sp, $FFFF	; Set stack pointer

.waitvbl:
	ldh a, [rLY]	; Read current line
	cp 144
	jr c, .waitvbl	; Loop if line 144 not reached

	xor a
	ldh [rLCDC], a 	; Disable display

Init:
	ld	a, %11100100	; 11=Black 10=Dark grey 01=Light grey 00=White/Transparent
	ld	[rBGP], a		; Background palette
	; Since the ROM is CGB-compatible, also configure CGB Palettes
	ld a, %10000000
	ldh [rBGPI], a
	ld a, $ff			; Color 0 (White/Transparent)
	ldh [rBGPD], a
	ldh [rBGPD], a
	ld a, $99			; Color 1 (Light grey)
	ldh [rBGPD], a
	ldh [rBGPD], a
	ld a, $33			; Color 2 (Dark grey)
	ldh [rBGPD], a
	ldh [rBGPD], a
	ld a, $00			; Color 3 (Black)
	ldh [rBGPD], a
	ldh [rBGPD], a

	ld	a,0
	ld	[rSCX], a
	ld	[rSCY], a

	; Copy font tiles to VRAM
	ld bc, TilesFontDataEnd-TilesFontData
	ld de, TilesFontData
	ld hl, vFontTiles	; Copy to tiles 0x20-0x3f
	CopyFont1bpp

	; Copy cover tilemap
	ld bc, CoverTilemapDataEnd-CoverTilemapData
	ld de, CoverTilemapData
	ld hl, _SCRN0
	CopyData

	; Copy logo tiles
	ld bc, TilesLogoDataEnd-TilesLogoData
	ld de, TilesLogoData
	ld hl, vCoverTiles
	CopyData
	
	; Show title
	ld bc, TitleDataEnd-TitleData
	ld de, TitleData
	ld hl, _SCRN0+1+(SCRN_VX_B*15)
	CopyData
	
	; Show playback rate
	ldh a, [workVarCapabilities]
	bit 0, a
	jr z, .skipDoubleRate
	ld bc, PlaybackDoubleRateDataEnd-PlaybackDoubleRateData
	ld de, PlaybackDoubleRateData
	jr .skipNormalRate
.skipDoubleRate:
	ld bc, PlaybackNormalRateDataEnd-PlaybackNormalRateData
	ld de, PlaybackNormalRateData
.skipNormalRate:
	ld hl, _SCRN0+1+(SCRN_VX_B*16)
	CopyData

	ld a, %10010001		; Screen on, Background on, tiles at $8000, OBJ off
	ldh [rLCDC], a

	; Wait for "new track" signal on serial
WaitForNewTrack:
	; Set serial output with rom capabilities
	ldh a, [workVarCapabilities]
	ldh [rSB], a

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
	
	; Trigger reset
	cp $f1
	; Or loop
	jr nz, WaitForNewTrack


NewTrack:
	di				; Disable interrupts
	
	xor a
	ldh [rNR52], a	; Disable sound

	ldh [rIF], a	; Clear requested/unserviced interrupts

	ldh [rIE], a	; All interrupts OFF
	ld sp, $FFFF	; Set stack pointer

.waitvbl:
	ldh a, [rLY]	; Read current line
	cp 144
	jr c, .waitvbl	; Loop if line 144 not reached

	xor a
	ldh [rLCDC], a 	; Disable display

UpdateCover:
	; Get cover tiles from serial: read 14*13*16=2912 bytes
	ld bc, vCoverTilesEnd-vCoverTiles
	ld hl, vCoverTiles
	CopyDataFromSerial

UpdateMetadata:
	; Get 18 character tilemap for artist
	ld bc, 18
	ld hl, _SCRN0+1+(SCRN_VX_B*15)
	CopyDataFromSerial

	; Get 18 character tilemap for track title
	ld bc, 18
	ld hl, _SCRN0+1+(SCRN_VX_B*16)
	CopyDataFromSerial

StartPlayback:
	ld a, %10010001		; Screen on, Background on, tiles at $8000, OBJ off
	ldh [rLCDC], a

	PlaybackAudioInit
	PlaybackTimerInit
	PlaybackSerialInit

	ei

.waitforInt:
	halt
	jr .waitforInt


SerialISR:
	PlaybackSerialISR


SECTION "Tiles", ROM0[$1000]

TilesFontData:
	chr_IBMPC1	2,3
TilesFontDataEnd:

TilesLogoData:
	INCBIN "logo.2bpp"
TilesLogoDataEnd:

TitleData:
	db $00, $00, $00, "P"-$20, "L"-$20, "A"-$20, "Y"-$20, $00, "I"-$20, "T"-$20, $00, "L"-$20, "O"-$20, "U"-$20, "D"-$20, $00, $00, $00
TitleDataEnd:

PlaybackNormalRateData:
	db $00, $00, $00, "N"-$20, "O"-$20, "R"-$20, "M"-$20, "A"-$20, "L"-$20, $00, "S"-$20, "P"-$20, "E"-$20, "E"-$20, "D"-$20, $00, $00, $00
PlaybackNormalRateDataEnd:

PlaybackDoubleRateData:
	db $00, $00, $00, "D"-$20, "O"-$20, "U"-$20, "B"-$20, "L"-$20, "E"-$20, $00, "S"-$20, "P"-$20, "E"-$20, "E"-$20, "D"-$20, $00, $00, $00
PlaybackDoubleRateDataEnd:

CoverTilemapData:
	db $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $40, $41, $42, $43, $44, $45, $46, $47, $48, $49, $4a, $4b, $4c, $4d, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $4e, $4f, $50, $51, $52, $53, $54, $55, $56, $57, $58, $59, $5a, $5b, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $5c, $5d, $5e, $5f, $60, $61, $62, $63, $64, $65, $66, $67, $68, $69, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $6a, $6b, $6c, $6d, $6e, $6f, $70, $71, $72, $73, $74, $75, $76, $77, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $78, $79, $7a, $7b, $7c, $7d, $7e, $7f, $80, $81, $82, $83, $84, $85, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $86, $87, $88, $89, $8a, $8b, $8c, $8d, $8e, $8f, $90, $91, $92, $93, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $94, $95, $96, $97, $98, $99, $9a, $9b, $9c, $9d, $9e, $9f, $a0, $a1, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $a2, $a3, $a4, $a5, $a6, $a7, $a8, $a9, $aa, $ab, $ac, $ad, $ae, $af, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $b0, $b1, $b2, $b3, $b4, $b5, $b6, $b7, $b8, $b9, $ba, $bb, $bc, $bd, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $be, $bf, $c0, $c1, $c2, $c3, $c4, $c5, $c6, $c7, $c8, $c9, $ca, $cb, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $cc, $cd, $ce, $cf, $d0, $d1, $d2, $d3, $d4, $d5, $d6, $d7, $d8, $d9, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $da, $db, $dc, $dd, $de, $df, $e0, $e1, $e2, $e3, $e4, $e5, $e6, $e7, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $e8, $e9, $ea, $eb, $ec, $ed, $ee, $ef, $f0, $f1, $f2, $f3, $f4, $f5, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
CoverTilemapDataEnd:


SECTION "VRAM Tiles", VRAM[$8000]

; Characters are shifted down by 0x20 to fit in the 0x00-0x3f range
vFontTiles:
	ds 64 * 16
vCoverTiles:
	ds 14 * 13 * 16
vCoverTilesEnd:
