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

; FIXME Fit ISR routine(s) before $150 ?
REPT $150 - $104
	db 0
ENDR


SECTION "Game code", ROM0[$150]

ResetGame:
	; TODO Clean stuff and restart

	; Disable all interrupts
	;di
	;xor a
	;ldh [rIE], a

	; Disable sound
	xor a
	ldh [rNR52], a

	; FIXME Need to clear more stuff ??
	; FIXME We are still in serial interrupt execution --> interrupts are disabled
	; TODO Clear rIF? (requested/unserviced interrupts)
	ldh [rIF], a

	; FIXME We could skip the init stuff (bg, palettes, font tiles) when resetting --> copy the first few instructions from Start until LCD is turned off, then jumpt to reading tiles from serial

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

	; TODO Show logo and 'play it loud!' at boot
	; TODO Then start playback with empty buffers (or just serial handler ?)
	; TODO When the source wants to play a new track, it sends $f1 which triggers a soft reset --> turn lcd off and wait for cover+metadata, then stream audio)
	; FIXME 14*14 tiles (centered) cover ?

	; Copy cover tiles to VRAM
;	ld bc, CoverTilesDataEnd-CoverTilesData
;	ld de, CoverTilesData
;	ld hl, vCoverTiles	; Copy to tiles 0x40-...
;	CopyData

	; TODO Copy cover tilemap
	ld bc, CoverTilemapDataEnd-CoverTilemapData
	ld de, CoverTilemapData
	ld hl, _SCRN0
	CopyData

	; Copy text into tilemap
;	ld bc, TitleDataEnd-TitleData
;	ld de, TitleData
;	ld hl, _SCRN0+4+(SCRN_VX_B*11)
;	CopyData

	;ld a, %10010011		; Screen on, Background on, tiles at $8000
	;ldh [rLCDC], a

	;PlaybackAudioInit
	;PlaybackTimerInit
	;PlaybackSerialInit
	;ClearAudioBuffers
	; TODO ClearCoverTiles --> default 'play it loud' logo ?? blank ??

	; TODO Switch serial to 'tiles' mode (writes to VRAM)
	; TODO Enable screen only after cover is received ? (or enable it for 1 frame only?) (or write to WRAM and update VRAM during (multiple) VBlank ISR?)
	;ReadySerialCoverTiles
	; TODO Wait long enough??? Wait for status change?


	; TODO Define protocol for serial sender: send cover tiles + tilemap for title/artist (fixed number of bytes? end delimiter ??) then send audio samples
	; TODO When changing track: stop playback and serial isr --> jump back to waiting for cover tiles and tilemap ? --> triggerred by an end delimiter in the samples data ???



	; Initialize tiles from serial: read 13*13*2=2704 bytes

	ld bc, vCoverTilesEnd-vCoverTiles
	ld hl, vCoverTiles
	CopyDataFromSerial

	; FIXME 18 character tilemap for artist
	ld bc, 18
	ld hl, _SCRN0+1+(SCRN_VX_B*15)
	CopyDataFromSerial

	; FIXME 18 character tilemap for track
	ld bc, 18
	ld hl, _SCRN0+1+(SCRN_VX_B*16)
	CopyDataFromSerial

	ld a, %10010011		; Screen on, Background on, tiles at $8000
	ldh [rLCDC], a

	; FIXME Should reset buffers position
	; FIXME Should start filling buffer from serial _before_ starting audio (audio on) playback (timer interrupt)

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

CoverTilesData:	; FIXME Should receive cover tiles from serial !!
	INCBIN "cover.2bpp"
CoverTilesDataEnd:

CoverTilemapData:
	db $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $40, $41, $42, $43, $44, $45, $46, $47, $48, $49, $4a, $4b, $4c, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $4d, $4e, $4f, $50, $51, $52, $53, $54, $55, $56, $57, $58, $59, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $5a, $5b, $5c, $5d, $5e, $5f, $60, $61, $62, $63, $64, $65, $66, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $67, $68, $69, $6a, $6b, $6c, $6d, $6e, $6f, $70, $71, $72, $73, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $74, $75, $76, $77, $78, $79, $7a, $7b, $7c, $7d, $7e, $7f, $80, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $81, $82, $83, $84, $85, $86, $87, $88, $89, $8a, $8b, $8c, $8d, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $8e, $8f, $90, $91, $92, $93, $94, $95, $96, $97, $98, $99, $9a, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $9b, $9c, $9d, $9e, $9f, $a0, $a1, $a2, $a3, $a4, $a5, $a6, $a7, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $a8, $a9, $aa, $ab, $ac, $ad, $ae, $af, $b0, $b1, $b2, $b3, $b4, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $b5, $b6, $b7, $b8, $b9, $ba, $bb, $bc, $bd, $be, $bf, $c0, $c1, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $c2, $c3, $c4, $c5, $c6, $c7, $c8, $c9, $ca, $cb, $cc, $cd, $ce, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $cf, $d0, $d1, $d2, $d3, $d4, $d5, $d6, $d7, $d8, $d9, $da, $db, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $dc, $dd, $de, $df, $e0, $e1, $e2, $e3, $e4, $e5, $e6, $e7, $e8, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	db $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
CoverTilemapDataEnd:


SECTION "VRAM Tiles", VRAM[$8000]

; FIXME 0x20-0x5f takes 64 tiles --> move down to 0x00-0x3f and encode all text by shifting characters -0x20 ?? tile 0 will be empty because mapped to 0x20 (space)

vFontTiles:
	ds 64 * 16
vCoverTiles:
	ds 13 * 13 * 16
vCoverTilesEnd:
; TODO Make vCoverTilesEnd align with 0x100 to only compare high byte?


SECTION	"Variables", WRAMX

ALIGN 8
wBuffer0:		DS	$400
wBuffer1:		DS	$400
wBuffersEnd:
