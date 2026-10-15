# Monster Lab

Creature, sci-fi and vintage-media voice processor for audio drama. Native Apple Silicon Mac app (JUCE/C++).

Load a voice recording, click presets to stack effect layers (like Photoshop layers), tweak, A/B against the original, export a 24-bit WAV for Audition. The original file is never modified.

## Getting the app (no programming needed)

Every time code is pushed to this GitHub repository, GitHub builds the Mac app automatically on one of its Apple Silicon Macs.

1. Open the repository on github.com and click the **Actions** tab.
2. Click the newest run at the top with a green tick (a yellow dot means it's still building; it takes about 10 minutes).
3. Scroll to **Artifacts** at the bottom and click **Monster-Lab-mac**. A zip downloads.
4. Double-click the zip in Downloads (twice if a second zip appears) until you see **Monster Lab**.
5. Drag **Monster Lab** into your **Applications** folder.

### First launch (one time only)

The app isn't signed with a paid Apple developer certificate, so macOS blocks it the first time.

1. Double-click Monster Lab. macOS says it can't be opened. Click **Done** (or **OK**).
2. Open **System Settings → Privacy & Security**, scroll to the bottom, and click **Open Anyway** next to "Monster Lab was blocked".
3. Confirm with your password or Touch ID, then click **Open**.

After that it opens with a normal double-click.

## Using it

- **Right panel:** drag a WAV (or AIFF/FLAC) onto the waveform, or click OPEN AUDIO.
- **Left panel:** click a preset. Each click *adds* a new layer on top; nothing is replaced.
- **Center top (the stack):** layers process top to bottom. Each row has an on/off light, a wet/dry blend, move up/down, duplicate, save as preset, and delete. Double-click a name to rename it.
- **Center bottom:** every effect of the selected layer. Each effect has its own on/off light and each section (Voice, Texture, Sci-Fi, Space, Media) has a master switch. Moving any control switches its effect on. Each section's **RESET** button puts that section back to how the layer started, so experiment freely. Double-click a slider to reset it; click the number to type a value.
- **A / B:** switch between the original and the processed result while playing. Space bar plays/pauses.
- **EXPORT 24-BIT WAV** writes a new file next to the original.
- **SAVE / LOAD CHARACTER** stores the whole stack as a `.monster` file (`Documents/Monster Lab/Characters`). Saved layer presets live in `Documents/Monster Lab/Layer Presets` and appear under MY LAYER PRESETS.

The app renders the whole recording after each change (usually under a second or two on an M-series Mac; heavy stacks like Ghost or Eldritch take longer). The status light is amber while rendering and teal when ready. Layers above the one you change are reused instead of re-rendered, so tweaking the last layer in the stack is fastest.

**Reverse reverb lead-in:** when a layer uses Reverse Reverb, the export starts *before* the original so the swell isn't cut off. The status line tells you exactly how much earlier (for example 2.35 s), so you can line it up in Audition.

## What each effect actually does

Nothing here is a placeholder. Where an effect is a simpler model than a commercial plugin, it says so.

| Effect | How it's implemented |
|---|---|
| Pitch | Rubber Band Library R3 engine, formant-preserving pitch shift, duration unchanged |
| Formant | Rubber Band R3 formant scaling, independent of pitch |
| Creature Size | Combined pitch + formant shift in the same Rubber Band pass, plus a low (bigger) or high (smaller) body resonance |
| Bass | Low-shelf EQ |
| Multiple Voices | 1–6 extra Rubber Band pitch-shifted copies, detuned, delayed and panned |
| Grit | Level-tracking saturation of the voice's mid band plus slow, smooth throat roughness. No added noise (v0.3 fix for the crinkly 'plastic bag' sound) |
| Growl | Jittered amplitude pulses at 15–90 Hz (vocal-fry style) + saturated low band. A modelled growl, not a sampled one |
| Breath / Whisper | True STFT whisperisation (keeps the voice's spectrum, randomises phase) |
| Distortion | Asymmetric tanh waveshaper + tone filter |
| Instability | Random pitch drift (modulated delay) + amplitude flicker |
| Subharmonics | Octave-down (or two octaves) Rubber Band copy, low-passed and blended |
| Robot | STFT robotisation, monotone at a set pitch |
| Ring Mod | Sine ring modulator |
| Freq Shift | Single-sideband frequency shifter (IIR Hilbert transform) |
| Bitcrush | Bit-depth reduction + sample-and-hold rate reduction |
| Flanger / Chorus | Modulated delays (flanger with feedback, 3-voice chorus) |
| Spectral Freeze | STFT spectral hold with adjustable decay; at 100% it holds forever |
| Shimmer Reverb | Reverb → Rubber Band pitch shift (any interval, ±24 st) → second reverb |
| Reverse Reverb | Automatic reverse → reverb → reverse, with lead-in so swells come *before* the words |
| Reverb | 8-line feedback-delay-network reverb with diffusion and tone control |
| Echo / Delay | Tape-style darkening echo; clean stereo ping-pong delay |
| Stereo Movement | Auto-pan |
| Playback Speed | Varispeed resample (changes pitch and length, like tape) |
| Wow / Flutter | Slow and fast modulated delay with random drift |
| Tape Saturation | Biased soft clipping, head bump, high-frequency loss |
| Resonance | Telephone, Megaphone, AM Radio, Walkie-Talkie, Gramophone EQ + saturation models |
| Bandwidth | 24 dB/oct high-pass and low-pass |
| Compression | Feed-forward peak compressor |
| Dropouts | Random gain dips |
| Hiss / Crackle | Shaped noise; random clicks + surface rumble |

Every layer automatically matches its loudness to the signal coming into it, so stacking doesn't explode. The final output is turned down if it would clip.

## Known limitations (v0.2)

- Changes are rendered offline (whole file), not live while dragging. Fine for lines and scenes; slow for very long files (limit 20 minutes).
- The waveform region-selection idea isn't built yet.
- No undo yet.
- ChowTapeModel was not used: it's a full plugin with heavy dependencies. The tape effects here are simpler models.
- Not yet a VST3 plugin. The engine is separate from the interface so a plugin version can reuse it, but real-time plugin processing needs its own work (Rubber Band has a real-time mode for that).

## Licence

Monster Lab includes the **Rubber Band Library** (GPL v2 or later) and **JUCE** (AGPLv3 / commercial). That is fine for your own use. If you ever sell or distribute Monster Lab, you'd need to release its source under the GPL/AGPL or buy commercial licences for Rubber Band (breakfastquay.com) and JUCE.

## For developers

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMONSTERLAB_BUILD_TESTS=ON
cmake --build build -j
./build/MonsterLabRenderTest_artefacts/Release/MonsterLabRenderTest voice.wav out
```

- `Source/Engine/Effects.h`: the effect table (names, ranges, defaults, units)
- `Source/Engine/Model.cpp`: layers, presets, save/load
- `Source/Engine/Renderer.cpp`: all DSP
- `Source/UI/MainComponent.cpp`: interface
