INCLUDE "hardware.inc"
INCLUDE "ibmpc1.inc"
INCLUDE "macros.inc"
INCLUDE "audio.inc"
INCLUDE "timer.inc"
INCLUDE "serial.inc"
INCLUDE "joypad.inc"


SECTION "VBlank", ROM0 [$40]
	reti

SECTION "Timer", ROM0[$50]
	jp TimerISR

SECTION "Serial", ROM0 [$58]
	jp SerialISR

; TODO Joypad ISR ?? mute, commands, debug...
SECTION "Joypad", ROM0 [$60]
	jp JoypadISR

SECTION "Header", ROM0[$100]

EntryPoint:
	di
	jp Start

REPT $150 - $104
	db 0
ENDR


SECTION "Game code", ROM0[$150]

Start:
	di
	xor a
	ldh [rIE], a	; All interrupts OFF
	ld sp, $FFFF	; Set stack pointer

IF __ENABLE_DOUBLE_SPEED__ == 1
	; Switch to double speed
	ld a, $30
	ldh [rP1], a
	ld a, $01
	ldh [rKEY1], a
	stop
	nop
	nop
ENDC

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
	
IF __DEBUG__ == 1
	; Show playback rate
	ld bc, PlaybackRateDataEnd-PlaybackRateData
	ld de, PlaybackRateData
	ld hl, _SCRN0+1+(SCRN_VX_B*16)
	CopyData
ENDC

	ld a, %10010011		; Screen on, Background on, tiles at $8000
	ldh [rLCDC], a


	xor a
	ldh [rIF], a	; Clear requested/unserviced interrupts
	ldh [rIE], a	; All interrupts OFF
	ld sp, $FFFF	; Set stack pointer
	
	; FIXME Enable joypad interrupt to trigger newtrack ?
	JoypadInit
	ReadyJoypad
	ei

	; Wait for "new track" signal on serial
WaitForNewTrack:
	; Set constant serial output
	ld a, $f2
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
	; FIXME we use stack pointer as a communication register between playback and serial ISR!!
	ld sp, $FFFF	; Set stack pointer
	;ld sp, $0000	; Set stack pointer

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

	ld a, %10010011		; Screen on, Background on, tiles at $8000
	ldh [rLCDC], a

	; FIXME Enable joypad interrupt to trigger newtrack ?

	PlaybackAudioInit
	PlaybackTimerInit
	PlaybackSerialInit
	JoypadInit
	ClearAudioBuffers

	ReadySerial
	ReadyTimer
	ReadyJoypad

	ei

	; fixme just HALT ??
.waitforInt:
	halt
	jr .waitforInt


TimerISR:
	PlaybackTimerISR

SerialISR:
	PlaybackSerialISR

JoypadISR:
	DoJoypadISR


SECTION "Tiles", ROM0[$1000]

TilesFontData:
	chr_IBMPC1	2,3
TilesFontDataEnd:

TilesLogoData:
	INCBIN "logo.2bpp"
TilesLogoDataEnd:

TitleData:
IF __ENABLE_DOUBLE_SPEED__ == 1
	db $00, $00, "P"-$20, "L"-$20, "A"-$20, "Y"-$20, $00, "I"-$20, "T"-$20, $00, "L"-$20, "O"-$20, "U"-$20, "D"-$20, "E"-$20, "R"-$20, $00, $00
ELSE
	db $00, $00, $00, "P"-$20, "L"-$20, "A"-$20, "Y"-$20, $00, "I"-$20, "T"-$20, $00, "L"-$20, "O"-$20, "U"-$20, "D"-$20, $00, $00, $00
ENDC
TitleDataEnd:

IF __DEBUG__ == 1
PlaybackRateData:
IF __ENABLE_DOUBLE_SPEED__ == 1
	db $00, $00, $00, $00, $00, "1"-$20, "6"-$20, "3"-$20, "8"-$20, "4"-$20, $00, "H"-$20, "Z"-$20, $00, $00, $00, $00, $00
;	db $00, $00, $00, $00, $00, "1"-$20, "2"-$20, "4"-$20, "8"-$20, "3"-$20, $00, "H"-$20, "Z"-$20, $00, $00, $00, $00, $00
ELSE
	db $00, $00, $00, $00, $00, "8"-$20, "1"-$20, "9"-$20, "2"-$20, $00, $00, "H"-$20, "Z"-$20, $00, $00, $00, $00, $00
;	db $00, $00, $00, $00, $00, "6"-$20, "2"-$20, "4"-$20, "1"-$20, $00, $00, "H"-$20, "Z"-$20, $00, $00, $00, $00, $00
ENDC
PlaybackRateDataEnd:
ENDC

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


SECTION	"Variables", WRAMX

ALIGN 8
wBuffer0:		DS	$400
wBuffer1:		DS	$400
wBuffersEnd:
