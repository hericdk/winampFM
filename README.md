# winampFM Synth

**A Winamp 2.x-style firmware for the M-VAVE FM-1.** Your sounds become a playlist, four tracks record as loops, effects sit on sliders and a visualizer is made only of lines. Launch version **v0.1**, based on **SLOOP 2.6**.

- **Site, installer and 3D simulator (the real firmware in your browser, with sound):** https://hericdk.github.io/winampFM/
- **Install on your FM-1:** open the site in Chrome or Edge, connect the FM-1 by USB and press install. The package is also in [`release/winampfm-0.1.fwsc`](release/).
- **License:** GPL-3.0-only, like SLOOP, Felucca and FM1 Quest.

## What is unique to winampFM

These were made for winampFM, on top of everything SLOOP already does:

| Feature | What it does |
|---|---|
| **Winamp main window** | The home screen is a Winamp 2.x window: time counter, spectrum, BPM, the current sound in a scrolling marquee, and the four tracks as buttons. |
| **The sound list as a playlist** | Every sound is a song in a Winamp playlist. Scroll it and the sound loads on the track you are on. |
| **Reel-to-reel recording** | Arm REC and play: a tape deck shows the real take (input meter, counter, notes, passes). The bolt turns into a red LED. REC again closes the loop and takes the tempo from what you played. |
| **Effects on sliders** | SEND · SPACE · MASTER · PARTY · TRACK groups as Winamp equalizer sliders, moved by the four knobs. The line above them reacts to the sound. |
| **PARTY** | Decides who is on stage: SOLO, DUEL, TRIAD or everyone. AUTOTURN changes the stage in time with the song; KEEP holds a lead while the others take turns; YOYO walks across the tracks and back. |
| **TAPES** | Every sound is a cassette in a PS1-style carousel. Stop on a tape to hear it, then copy the sound and its phrase into your track. Autoplay can be switched off in the menu. |
| **Visualizer** | Pixel art made of lines, driven by the sound: SCOPE, BARS, MILK, RAYS, TUNNEL. SELECT changes it. |
| **Punch-in carousel** | Hold FX and the black keys show the 27 performance effects as a moving carousel over a keyboard, with REV/ECHO THROW on the octave keys. |
| **Brazilian sounds** | CAVAQUINHO, BERIMBAU, AGOGO BELL, SANFONA and BRASIL drum kits, next to all 153 SLOOP 2.6 sounds (157 in total). |
| **Easier navigation** | SELECT cycles the page tabs with wrap-around; FILT/MIX in one FX tab; screen brightness, LED colours and boot mute in the menu. |
| **3D simulator** | The real firmware compiled to WebAssembly, in a 3D model of the FM-1, with a tutorial bar that plays demos by itself. |
| **Screen recorder widget** | A small Win98-style window on the site records the simulator screen and sound, to save, share or lay over your own videos. |

Merged from SLOOP 2.4, 2.5 and 2.6 (features the community asked for): FM6 engine, PHYS and NOISE engines with 77 new sounds, INSERT per track, LFO 2, ENV SYNC, filter TYPE, step LEN/VEL, drum mute, TEXTURE, MIDI (channels, Program Change, LEARN, clock), the 27 performance effects and REV/ECHO THROW.

## Use it in your own work

You are welcome to use these features, screens and ideas in your own firmware or project, **as long as you give proper credit**: a line in your README **or** in your presentation video, for example:

> Features from winampFM Synth by Hericdk (github.com/hericdk/winampFM)

The code is GPL-3.0-only, so any firmware you distribute that is based on it must also ship its source under the same licence. The credit above is the author's request, on top of the licence.

## What is in this repository

| Folder | Content |
|---|---|
| `site/` | The site: installer, 3D simulator (`site/sim/`) and the feature clips. |
| `release/` | The firmware package `winampfm-0.1.fwsc`. |
| `firmware/` | The complete source of that firmware (C, JieLi AC79), as the GPL requires. `sh firmware/build.sh` builds the package (see `firmware/BUILDING.md`). |

**Rescue:** hold OCT− alone while switching the FM-1 on to enter the USB rescue mode and reinstall any firmware. Installing custom firmware is at your own risk.

## Credits

- **winampFM Synth:** Hericdk ([Instagram](https://instagram.com/hericdk) · [Reddit](https://reddit.com/u/hericdk))
- **FM1 Quest** by Hericdk, where the winampFM screens started
- **SLOOP** by [isod89](https://github.com/isod89/sloop-fm1), the firmware winampFM is built on
- **Felucca** by Leo Kuroshita ([@kurogedelic](https://github.com/kurogedelic)), Hügelton Instruments, the base of SLOOP
- Font: Pixelify Sans (SIL OFL). Sampled sounds: CC0 recordings (asset terms in `firmware/LICENSING.md`).

Not affiliated with Winamp or M-VAVE. Winamp, M-VAVE and FM-1 are trademarks of their respective owners.
