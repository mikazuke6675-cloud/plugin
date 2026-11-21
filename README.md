# Afterhours — late-night hip-hop synth (VST3)

A polyphonic synth for dark, spacey modern hip-hop: muffled "underwater" keys, wide detuned pads,
gliding psychedelic leads and sliding 808s.

## Build & install on Windows (one time, ~10 minutes)

1. Install **Visual Studio 2022 Community** (free) and tick the **"Desktop development with C++"** workload.
2. Unzip this folder somewhere simple, e.g. `C:\Afterhours`.
3. Right-click **`build.bat`** → **Run as administrator**.
   It compiles the plugin and copies `Afterhours.vst3` into `C:\Program Files\Common Files\VST3`.
4. Rescan plugins in your DAW (FL Studio: *Options → Manage plugins → Find plugins*) and load **Afterhours** as an instrument.

A standalone app is also built: `build\Afterhours_artefacts\Release\Standalone\Afterhours.exe`.

**No Visual Studio?** Push this folder to a GitHub repo. The included workflow (`.github/workflows/build.yml`)
builds the Windows and macOS VST3s automatically; download them from the run's *Artifacts* section.

## Presets

| Preset | What it is | Play it |
|---|---|---|
| Underwater Keys | Muffled triangle/sine keys, chorus, tape wobble | Slow minor-7th / 9th chords |
| Toronto Night Pad | 5-voice detuned saw pad, slow filter swell, big reverb | Hold chords, let it breathe |
| Cactus Space Lead | Mono gliding lead with vibrato, dotted-1/8 delay | Overlap notes to slide |
| Glide 808 | Driven sine 808 with a knock on the attack, 12-semitone bend | Overlap notes to slide; play C1–C2 |
| Haze Pluck | Short filtered pluck with 1/4 delay | Arpeggios, 1/8 or 1/16 patterns |
| Wobble Bell | Sine bell with heavy tape warble | Sparse high melodies |
| Dark Choir Pad | 7-voice breathy pad with a slow filter LFO | Long held chords |
| Late Night Sub | Clean round sub bass | Under the 808 or instead of it |

## Signature controls

- **MUFFLE** (Master): the "through-the-wall" lowpass that gives that filtered, late-night sound. Automate it for drops.
- **TAPE WARBLE** (Motion): slow random pitch drift, like a worn cassette.
- **MONO + LEGATO + GLIDE** (Voice): when you overlap notes in mono mode, the pitch slides instead of retriggering. That's how 808 slides work.
- **WIDTH**: stereo spread of the unison voices.
- Mod wheel adds vibrato. Sustain pedal is supported. The delay syncs to your DAW tempo.

## Under the hood

Up to 16 voices. Each has PolyBLEP oscillators (up to 7-voice unison on osc A), plus osc B, sub and noise,
pre-filter drive, a 12/24 dB zero-delay-feedback lowpass, and two ADSRs.
Global FX chain: chorus → tempo-synced ping-pong delay → reverb → muffle → soft clip.

Built with JUCE 8 (bundled in `JUCE/`; free for personal use under its AGPLv3/commercial licence).
