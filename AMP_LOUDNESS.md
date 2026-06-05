# Amp loudness + EQ-headroom standard

Goal: **every amp VST sounds at the same volume**, and **boosting tone/EQ never
hard-clips**. This doc is the recipe so each new amp matches the rest.

## The one rule

Every amp's per-channel output ends with the **same final stage**:

```cpp
// transparent below ±0.90, soft-saturates to a ±0.99 ceiling (no hard clip)
static inline float rbAmpLvl(float x){ const float t=0.90f,c=0.99f,a=(x<0.f?-x:x);
    if(a<=t) return x; return (x<0.f?-1.f:1.f)*(t+(c-t)*std::tanh((a-t)/(c-t))); }

// in run(): wrap the channel output
oL[i] = rbAmpLvl(kLvl * core.process(iL[i]));
```

Two parts:

1. **`kLvl`** — a per-amp constant that makes the amp hit the common loudness
   **target ≈ 0.19 RMS (≈ −14 LUF)** (a 110 Hz–1.8 kHz multitone, measured at the
   amp's real Rocksmith song settings). That's what equalizes volume across amps.
2. **`rbAmpLvl`** — a soft ceiling: it does nothing below ±0.90 (so the matched
   tone is untouched), then rounds off to ±0.99 above. So an EQ/tone boost
   soft-saturates near full scale instead of hard-clipping the engine.

**Target = −14.00 dBFS RMS** multitone (updated 2026-06-05, was −15). Higher
targets (0.30 / 0.38 / +6 dB) made the soft-clip graze hard-playing transients
and audibly distort; −14 keeps the loudest plucks under the soft-clip threshold.
Don't change the target per-amp; only `kLvl`.

> **Two measurement conventions exist — pick one and be consistent:**
> 1. **Real-RS-settings** (this doc's original method): pull the knob values the
>    game sends from `preset_pieces.vst_state`, median across songs. Honest for
>    "how loud will it actually play," but per-amp and harder to reproduce.
> 2. **kDef preset** (used for the 2026-06 bass-amp batch + re-norm): measure at
>    the amp's default param array `kXxxDef` (its voiced "noon"). Fully
>    reproducible to 0.01 dB, amp-agnostic script. All 18 bass amps now sit at
>    **−14.00 dBFS RMS at kDef**. Songs override the knobs via the RS mapping
>    anyway, so the reference point is somewhat arbitrary — consistency matters
>    more than which point. The full build pipeline uses convention 2; see
>    **AMP_BUILD_GUIDE.md**.

> Note: if a single amp distorts way more than the rest, first check the
> **gain the song actually sends** (RS knob → param mapping) — an amp at
> gain 100 when the song asked for 40 will distort regardless of loudness.
> That's a mapping/settings issue, not a `kLvl` issue.

## Tuning `kLvl` for a NEW amp (5 steps)

1. Build the amp with `kLvl = 1.0f` first.
2. Measure its **multitone RMS at the real RS settings** (the knob values the
   game actually sends — pull them from `nam_tone.db` `preset_pieces.vst_state`,
   take the median across songs that use the gear). Harness below.
3. `kLvl = 0.19 / measured_RMS`.
4. Rebuild, re-measure → should read ~0.19. Re-measure the **EQ-max peak** (all
   tone/EQ knobs at 1.0): must be **≤ ~0.99** (rbAmpLvl guarantees it).
5. Verify against another amp by ear — they should feel equal.

> Measure at REAL settings, not "noon": bass amps sit at low gain / high Ultra-Lo,
> guitar amps at high gain — noon lies. White noise / a single sine also lie
> (they ignore the amp's voicing); a **multitone 110 Hz–1.8 kHz** is the honest
> proxy. (This is why earlier white-noise passes drifted ~10–15 dB.)

## Measurement harness (offline, no host)

Include the amp's `*Plugin.cpp`, derive a `Probe` to reach protected
`run()`/`setParameterValue()`/`initParameter()`/`sampleRateChanged()`, feed a
multitone at input RMS 0.1, read output RMS of the settled half. Compile with
`/usr/bin/clang++ -isysroot $(xcrun --show-sdk-path) -std=c++14 -I. -I.. \
-I../../DPF/distrho -I../../DPF/dgl harness.cpp ../../DPF/distrho/src/DistrhoPlugin.cpp`.
Call `sampleRateChanged(48000)` (host normally sets SR; offline it's 0). Set the
gain-equivalent param by NAME — it's "Gain" for most, **"Solid State"** for
Hartke (Sharke), **"Volume"** for the GK (FK800). See [[reference_build_bundled_vsts]].

## Re-normalizing a whole set fast (used 2026-06-05 to move 17 bass amps to −14)

The output level is **one coefficient** per amp in `run()` — a named `kXxxLvl`
const, or an inline literal before `process`. To shift an amp by Δ dB, multiply
that coefficient by **`10^(Δ/20)`** where `Δ = (−14) − measured`. EXACT for the
softclip-bounded forms (`kLvl·softClip(…)·0.98` — output stays below the 0.90
`rbAmpLvl` knee, so the wrap is identity) and held perfectly even for the bare
`coeff·process` forms (SharkeHB3500: −28.84, no makeup stage → ×5.52 → −14.00;
MarstenDBS7400: −9.28 → ×0.58 → −14.00). One pass, no iteration. **Changing this
coefficient does not touch the param layout → saved `preset_pieces.vst_state`
stays valid, no DB recompute.**

## Bass-amp output-coeff values (target −14.00 dBFS @ kDef, 2026-06-05)

| amp | coeff | | amp | coeff |
|---|---|---|---|---|
| TracerV8 (kTracerLvl) | 0.2775 | | Lovolt100 (kLovoltLvl) | 0.3106 |
| SbrRedhead (kRedheadLvl) | 0.2718 | | SillaBoogieBass400 (kSillaLvl) | 0.2973 |
| DustupCDN (kDustupLvl) | 0.2563 | | CitrusAD200 (kCitrusLvl) | 0.2705 |
| ElectricB600F (kElectricLvl) | 0.249 | | SamplegSBTCL (inline·softClip) | 0.3583 |
| BenderFumble800 (kRumbleLvl) | 0.3113 | | SamplegV4B (inline·softClip) | 0.2529 |
| AidenGT300/550/880 (kEdenLvl) | 0.3106 | | PeeBeeTMinus (inline·softClip) | 0.3007 |
| FreddyKrueger800BR (inline·process) | 0.9183 | | SharkeHB5000 (inline·hbSoftClip) | 0.2830 |
| SharkeHB3500 (inline·process) | 0.9882 | | MarstenDBS7400 (inline·process) | 1.1906 |

Guitar amps (DSL100 0.638, DualRect 0.751, EN30 0.483, TW22 0.476, TW26 0.522,
TW40 1.067, MarkIII 0.415, MarkIV 0.429, FK800 was 0.855) still at the older
−15-era pass — re-run the batch if they need −14 too. EQ-max peaks ≤ 0.99 by
rbAmpLvl.

## Gotchas

- **No hot reload + orphan backend**: closing Slopsmith doesn't kill the uvicorn
  backend; the next open serves STALE code. After rebuilding, `Cmd+Q` →
  `pkill -f "uvicorn server:app"` → reopen. (See [[project_slopsmith_plugins]].)
- After editing source, rebuild → copy binary into `vst/amps/<Bundle>.vst3/Contents/MacOS/<BIN>`
  → `codesign --force --sign -`. Renamed bundles (Box DC30 etc.) keep the build
  NAME as the inner binary (EN30) but a parody folder name.
- The amps the colleague restructured (EN30/TW22/TW26 → Core.h) keep their
  internal makeup; `rbAmpLvl(kLvl·…)` just wraps it — that's fine.
