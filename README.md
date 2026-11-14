# Cardputer ADV Frequency Generator

A tone generator for the M5Stack Cardputer ADV with pretuned frequencies and a custom generator.
The built-in help is always available on the device: press **h**.

## Controls

### All modes
| Key | Action |
|---|---|
| `Enter` | Play / stop |
| `Tab` | Switch between Presets and Custom |
| `-` / `=` | Volume down / up |
| `w` | Toggle sine / square wave |
| `t` | Cycle auto-stop timer: off, 5, 10, 15, 30, 60 min |
| `r` | Rotate the screen 180 degrees |
| `h` | Help (use `;` and `.` to change page, any other key closes) |

### Presets mode
| Key | Action |
|---|---|
| `;` (up arrow) | Previous preset |
| `.` (down arrow) | Next preset |

Changing preset while a tone is playing switches the tone instantly.

### Custom mode
| Key | Action |
|---|---|
| `0`-`9`, `.` | Type a frequency in Hz (0.1 - 20000) |
| `Del` | Backspace |
| `;` | Frequency + step |
| `,` | Frequency - step |
| `/` | Cycle step size: 0.1 / 1 / 10 / 100 / 1000 Hz |

The tone updates live while it is playing.

## Presets

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

## Notes

- The built-in speaker is small. It is clearest between roughly 200 Hz and 8 kHz. Below about 100 Hz it is very faint or silent; use an external amplifier or headphones for the low presets.
- Sine is smoother; square is louder.
- These frequencies are popular for relaxation and meditation, but there is no good scientific evidence that they heal anything. Treat this as a sound and relaxation tool.
- If the screen is upside down, press `r`.

## Building

Push this repository to GitHub. The workflow in `.github/workflows/build.yml` builds the firmware and
publishes a single flashable `cardputer_freq_gen.bin` under **Actions → latest run → Artifacts**.
Flash it at address `0x0` with the browser flasher (hold G0 while plugging in the Cardputer).

To build locally instead: install PlatformIO and run `pio run`.
