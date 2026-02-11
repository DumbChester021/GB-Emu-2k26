The Cartridge Header
Each cartridge contains a header, located at the address range $0100—$014F. The cartridge header provides the following information about the game itself and the hardware it expects to run on:

0100-0103 — Entry point
After displaying the Nintendo logo, the built-in boot ROM jumps to the address $0100, which should then jump to the actual main program in the cartridge. Most commercial games fill this 4-byte area with a nop instruction followed by a jp $0150.

0104-0133 — Nintendo logo
This area contains a bitmap image that is displayed when the Game Boy is powered on. It must match the following (hexadecimal) dump, otherwise the boot ROM won’t allow the game to run:

CE ED 66 66 CC 0D 00 0B 03 73 00 83 00 0C 00 0D
00 08 11 1F 88 89 00 0E DC CC 6E E6 DD DD D9 99
BB BB 67 63 6E 0E EC CC DD DC 99 9F BB B9 33 3E
The way the pixels are encoded is as follows: (more visual aid)

The bytes $0104—$011B encode the top half of the logo while the bytes $011C–$0133 encode the bottom half.
For each half, each nibble encodes 4 pixels (the MSB corresponds to the leftmost pixel, the LSB to the rightmost); a pixel is lit if the corresponding bit is set.
The 4-pixel “groups” are laid out top to bottom, left to right.
Finally, the monochrome models upscale the entire thing by a factor of 2 (leading to somewhat chunky pixels).
The Game Boy’s boot procedure first displays the logo and then checks that it matches the dump above. If it doesn’t, the boot ROM locks itself up.

The CGB and later models only check the top half of the logo (the first $18 bytes).

0134-0143 — Title
These bytes contain the title of the game in upper case ASCII. If the title is less than 16 characters long, the remaining bytes should be padded with $00s.

Parts of this area actually have a different meaning on later cartridges, reducing the actual title size to 15 ($0134–$0142) or 11 ($0134–$013E) characters; see below.

013F-0142 — Manufacturer code
In older cartridges these bytes were part of the Title (see above). In newer cartridges they contain a 4-character manufacturer code (in uppercase ASCII). The purpose of the manufacturer code is unknown.

0143 — CGB flag
In older cartridges this byte was part of the Title (see above). The CGB and later models interpret this byte to decide whether to enable Color mode (“CGB Mode”) or to fall back to monochrome compatibility mode (“Non-CGB Mode”).

Typical values are:

Value	Meaning
$80	The game supports CGB enhancements, but is backwards compatible with monochrome Game Boys
$C0	The game works on CGB only (the hardware ignores bit 6, so this really functions the same as $80)
Setting bit 7 will trigger a write of this register value to KEY0 register which sets the CPU mode.

0144–0145 — New licensee code
This area contains a two-character ASCII “licensee code” indicating the game’s publisher. It is only meaningful if the Old licensee is exactly $33 (which is the case for essentially all games made after the SGB was released); otherwise, the old code must be considered.

Sample licensee codes:

Code	Publisher
00	None
01	Nintendo Research & Development 1
08	Capcom
13	EA (Electronic Arts)
18	Hudson Soft
19	B-AI
20	KSS
22	Planning Office WADA
24	PCM Complete
25	San-X
28	Kemco
29	SETA Corporation
30	Viacom
31	Nintendo
32	Bandai
33	Ocean Software/Acclaim Entertainment
34	Konami
35	HectorSoft
37	Taito
38	Hudson Soft
39	Banpresto
41	Ubi Soft1
42	Atlus
44	Malibu Interactive
46	Angel
47	Bullet-Proof Software2
49	Irem
50	Absolute
51	Acclaim Entertainment
52	Activision
53	Sammy USA Corporation
54	Konami
55	Hi Tech Expressions
56	LJN
57	Matchbox
58	Mattel
59	Milton Bradley Company
60	Titus Interactive
61	Virgin Games Ltd.3
64	Lucasfilm Games4
67	Ocean Software
69	EA (Electronic Arts)
70	Infogrames5
71	Interplay Entertainment
72	Broderbund
73	Sculptured Software6
75	The Sales Curve Limited7
78	THQ
79	Accolade8
80	Misawa Entertainment
83	LOZC G.
86	Tokuma Shoten
87	Tsukuda Original
91	Chunsoft Co.9
92	Video System
93	Ocean Software/Acclaim Entertainment
95	Varie
96	Yonezawa10/S’Pal
97	Kaneko
99	Pack-In-Video
9H	Bottom Up
A4	Konami (Yu-Gi-Oh!)
BL	MTO
DK	Kodansha
0146 — SGB flag
This byte specifies whether the game supports SGB functions. The SGB will ignore any command packets if this byte is set to a value other than $03 (typically $00).

