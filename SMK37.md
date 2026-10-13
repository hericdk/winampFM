# winampFM on the M-Vave SMK-37

The M-Vave SMK-37 has the FM-1's chip (JieLi AC791N), so winampFM runs on it as a target board: `BOARD=smk37`.
Everything SMK-37-specific sits behind `BOARD_SMK37`; the FM-1 build is unchanged, byte for byte. winampFM's
screens, windows and controls work as on the FM-1 (see the [README](README.md) and the site); the SMK-37's extra
controls add to them.

> Unofficial firmware for the SMK-37, not affiliated with or endorsed by M-Vave. No warranty: install at your own
> risk and read [Safety](#safety) first.

## Models

| Model | Build | Package identity |
|---|---|---|
| SMK-37 Pro | `BOARD=smk37 sh firmware/build.sh` | `SMK-37 Pro_900` (`firmware/build/winampfm-smk37.fwsc`) |
| SMK-37 Elite | `BOARD=smk37 MODEL=elite sh firmware/build.sh` | `SMK-37 Elite_900` (`firmware/build/winampfm-smk37-elite.fwsc`) |
| MKE-P37 | `BOARD=smk37 MODEL=mke sh firmware/build.sh` | `MKE-P37_900` (`firmware/build/winampfm-smk37-mke.fwsc`) |
| Starrykey 37 Play | the SMK-37 Pro build | `SMK-37 Pro_900` |

They are one keyboard under several names. The `MODEL` builds differ only in the name the keyboard reports (its
identity, its update loader's, the update record's), so M-Vave's updater still recognises the unit for the way back
to the stock firmware. Toolchain and SDK: [firmware/BUILDING.md](firmware/BUILDING.md). Both boards write the same
files in `firmware/build/`: copy each SMK-37 package out before the next build. The 3D simulator's firmware can be
built for the SMK-37 too: `BOARD=smk37 sh firmware/build-wasm.sh` (`sim/winampfm-smk37.wasm`).

## Install

Connect the keyboard with a USB data cable. It shows up as the MIDI port `Felucca` (`Felucca Update` while it
updates), as on the FM-1.

```sh
python3 firmware/tools/fm1_install.py firmware/build/winampfm-smk37.fwsc   # Python 3 with mido and python-rtmidi
```

The installer finds the keyboard, checks the model and the package, asks the running firmware (the stock one or
winampFM) to start the update loader and writes the package; then the keyboard restarts and the installed identity
is checked. The site's installer does the same in Chrome or Edge once the site lists the SMK-37 packages
(`SMK_PKGS` in `index.html`): it writes the package of the connected model. A package never goes to the other
device (FM-1 / SMK-37), and a keyboard running a firmware with its own updater is left alone.

Until this project's releases include SMK-37 packages, unofficial builds can be installed with the web installer:
<https://genkigenki.github.io/smk37/>.

**First start.** winampFM keeps its projects, presets and settings in the keyboard's own storage area, tagged as
winampFM's. The first start clears what another firmware left there (the stock settings, another port's projects).
The factory calibration, Bluetooth pairing and the stock resources are outside that area and never touched.
USR1 and USR2 hold user samples; USR3 and USR4 have no flash on the SMK-37 and stay empty.

**Back to the stock firmware:** install the official package for your model with M-Vave's own updater
(M-UPGRADE), or give it to the installer: `python3 firmware/tools/fm1_install.py --force SMK-37_Pro_016.fwsc`.
winampFM keeps the stock update protocol, so the stock tools see the keyboard as usual.

