# Cardputer ADV Frequency + Noise Generator

A tone and noise generator for the M5Stack Cardputer ADV: pretuned frequencies, a custom frequency
generator, and seven noise colors. The built-in help is always on the device: press **h**.

## Controls

### All modes
| Key | Action |
|---|---|
| `Enter` | Play / stop |
| `Tab` | Cycle modes: Presets, Custom, Noise |
| `-` / `=` | Volume down / up (0-10) |
| `o` | Output: `spk` (speaker) or `hp` (headphones, lower level cap) |
| `w` | Toggle sine / square wave (tones only) |
| `t` | Cycle auto-stop timer: off, 5, 10, 15, 30, 60 min |
| `r` | Rotate the screen 180 degrees |
| `h` | Help (use `;` and `.` to change page, any other key closes) |

### Presets and Noise modes
| Key | Action |
|---|---|
| `;` (up arrow) | Previous item |
| `.` (down arrow) | Next item |

Changing the selection while a sound is playing switches it instantly.

### Custom mode
| Key | Action |
|---|---|
| `0`-`9`, `.` | Type a frequency in Hz (0.1 - 20000) |
| `Del` | Backspace |
| `;` | Frequency + step |
| `,` | Frequency - step |
| `/` | Cycle step size: 0.1 / 1 / 10 / 100 / 1000 Hz |

The tone updates live while it is playing.

## Frequency presets

| Frequency | Name | Description |
|---|---|---|
| 174 Hz | Solfeggio 174 | Traditionally linked to relaxation and easing tension |
| 285 Hz | Solfeggio 285 | Traditionally linked to rest, renewal and recovery |
| 396 Hz | Solfeggio 396 | Traditionally linked to releasing fear and guilt |
| 417 Hz | Solfeggio 417 | Traditionally linked to change and letting go |
| 528 Hz | Solfeggio 528 | The "love" tone, popular for calm and positivity |
| 639 Hz | Solfeggio 639 | Traditionally linked to connection and harmony |
| 741 Hz | Solfeggio 741 | Traditionally linked to clarity and expression |
| 852 Hz | Solfeggio 852 | Traditionally linked to intuition and focus |
| 963 Hz | Solfeggio 963 | Traditionally linked to awareness and stillness |
| 432 Hz | Concert A 432 | Alternate tuning of A; many find it warmer |
| 440 Hz | Concert A 440 | Standard concert pitch A4, a handy tuning reference |
| 136.1 Hz | OM tone | Tied to Earth's year; used in meditation and chanting |
| 111 Hz | 111 Hz | Popular meditation tone, said to aid relaxation |
| 7.83 Hz | Schumann 7.83 | Earth's resonance; too low for the built-in speaker |
| 40 Hz | Gamma 40 | Gamma brainwave range (focus); too low for the speaker |
| 10 Hz | Alpha 10 | Alpha range (relaxed wakefulness); too low for the speaker |
| 6 Hz | Theta 6 | Theta range (drowsy, meditative); too low for the speaker |
| 2 Hz | Delta 2 | Delta range (deep sleep); too low for the speaker |

## Noise colors

| Noise | Shape | Description |
|---|---|---|
| White | flat | Equal energy at all pitches; bright hiss that masks sound |
| Pink | -3 dB/octave | Softer than white; like steady rain; good for focus and sleep |
| Brown | -6 dB/octave | Deep rumble; like thunder or a waterfall; calming |
| Green | around 500 Hz | Mid-range; natural, like a forest or stream |
| Blue | +3 dB/octave | Bright and hissy; stronger highs than pink; can be sharp |
| Violet | +6 dB/octave | Very bright, mostly high pitches; can be harsh |
| Grey | even loudness | Approximation tuned to human hearing: lows and highs boosted, mids softer |

## Audio quality and headphones

Audio is generated in software at 16-bit / 48 kHz on its own task, with fixed headroom, smooth
volume changes (no clicks) and a soft limiter on noise. This removes the crackling that came from
running the output at full scale.

- Using headphones? Press `o` to switch to `hp` mode, which caps the level well below the speaker
  setting. Headphones are far more sensitive than the speaker.
- Still hearing distortion? Lower `HW_VOLUME`, `MAX_GAIN_SPK` or `MAX_GAIN_HP` at the top of
  `src/main.cpp` and rebuild.
- The built-in speaker is clearest between roughly 200 Hz and 8 kHz. Below about 100 Hz it is very
  faint or silent; use headphones or an external amplifier for the low presets.
- Sine is smoother; square is louder.
- If the screen is upside down, press `r`.

## Notes

These sounds are popular for relaxation and meditation, but there is no good scientific evidence that
specific frequencies heal anything. Treat this as a sound and relaxation tool, and keep the volume
comfortable, especially with headphones.

## Building

Push this repository to GitHub. The workflow in `.github/workflows/build.yml` builds the firmware and
publishes a single flashable `cardputer_freq_gen.bin` under **Actions, latest run, Artifacts**.
Flash it at address `0x0` with the browser flasher (hold G0 while plugging in the Cardputer).

To build locally instead: install PlatformIO and run `pio run`.