0147 — Cartridge type
This byte indicates what kind of hardware is present on the cartridge — most notably its mapper.

Code	Type
$00	ROM ONLY
$01	MBC1
$02	MBC1+RAM
$03	MBC1+RAM+BATTERY
$05	MBC2
$06	MBC2+BATTERY
$08	ROM+RAM 11
$09	ROM+RAM+BATTERY 11
$0B	MMM01
$0C	MMM01+RAM
$0D	MMM01+RAM+BATTERY
$0F	MBC3+TIMER+BATTERY
$10	MBC3+TIMER+RAM+BATTERY 12
$11	MBC3
$12	MBC3+RAM 12
$13	MBC3+RAM+BATTERY 12
$19	MBC5
$1A	MBC5+RAM
$1B	MBC5+RAM+BATTERY
$1C	MBC5+RUMBLE
$1D	MBC5+RUMBLE+RAM
$1E	MBC5+RUMBLE+RAM+BATTERY
$20	MBC6
$22	MBC7+SENSOR+RUMBLE+RAM+BATTERY
$FC	POCKET CAMERA
$FD	BANDAI TAMA5
$FE	HuC3
$FF	HuC1+RAM+BATTERY
11 No licensed cartridge makes use of this option. The exact behavior is unknown.
12 MBC3 with 64 KiB of SRAM refers to MBC30, used only in Pocket Monsters: Crystal Version (the Japanese version of Pokémon Crystal Version).
0148 — ROM size
This byte indicates how much ROM is present on the cartridge. In most cases, the ROM size is given by 32 KiB × (1 << <value>):

Value	ROM size	Number of ROM banks
$00	32 KiB	2 (no banking)
$01	64 KiB	4
$02	128 KiB	8
$03	256 KiB	16
$04	512 KiB	32
$05	1 MiB	64
$06	2 MiB	128
$07	4 MiB	256
$08	8 MiB	512
$52	1.1 MiB	72 13
$53	1.2 MiB	80 13
$54	1.5 MiB	96 13
13 Only listed in unofficial docs. No cartridges or ROM files using these sizes are known. As the other ROM sizes are all powers of 2, these are likely inaccurate. The source of these values is unknown.
0149 — RAM size
This byte indicates how much RAM is present on the cartridge, if any.

If the cartridge type does not include “RAM” in its name, this should be set to 0. This includes MBC2, since its 512 × 4 bits of memory are built directly into the mapper.

Code	SRAM size	Comment
$00	0	No RAM
$01	–	Unused 14
$02	8 KiB	1 bank
$03	32 KiB	4 banks of 8 KiB each
$04	128 KiB	16 banks of 8 KiB each
$05	64 KiB	8 banks of 8 KiB each
14 Listed in various unofficial docs as 2 KiB. However, a 2 KiB RAM chip was never used in a cartridge. The source of this value is unknown.
Various “PD” ROMs (“Public Domain” homebrew ROMs, generally tagged with (PD) in the filename) are known to use the $01 RAM Size tag, but this is believed to have been a mistake with early homebrew tools, and the PD ROMs often don’t use cartridge RAM at all.

014A — Destination code
This byte specifies whether this version of the game is intended to be sold in Japan or elsewhere.

Only two values are defined:

Code	Destination
$00	Japan (and possibly overseas)
$01	Overseas only
014B — Old licensee code
This byte is used in older (pre-SGB) cartridges to specify the game’s publisher. However, the value $33 indicates that the New licensee codes must be considered instead. (The SGB will ignore any command packets unless this value is $33.)

Here is a list of known Old licensee codes (source).

HEX	Licensee
00	None
01	Nintendo
08	Capcom
09	HOT-B
0A	Jaleco
0B	Coconuts Japan
0C	Elite Systems
13	EA (Electronic Arts)
18	Hudson Soft
19	ITC Entertainment
1A	Yanoman
1D	Japan Clary
1F	Virgin Games Ltd.3
24	PCM Complete
25	San-X
28	Kemco
29	SETA Corporation
30	Infogrames5
31	Nintendo
32	Bandai
33	Indicates that the New licensee code should be used instead.
34	Konami
35	HectorSoft
38	Capcom
39	Banpresto
3C	Entertainment Interactive (stub)
3E	Gremlin
41	Ubi Soft1
42	Atlus
44	Malibu Interactive
46	Angel
47	Spectrum HoloByte
49	Irem
4A	Virgin Games Ltd.3
4D	Malibu Interactive
4F	U.S. Gold
50	Absolute
51	Acclaim Entertainment
52	Activision
53	Sammy USA Corporation
54	GameTek
55	Park Place15
56	LJN
57	Matchbox
59	Milton Bradley Company
5A	Mindscape
5B	Romstar
5C	Naxat Soft16
5D	Tradewest
60	Titus Interactive
61	Virgin Games Ltd.3
67	Ocean Software
69	EA (Electronic Arts)
6E	Elite Systems
6F	Electro Brain
70	Infogrames5
71	Interplay Entertainment
72	Broderbund
73	Sculptured Software6
75	The Sales Curve Limited7
78	THQ
79	Accolade8
7A	Triffix Entertainment
7C	MicroProse
7F	Kemco
80	Misawa Entertainment
83	LOZC G.
86	Tokuma Shoten
8B	Bullet-Proof Software2
8C	Vic Tokai Corp.17
8E	Ape Inc.18
8F	I’Max19
91	Chunsoft Co.9
92	Video System
93	Tsubaraya Productions
95	Varie
96	Yonezawa10/S’Pal
97	Kemco
99	Arc
9A	Nihon Bussan
9B	Tecmo
9C	Imagineer
9D	Banpresto
9F	Nova
A1	Hori Electric
A2	Bandai
A4	Konami
A6	Kawada
A7	Takara
A9	Technos Japan
AA	Broderbund
AC	Toei Animation
AD	Toho
AF	Namco
B0	Acclaim Entertainment
B1	ASCII Corporation or Nexsoft
B2	Bandai
B4	Square Enix
B6	HAL Laboratory
B7	SNK
B9	Pony Canyon
BA	Culture Brain
BB	Sunsoft
BD	Sony Imagesoft
BF	Sammy Corporation
C0	Taito
C2	Kemco
C3	Square
C4	Tokuma Shoten
C5	Data East
C6	Tonkin House
C8	Koei
C9	UFL
CA	Ultra Games
CB	VAP, Inc.
CC	Use Corporation
CD	Meldac
CE	Pony Canyon
CF	Angel
D0	Taito
D1	SOFEL (Software Engineering Lab)
D2	Quest
D3	Sigma Enterprises
D4	ASK Kodansha Co.
D6	Naxat Soft16
D7	Copya System
D9	Banpresto
DA	Tomy
DB	LJN
DD	Nippon Computer Systems
DE	Human Ent.
DF	Altron
E0	Jaleco
E1	Towa Chiki
E2	Yutaka # Needs more info
E3	Varie
E5	Epoch
E7	Athena
E8	Asmik Ace Entertainment
E9	Natsume
EA	King Records
EB	Atlus
EC	Epic/Sony Records
EE	IGS
F0	A Wave
F3	Extreme Entertainment
FF	LJN
1 Later known as Ubisoft.
2 Later succeeded by Blue Planet Software, then acquired by The Tetris Company in 2020.
3 Later known as Virgin Mastertronic Ltd., then Virgin Interactive Entertainment, then Avalon Interactive Group, Ltd..
4 Later known as LucasArts between 1990-2021.
5 Later known as Atari SA.
6 Later accquired by Iguana Entertainment in 1995. Parent studio owned by Acclaim Entertainment.
7 Later known as SCi (Sales Curve Interactive), then SCi Entertainment Group plc, then Eidos, then acquired by Square Enix in 2009.
9 Later known as Spike Chunsoft Co., Ltd..
16 Later known as Kaga Create.
17 Known as Vic Tokai Corporation until 2011 when the name changed to Tokai Communications.
8 Later Infogrames North America, Inc.
15 Later named Caesars Entertainment, Inc.
18 Now known as Creatures, Inc.
19 Not to be confused with the IMAX motion picture film format.
10 Merged into Sega as Sega-Yonezawa, later becoming Sega Toys, and finally Sega Fave.
014C — Mask ROM version number
This byte specifies the version number of the game. It is usually $00.

014D — Header checksum
This byte contains an 8-bit checksum computed from the cartridge header bytes $0134–014C. The boot ROM computes the checksum as follows:

uint8_t checksum = 0;
for (uint16_t address = 0x0134; address <= 0x014C; address++) {
    checksum = checksum - rom[address] - 1;
}
The boot ROM verifies this checksum. If the byte at $014D does not match the lower 8 bits of checksum, the boot ROM will lock up and the program in the cartridge won’t run.

014E-014F — Global checksum
These bytes contain a 16-bit (big-endian) checksum simply computed as the sum of all the bytes of the cartridge ROM (except these two checksum bytes).

This checksum is not verified, except by Pokémon Stadium’s “GB Tower” emulator (presumably to detect Transfer Pak errors).