**If it does not start:** after two failed starts winampFM opens its USB recovery screen (identity `SMK-37 Pro_000`,
or the model's), where the installer can write a package again; after a third it drops into the chip's mask-ROM
update mode (UBOOT). OCTAVE− held at power-on opens the USB recovery screen at once.

## Knobs, faders, wheels

| FM-1 printed | winampFM | SMK-37 printed | on the SMK-37 |
|---|---|---|---|
| MASTER (pot) | output volume | K1 | master volume (a virtual pot, 24 steps a click) |
| SELECT | the slider group (SEND, SPACE, MASTER, PARTY, TRACK), the visualizer's picture, a page's tabs, the tempo | K2 | SELECT |
| ALGORITHM | the track: 1, 2, 3, DRUMS | K3 | ALGORITHM |
| PRESETS | the playlist: scroll and load the sound (the kit on DRUMS); TAPES: spin | K4 | PRESETS |
| KNOB 1–4 | the group's four sliders, a page's four values | K5–K8 | KNOB 1–4 |
| — | — | FADER 1–4 | tracks 1, 2, 3 and DRUMS, by FADER BANK: A level, B filter (low-pass left, open in the middle, high-pass right), C reverb send, D delay send. A fader takes over once it moves, also after a bank change. |
| — | — | PITCH wheel | pitch bend, ±2 semitones: the selected track (DRUMS selected: the three synth tracks) |
| — | — | MOD wheel | vibrato on the same tracks, 10 Hz, up to ±1 semitone (up = more) |
| — | — | PEDAL jack | not used |

## Buttons

| FM-1 printed | winampFM | SMK-37 printed | on the SMK-37 |
|---|---|---|---|
| PLAY | play / stop; held: TAPES | SEQ PLAY | PLAY |
| REC | arm, close the loop; held: clear the track; FX + REC: reset the track's effects | SEQ REC | REC |
| HOME | the next window (PLAYLIST, EFFECTS, VISUALIZER, TAPES); held: PREFERENCES | PATCH | HOME; also closes the FADER BANK and PAD SOUNDS windows |
| ENV | envelope pages | CHORD | ENV |
| LFO | LFO pages | NOTE REPEAT | LFO |
| FX | effect pages; held: the punch-in effects on the keys | FX | FX |
| SEL | key and chord pages; held: key layer; TAPES: copy into the track | SCALE | SEL; held, the pads are a piano (the scale green, its root brighter) |
| EDIT | sound pages; held: erase layer | PARA | EDIT |
| ARP | arpeggiator pages; held: note repeat | ARP | ARP; lit while the selected track's arpeggiator runs |
| SEQ | step pages; held: step layer | SEQ | SEQ |
| GLO | global pages; held: mix layer (mute, solo, fill) | GLOBE | GLO |
| SAVE | song screen; held: song layer | BT | SAVE; held, the pads show the sections (used green, free white) |
| OCT− / OCT+ | octave; drums: ghost / hard hits | OCTAVE− / OCTAVE+ | OCT− / OCT+ |
| — | — | KNOB BANK | opens and closes the PAD SOUNDS window (lit while open) |
| — | — | FADER BANK | opens the FADER BANK window; pressed again, `<` `>` or K2: the next bank A → B → C → D (closes after 4 s; lit while up) |
| — | — | `<` `>` | as a SELECT click, never the tempo: on the player the slider group (on the visualizer its picture), on a page its tabs; SEQ held: the step pages |

One window of these at a time: FADER BANK closes PAD SOUNDS, KNOB BANK closes FADER BANK.

## Keys and pads

| FM-1 | winampFM | SMK-37 | on the SMK-37 |
|---|---|---|---|
| 27 keys F3–G5 | notes; in layers: tiles (steps, effects, mix, sections) | keys F3–G5 (the middle 27) | the same, with velocity |
| — | — | keys C3–E3 (the lowest 5) | synth tracks: notes; DRUMS: C3 kick, D3 snare, E3 hat (a black key plays its left white neighbour) |
| — | — | keys G#5–C6 (the highest 5) | synth tracks: notes; DRUMS: G#5 cowbell, A5 open hat, B5 crash, C6 ride |
| LEDs under the 27 keys | what is on or playing | 16 RGB pads | the 16 white keys F3–G5 with velocity: as steps and tiles top row 1–8, bottom row 9–16; as notes the low ones at the bottom |

The pads show the selected track's loop in the colour its notes have in the player's seek bar (track 1 orange,
2 blue, 3 green, DRUMS yellow): an empty step pale, a step with notes brighter, the playhead and what sounds
brightest. In a layer they show its tiles the same way. A pad used as a tile or a step fires once per press. The
power-on light wave runs over the pads.

## The PAD SOUNDS window

KNOB BANK shows the sounds of one kind on the 16 pads and on the screen (2 × 8, in the pads' order and colours):
basses, keys, organs, pads, leads, plucks, stabs, effects, then the user presets, in the playlist's order, 16 a page;
on DRUMS the kits. It opens at the selected sound's page. A pad loads its sound and plays a short preview; `<` `>`
step the pages; KNOB BANK or PATCH closes it. Turning K4 keeps winampFM's own playlist on the screen.

## Menu

PREFERENCES (PATCH held) has the FM-1's rows except KEYBOARD LED and BRIGHTNESS: the SMK-37 has no key LEDs, and
its screen's backlight is always on.

## Not available on the SMK-37

- TRS MIDI IN (and MIDI clock over TRS): the FM-1's MIDI IN pin is a key-matrix row on the SMK-37. USB MIDI works
  as on the FM-1.
- User sample slots USR3 and USR4 (no flash behind them).

## Safety

- **Back up the whole 1 MiB flash first**, over the chip's mask-ROM UBOOT mode. The stock firmware has no way into
  UBOOT, so you need a USB_KEY dongle (as for the FM-1); with winampFM installed, hold OCTAVE− and OCTAVE+ for
  5 seconds while stopped. Keep the dump private: it holds data unique to your unit.
- **Never write 0x0–0x3FFF, 0xC0000–0xC1FFF** (Bluetooth pairing and the factory key and pad calibration),
  **0xF4000–0xFDFFF** (the stock resources) **or anything from 0xFF000**: they are unique to each unit or needed to
  start it, and cannot be recreated. The firmware and its update loader write only an explicit allow-list of flash
  ranges (checked at compile time and again in the flash driver), but a manual UBOOT write can; restore only the
  ranges you need.
- `FELUCCA_UART` must stay 0 on the SMK-37: the build refuses otherwise.
- Power the keyboard from a reliable USB port during an update and do not unplug it while the update screen is up.
