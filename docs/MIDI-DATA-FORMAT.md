# Yamaha MODX M (and Montage): MIDI data format

Sources: MODX M Data List (En_B0), "MIDI Data Format" and "MIDI Data Table" sections; Montage Data List (En_D0) for the comparison at the bottom. The PDF layout is two columns, so check any value you plan to hard-code against the real MODX M. Values are copied as printed.

## Channel messages the MODX M handles
Receive and transmit unless noted. All control changes are recorded and played back by the sequencer part.

| CC | Function | Notes |
|---|---|---|
| 0 / 32 | Bank Select MSB / LSB | see bank table below; applied on Program Change |
| 1 | Modulation | receive and transmit |
| 5 | Portamento Time | |
| 6 / 38 | Data Entry MSB / LSB | used with RPN |
| 7 | Channel Volume | |
| 10 | Pan | |
| 11 | Expression | receive only in the list |
| 64 | Sustain | |
| 65 | Portamento Switch | |
| 66 | Sostenuto | |
| 71 | Harmonic Content (received) / Filter Resonance (transmitted) | offset around 64, as printed |
| 72 | EG Release Time | offset around 64 |
| 73 | EG Attack Time | offset around 64 |
| 74 | Brightness (received) / Filter Cutoff (transmitted) | offset around 64 |
| 75 | EG Decay Time | offset around 64 |
| 91 | Effects 1 Depth (reverb send) | |
| 94 | Effects 4 Depth (variation send) | |
| 96 / 97 | Data Entry Inc / Dec | |
| 1 to 95 | Assignable controllers | the control number is set per performance |

Default control numbers of the assignable controllers (as printed): Breath controller 2; Foot controller 1: 11; Foot controller 2: Super Knob; Assignable knobs 1 to 8: 17 to 24; Ribbon controller 16; Assignable function 1 and 2: 86 and 87; Motion Seq trigger 89; Foot switch: Arp SW. The printed table is loosely aligned in the PDF, so confirm each pairing on the unit.

Channel mode: All Sound Off 78H, Reset All Controllers 79H, All Notes Off 7BH, Omni Off 7CH, Omni On 7DH, Mono 7EH, Poly 7FH. Other messages: Program Change, Channel Aftertouch, Pitch Bend (10-bit), Key Aftertouch (receive), Song Position, Timing Clock, Start, Continue, Stop, Active Sensing.

RPN (CC 101 MSB, 100 LSB, then Data Entry): 0/0 Pitch Bend Sensitivity (0 to 24 semitones), 0/1 Master Fine Tune, 0/2 Master Coarse Tune (28H to 58H, 40H = 0), 7F/7F RPN reset.

## Bank select and program numbers
Program number 0 to 127 means patch 1 to 128. Banks as printed:
- GM Voice: MSB 0, LSB 0. GM Drum: MSB 127, LSB 0.
- Performance (single part): Preset MSB 63 LSB 0 to 39 (Preset 1 to 40); User MSB 64 LSB 0 to 4; Library MSB 64 LSB 8 to 127 (Library 1 to 120).
- Performance (multi part): Preset MSB 65 LSB 0 to 39; User MSB 66 LSB 0 to 4; Library MSB 66 LSB 8 to 127.
- Live Set: MSB 62, LSB 0 to 15 (page 1 to 16), receive only.
Bank Select takes effect when the Program Change arrives. Unsupported numbers are ignored.

## System exclusive
All Yamaha messages: `F0 43 <type|device> 7F 1C 0D <data> F7`. Group ID `7F 1C`, Model ID `0D` for the MODX M. The device number `n` is the System MIDI device number setting (0 to 15).

| Message | Format |
|---|---|
| Parameter change | `F0 43 1n 7F 1C 0D a1 a2 a3 a4 dd... F7` |
| Bulk dump | `F0 43 0n 7F 1C 0D bc bc a1 a2 a3 a4 dd... cs F7` |
| Bulk dump request | `F0 43 2n 7F 1C 0D a1 a2 a3 a4 F7` |
| Parameter request | `F0 43 3n 7F 1C 0D a1 a2 a3 a4 F7` |
| Master volume | `F0 7F 7F 04 01 ll mm F7` (only mm is used) |
| GM mode on | `F0 7E 7F 09 01 F7` |
| Identity request | `F0 7E 0n 06 01 F7` (receives as omni) |
| Identity reply | `F0 7E 7F 06 02 43 00 41 dd dd mm 00 00 7F F7` |

