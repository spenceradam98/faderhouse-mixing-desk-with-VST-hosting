# Faderhouse design notes

## Goal

A large-format-console-style mixer modelled on the behaviour of classic British
consoles (channel strip with filters, dynamics and a four-band EQ, aux sends,
a centre-section bus compressor), that works:

- **as a plugin** (VST3, AU on macOS) on a DAW track or bus, and
- **as a Standalone app** where each channel takes an audio-interface input or
  hosts a VST3 instrument, and every channel can host VST3 effects, so the
  desk can be used as a live mixer / plugin host without a DAW.

No trademarked names or artwork: the product is called Faderhouse (placeholder,
easy to change in `CMakeLists.txt`), with "brown" and "black" EQ modes rather
than any manufacturer's model names.

## Signal flow

```
source ─► trim/drive/Ø ─► [inserts if INS PRE] ─► HPF/LPF ─► dynamics ⇄ EQ ─► [inserts] ─► pan ─► fader ─► mute/solo ─► mix bus
                                                                                         └► sends A-D (pre or post fader)
sends A-D ─► FX return chain ─► return level/balance/mute ─► mix bus
mix bus ─► master inserts ─► bus compressor ─► master fader ─► output
```

- **Sources**: Off, mono input 1-16, stereo pair 1/2 ... 15/16, or the
  channel's hosted instrument (with a per-channel MIDI channel filter, so a
  multitimbral setup or a single keyboard with split channels works).
- **Pan**: constant-power (-3 dB centre) for mono sources, balance (unity
  centre) for stereo sources and FX returns.
- **Solo** is solo-in-place; FX returns are solo-safe. Mute and solo also cut
  the channel's sends.
- **Gain changes** (faders, pan, sends, returns, master) are ramped per block,
  so automation and fader moves don't click.

## DSP

| Block | Implementation |
|---|---|
| Input | Trim ±20 dB, phase invert, drive: biased tanh soft clip blended with dry, unity small-signal gain |
| Filters | HPF 16-350 Hz, 18 dB/oct (2nd-order + 1st-order); LPF 3-22 kHz, 12 dB/oct |
| EQ | RBJ biquads. HF 1.5-16 kHz and LF 30-450 Hz shelves (bell option), HMF 0.6-7 kHz and LMF 0.2-2.5 kHz bells, ±15 dB. Brown = constant Q; Black = proportional Q (narrower as gain rises) and tighter shelves |
| Compressor | Feed-forward, stereo linked, 6 dB soft knee, ratio 1-inf, 1 ms (fast) or 10 ms attack, 0.05-4 s release, partial auto make-up |
| Gate/expander | Threshold, range 0-80 dB, 20 ms hold, 0.2 ms attack, release smoothed in dB; expander mode is 1:2 down to the range |
| Bus compressor | Stereo linked VCA style, ratio 2/4/10, attack 0.1-30 ms, release 0.1-1.2 s or Auto (fast for peaks, slow for sustained reduction), make-up, parallel mix |

The EQ design code (`Biquad.h`) is shared by the audio path and the EQ curve
display, so the curve is exactly what you hear.

Portability: plain scalar C++ plus JUCE; no AVX/AVX2. macOS builds are
universal (x86_64 + arm64) with a 10.15 minimum, so Big Sur Intel Macs run it.

## Plugin hosting

`HostedChain` wraps a JUCE `AudioProcessorGraph` per chain: 16 instrument
slots, 16 insert chains, 4 FX return chains and the master inserts (37
graphs, all idle when empty). Each chain is stereo, wires MIDI to plugins that
accept it, and fans mono plugins out to stereo. Up to 6 effects per chain.

The installed-plugin list is shared by the whole console and saved to
`<user app data>/YourBrand/Faderhouse/KnownPlugins.xml`. Hosted plugins and
their state are saved in the session; restores that arrive on a non-message
thread are applied by a timer on the message thread. Editors close themselves
before their plugin is removed.

## Plugin vs Standalone buses

- **Standalone**: one input bus of up to 16 discrete channels (whatever the
  interface offers), stereo output. Channel *n* defaults to input *n*.
- **Plugin**: stereo main input plus 7 optional stereo inputs ("In 3/4" ...
  "In 15/16") that DAWs show as sidechain-style inputs. Channel 1 defaults to
  the main input, channels 2-8 to the extra inputs.

## Known limits (v0.1)

- No plugin delay compensation between channels: a high-latency insert on
  one channel will be out of time with the others. The master inserts are
  fine (everything goes through them).
- Plugin scanning runs in-process (as in Scope), so a crashing plugin can
  take the app down during a scan; the dead-man's-pedal file skips it next time.
- No groups/VCAs, no automation of insert slots, no meters on the input side.
- Stereo output only.

## Roadmap

1. Per-channel latency compensation for hosted plugins.
2. Out-of-process plugin scanning.
3. Audio-group buses (4 stereo groups) and VCA faders.
4. MIDI learn for faders/knobs (control surfaces), and a Standalone "snapshot" recall.
5. Input metering and a talkback/oscillator in the centre section.
6. Installers (reuse Scope's GitHub Actions + pkg/Inno Setup scripts).
