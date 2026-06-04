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

`0.19 RMS` (≈ −14.4 dBFS / −14 LUF) is the house reference — settled by ear with
the user (2026-06-04). Higher targets (0.30 / 0.38 / +6 dB) made the soft-clip
graze hard-playing transients and audibly distort; −14 LUF keeps the loudest
plucks under the soft-clip threshold. Don't change it per-amp; only `kLvl`.

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

## Current kLvl values (target 0.19 ≈ −14 LUF, 2026-06-04)

| amp | kLvl | | amp | kLvl |
|---|---|---|---|---|
| DSL100 | 0.638 | | MarkIII | 0.415 |
| DualRect | 0.751 | | MarkIV | 0.429 |
| EN30 / Box DC30 | 0.483 | | FK800 (GK) | 0.855 |
| TW22 / SuperNova | 0.476 | | Sharke HB3500 | 0.179 |
| TW26 / Deluxe | 0.522 | | Sharke HB5000 | 0.249 |
| TW40 | 1.067 | | Sampleg SVT | 0.365 |

All matched to the same multitone loudness; EQ-max peaks bounded ≤ 0.99 by
rbAmpLvl. To re-scale the whole set by ear: multiply every kLvl by 10^(dB/20).

## Gotchas

- **No hot reload + orphan backend**: closing Slopsmith doesn't kill the uvicorn
  backend; the next open serves STALE code. After rebuilding, `Cmd+Q` →
  `pkill -f "uvicorn server:app"` → reopen. (See [[project_slopsmith_plugins]].)
- After editing source, rebuild → copy binary into `vst/amps/<Bundle>.vst3/Contents/MacOS/<BIN>`
  → `codesign --force --sign -`. Renamed bundles (Box DC30 etc.) keep the build
  NAME as the inner binary (EN30) but a parody folder name.
- The amps the colleague restructured (EN30/TW22/TW26 → Core.h) keep their
  internal makeup; `rbAmpLvl(kLvl·…)` just wraps it — that's fine.