- `bc bc`: byte count, two 7-bit bytes. Addresses are four 7-bit bytes.
- Checksum: the 7-bit value that makes the low 7 bits of (model ID + address + data + checksum) equal 0.
- Multi-byte parameters send as many data bytes as the parameter needs.
- Identity reply device family: MODX M6 `6E 06`, M7 `6F 06`, M8 `70 06`. `mm` is the version: (version minus 1.0) times 10.
- A Performance bulk dump (header to footer) can be received in any block order, but not with unrelated blocks mixed in. To request a whole Performance, request its Bulk Header address.

## Address map (top addresses, four bytes)
Where `p` is a part number (0 to 15), `m`, `c`, `k`, `L`, `bb` are indexes as shown.

| Block | Address | Bytes |
|---|---|---|
| System | 00 00 00 00 | 78 |
| Sequencer setup | 00 00 01 00 | 22 |
| Remote | 00 00 03 00 | 112 |
| Micro tuning user table n | 00 01 0n 00 | 24 |
| Live Set bank name | 03 00 00 00 | 20 |
| Live Set page name (page p) | 03 1p 00 00 | 20 |
| Live Set slot name (page p, slot s) | 03 2p 0s 00 | 20 |
| Live Set slot | 03 3p 0s 00 | 18 |
| Live Set slot audio file path | 03 4p 0s 00 | 256 |
| Live Set slot identifier | 03 5p 0s 00 | 32 |
| Bulk header / footer | 04 .. .. .. / 05 .. .. .. | 0 |
| Performance name | 06 00 00 00 | 20 |
| Performance common, 1 byte params | 06 00 01 00 | 29 |
| Performance common, 2 byte params | 06 00 02 00 | 86 |
| Performance common controller | 06 00 03 00 | 86 |
| Reverb | 06 00 07 00 | 52 |
| Variation | 06 00 08 00 | 52 |
| VCM rotary speaker | 06 00 09 00 | 68 |
| Master EQ | 06 00 0A 00 | 34 |
| Master effect | 06 00 0B 00 | 52 |
| Motion Seq common settings | 06 00 0C 00 | 12 |
| Super Knob settings | 06 00 0D 00 | 90 |
| Audio In parts (A/D) / (USB) | 06 00 0E 00 / 06 00 0F 00 | 42 / 14 |
| Super Knob lane settings | 06 00 12 00 | 20 |
| Super Knob sequence m | 06 01 0m 00 | 102 |
| Scene c common, 1 byte / 2 byte | 06 02 0c 00 / 06 03 0c 00 | 19 / 44 |
| Assignable knob name k | 06 04 0k 00 | 16 |
| Part name (part p) | 1p 00 00 00 | |
| Part 1 byte / 2 byte params | 1p 00 01 00 / 1p 00 02 00 | |
| Part 2 byte pitch/effect | 1p 00 03 00 | |
| Insertion A / B | 1p 00 04 00 / 1p 00 05 00 | |
| Arpeggio part | 1p 00 06 00 | |
| LFO | 1p 00 07 00 | |
| Zone setting | 1p 00 08 00 | |
| Scene part (scene c) | 1p 03 0c 00 | |
| Part controller box (bb) | 1p 05 bb 00 | |
| Motion Seq part/lane settings | 1p 06 L0 00 / 1p 07 L0 00 | |
| Drum part key (kk = 0 to 72) | 2p 10 kk 00 | |
| FM-X part: common / filter / operator o | 3p 00 00 00 / 3p 00 01 00 / 3p 02 0o 00 | |
| AN-X part: common / oscillator o / filter f / wave folder | 4p 00 00 00 / 4p 02 0o 00 / 4p 04 0f 00 / 4p 05 00 00 | |
| VA part: common / FM operator o | 7p 00 00 00 / 7p 01 0o 00 | |

Byte counts for the per-part blocks and the parameter-by-parameter tables (the pages after the address map) are not copied here: they run for about 4,000 lines in the PDF. Generate them into a data file with a script before using them in code, and keep the generator in the repo.

## MODX M versus Montage
Both lists use the same message layout. The differences that matter for a controller:
- Model ID: MODX M `0D`, Montage `02`.
- Address size: MODX M uses four address bytes; Montage uses three (high, mid, low).
- Identity reply device family: MODX M6/M7/M8 `6E/6F/70 06`; Montage 6/7/8 `4E/4F/50 06`.
- Group ID is `7F 1C` on both.
Plan: abstract the address size and model ID so one backend can drive both.
