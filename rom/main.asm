INCLUDE "hardware.inc"
INCLUDE "ibmpc1.inc"
INCLUDE "macros.inc"
INCLUDE "audio.inc"
INCLUDE "timer.inc"
INCLUDE "serial.inc"


SECTION "VBlank", ROM0 [$40]
	reti

SECTION "Timer", ROM0[$50]
	jp TimerISR

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
	di
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

	; TODO Clear screen (on DMG, Nintendo Logo stays on)

	; Copy text into tilemap
	ld bc, TitleDataEnd-TitleData
	ld de, TitleData
	ld hl, _SCRN0+4+(SCRN_VX_B*11)
	CopyData

	ld a, %10010011		; Screen on, Background on, tiles at $8000
	ldh [rLCDC], a

	PlaybackAudioInit
	PlaybackTimerInit
	PlaybackSerialInit
	ClearAudioBuffers

	ReadySerial
	ReadyTimer

	ei

.waitforInt:
	jr .waitforInt


TimerISR:
	PlaybackTimerISR

SerialISR:
	PlaybackSerialISR


SECTION "Tiles", ROM0[$1000]

TilesFontData:
	chr_IBMPC1	2,3
TilesFontDataEnd:

TitleData:
	db "PLAY IT LOUD"
TitleDataEnd:


SECTION "VRAM Tiles", VRAM[$8000]
vEmptyTile:
	ds 1 * 16
vUnusedTiles:
	ds 31 * 16
vFontTiles:
	ds 32 * 16


SECTION	"Variables", WRAMX

ALIGN 8
wBuffer0:		DS	$400
wBuffer1:		DS	$400
wBuffersEnd:
