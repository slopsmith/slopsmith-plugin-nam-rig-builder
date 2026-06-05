# Building an amp VST3 — end-to-end guide

How to add a **component-level amp model** as a bundled VST3 to Rig Builder, the
way the 18 bass amps + the guitar amps were built. Read this top-to-bottom once;
after that it's a checklist. Companion docs: **AMP_LOUDNESS.md** (the −14 dBFS
loudness standard), `CLAUDE.md` (environment golden rules), `HANDOFF.md` (DB
schema + the bigger picture).

> Everything here is **plugin-only** (no Slopsmith bundle edits). Amps live on
> the `feat/amps-vst` branch. **No hot reload** — the user must quit + reopen
> Slopsmith (and kill the orphan backend, see §11) to load a rebuilt VST.

---

## 0. Philosophy

Model the **real circuit, stage by stage**, not a black-box EQ curve. Work from
a **schematic + a panel photo** (the photo decides knob layout/labels and often
which amp it really is — a name-matching schematic has burned us, see the Rumble
note in [[project-bass-amps-vst]]). Each tube/op-amp/tone-stack/power stage maps
to a building block below. Stay faithful in the UI too: same knobs, same names,
same order — but **parody branding only** (no real make/model/logo on the face;
the panel may carry a fake model code, e.g. "B600F").

---

## 1. Anatomy of an amp directory

`vst/src/amps/<codename>/` holds exactly five files:

| File | What it is |
|---|---|
| `XxxParams.h` | param enum + names + symbols + min/max/**def** arrays + EQ freq table |
| `XxxPlugin.cpp` | the DSP: building blocks + a per-channel class + the `Plugin` boilerplate |
| `Xxx_ui.cpp` | the native DPF UI (NanoVG). **Not** what the app shows — see §9 |
| `Makefile` | `NAME = <BundleName>`, `FILES_DSP`, `FILES_UI`, `TARGETS += vst3` |
| `DistrhoPluginInfo.h` | plugin metadata (name, unique id, I/O counts) |

The dir name is the **RS codename-ish** stem (e.g. `ht400b_silla`); the bundle
`NAME` is the **parody display name** (`SillaBoogieBass400`). The installed
artifact is `vst/amps/<NAME>.vst3`.

---

## 2. Shared DSP building blocks

All in the per-amp `Plugin.cpp` (copy-paste between amps; several are
byte-identical across the set — keep them so). The honest ones:

- **`rbAmpLvl(x)`** — the mandatory final soft-ceiling (transparent below ±0.90,
  rounds to ±0.99). Every channel output ends with it. See AMP_LOUDNESS.md.
- **`softClip(x) = tanh(x)`** — generic saturator (some amps use `hbSoftClip`).
- **`Mna`** — tiny fixed-size Modified Nodal Analysis solver (≤8 nodes, no heap,
  RT-safe): stamp resistors / capacitor companions / ideal op-amps / `gm`
  transconductance, Gaussian-eliminate per sample. The backbone for real
  circuit stages.
- **`Tube` (12AX7/ECC83)** — nodal triode: Koren plate law solved by
  Newton-Raphson each sample = the asymmetric tube grit. The signature preamp
  nonlinearity.
- **`Biquad`** — RBJ cookbook: `setLowShelf` / `setHighShelf` / `setPeak` /
  `setNotch` (band-reject) / `setBypass`. Tone stacks + fixed/param EQ + notch.
- **`MFB`** — multiple-feedback band-pass = one graphic-EQ band (nodal).
- **`pushPull`** — power-tube saturator (the output stage's compression).
- **`OptoComp`** — opto compressor (Trace/Ashdown style).
- **`SubOct`** — zero-cross divider sub-octave generator (Ashdown).

Pick the blocks the real amp actually has. A clean Class-D amp (e.g. Electric
B600F) may be just `op-amp gain → notch → EQ → master`, no tube, no softClip in
the path — only the `rbAmpLvl` ceiling.

---

## 3. `XxxParams.h`

One enum ending in `kParamCount`, then five parallel arrays sized
`[kParamCount]`. Boolean params (switches) go **last**. Pattern (from Silla):

```cpp
enum SillaParamId {
    kVol1 = 0, kVol2, kMiddle, kBass, kTreble, kMaster,   // knobs (continuous)
    kEq40, kEq100, kEq250, kEq625, kEq1560, kEq3900,      // graphic EQ
    kEqIn, kBright1, kBright2, kBassShift, kTrebShift,    // switches (boolean)
    kParamCount
};
static const int   kFirstEq = kEq40;
static const int   kNumEq   = 6;
static const float kEqFreqs[kNumEq] = { 40,100,250,625,1560,3900 };

static const char* const kSillaNames[kParamCount]   = { "Volume 1", ... };  // UI labels
static const char* const kSillaSymbols[kParamCount] = { "vol1", ... };      // automation ids
static const float kSillaMin[kParamCount] = { 0,0,... };
static const float kSillaMax[kParamCount] = { 1,1,... };
static const float kSillaDef[kParamCount] = { 0.50f, 0.30f, ... };          // the voiced "noon"
```

`kXxxDef` is **the loudness reference point** (§7) and the values the editor
opens at. Tone/EQ knobs flat = `0.50`; switches off = `0.00`. Param ranges are
normalized 0..1; the DSP rescales inside `setParams`.

---

## 4. `XxxPlugin.cpp` — the two halves

**(a) Per-channel class** — holds the stages, has `setParams(const float* p)`
(rescale 0..1 → real units, recompute coeffs) and `inline float process(float
x)` (run one sample through the chain). Stereo = two instances `L, R`.

**(b) `Plugin` subclass** — DPF boilerplate: `initParameter` (set hints; add
`kParameterIsBoolean` for `i >= kFirstSwitch`), `get/setParameterValue` (call
`recalc()` on set), `sampleRateChanged` (re-init SR + `recalc`), and:

```cpp
void run(const float** in, float** out, uint32_t frames) override {
    for (uint32_t i=0;i<frames;++i){
        oL[i] = rbAmpLvl(kLvl * softClip(kMakeup * L.process(iL[i])) * 0.98f);
        oR[i] = rbAmpLvl(kLvl * softClip(kMakeup * R.process(iR[i])) * 0.98f);
    }
}
```

### Gain-staging — pick ONE convention, never mix (this has bitten us)

Mixing them double-counts gain and the soft-clip saturates everything:

1. **makeup-inside-process** (V4B / Rumble): a per-Gain `outMakeup` lives *inside*
   `process()`; `run()` is just `rbAmpLvl(kLvl * softClip(process()) * 0.98)` —
   no extra makeup in run().
2. **makeup-in-run** (HB5000 / Eden): `process()` returns a small master-scaled
   signal; `run()` does `rbAmpLvl(kLvl * softClip(kMakeup * process()) * 0.98)`.

Clean amps skip the softClip entirely: `run()` = `rbAmpLvl(coeff * process())`.

---

## 5. Build · install · sign

DPF lives at `vst/src/DPF`. From the amp dir:

```bash
cd vst/src/amps/<codename>
make BUILD_DIR=/tmp/b TARGET_DIR=/tmp/t DPF_PATH=../../DPF vst3   # default /build,/bin are read-only
cp -R /tmp/t/<NAME>.vst3 ../../amps/<NAME>.vst3                   # install into the plugin
codesign --force --sign - ../../amps/<NAME>.vst3                  # ad-hoc; NO --deep
```

(For an in-place binary swap after a tiny DSP change you can replace just
`Contents/MacOS/<NAME>` and re-sign — see AMP_LOUDNESS.md gotchas.)

---

## 6. Offline harness — verify without a host

Compile the `Plugin.cpp` directly, derive a `Probe` to reach the protected
methods, feed a deterministic **multitone** and read output RMS. The exact
metric the −14 standard uses:

- signal = 5 sines **110/220/440/880/1760 Hz**, each amp `10^(-18/20)/√5`
- process at 48 kHz in 128-frame blocks **at the `kXxxDef` preset**
  (call `sampleRateChanged(48000)` then `setParameterValue(i, kXxxDef[i])` for
  all i — constructor defaults alone gave wrong numbers for some amps)
- measure RMS of output samples 4800→48000 → `20·log10(rms)` dBFS

```cpp
#define NDEBUG
#include "DistrhoPlugin.hpp"
#include "XxxPlugin.cpp"
#include <cstdio><cmath><vector>
using namespace DISTRHO;
struct Probe : public XxxPlugin {
  void prun(const float**i,float**o,uint32_t n){run(i,o,n);}
  void pset(uint32_t i,float v){setParameterValue(i,v);}
  void psr(double r){sampleRateChanged(r);} };
int main(){ Probe p; p.psr(48000); for(int i=0;i<kParamCount;++i) p.pset(i,kXxxDef[i]);
  std::vector<float> mt(48000,0.f); const double f[5]={110,220,440,880,1760};
  double amp=std::pow(10.0,-18.0/20.0)/std::sqrt(5.0);
  for(size_t i=0;i<mt.size();++i){ double s=0; for(int j=0;j<5;++j) s+=std::sin(2*M_PI*f[j]*i/48000.0); mt[i]=(float)(amp*s); }
  std::vector<float> oL(48000,0),oR(48000,0);
  for(uint32_t o=0;o<48000;o+=128){uint32_t n=(48000-o<128)?(48000-o):128;const float*ci[2]={mt.data()+o,mt.data()+o};float*co[2]={oL.data()+o,oR.data()+o};p.prun(ci,co,n);}
  double s=0;int c=0;for(int i=4800;i<48000;++i){s+=(double)oL[i]*oL[i];++c;}
  printf("%.2f dBFS\n",20*std::log10(std::sqrt(s/c))); return 0; }
```

```bash
/usr/bin/clang++ -isysroot $(xcrun --show-sdk-path) -std=c++14 -O2 -DNDEBUG \
  -I. -I.. -I../../DPF/distrho -I../../DPF/dgl \
  /tmp/h.cpp ../../DPF/distrho/src/DistrhoPlugin.cpp -o /tmp/h && /tmp/h
```

Also sanity-check: a NaN sweep, monotonic gain vs the Gain knob, and each
tone/EQ knob moves spectrum the right direction. (IDE diagnostics on these files
are offline false-positives — `DistrhoPlugin.hpp not found` etc.)

---

## 7. Loudness — hit −14.00 dBFS

Target = **−14.00 dBFS** multitone at `kXxxDef`. To set or re-norm an amp:
multiply its single output coefficient (the `kXxxLvl` const or the inline
literal before `process`) by **`10^((−14 − measured)/20)`**, rebuild, re-measure
(one pass — the scaling is exact below the rbAmpLvl knee). Full rationale, the
two measurement conventions, and the current per-amp coefficient table live in
**AMP_LOUDNESS.md**. Changing only this coefficient never touches the param
layout, so any saved `vst_state` stays valid (no DB recompute).

---

## 8. Register the amp (3 JSONs in `data/`)

1. **`rs_gear_to_vst.json`** — `"<RS_gear_key>": [{ "name":"<NAME>", "format":"VST3",
   "bundled":"vst/amps/<NAME>.vst3", "notes":"..." }]`. The RS gear key is
   **case-sensitive** and must equal `preset_pieces.rs_gear_type` exactly (the DB
   uses e.g. `Bass_Amp_EdenWT800`, capital T — a mismatch silently no-ops).
2. **`rs_knob_to_vst_param.json`** — `"<RS_gear_key>": { "<vststem>": { "Gain":
   {"param":"Gain","scale":...}, ... } }`. Maps each RS knob → a VST param by
   **name**, via `out = rs_value*scale + offset` (then `invert` optional):
   - **RS knobs are 0–100** in `params_json` (50 = flat), so an EQ band centered
     at 0.5 uses `scale 0.01`, no offset (50→0.5). *Do not* assume 0–10.
   - A subtractive/passive tone stack (Orange-style: full-up = flat) needs the
     mapping that makes RS-flat = param-flat (offset 0.5 with the right scale).
   - Gain that's just a level → `scale 0.01`.
3. **`vst_display_names.json`** — `"<vststem>": "<Display Name>"` (vststem =
   lowercased `.vst3` basename). This is the parody name shown in the catalog.

Validate all three: `deno eval 'JSON.parse(await Deno.readTextFile("data/x.json"))'`.

---

## 9. In-app canvas face (`pedal_canvas.js`)

The app does **not** open the native DPF window — it renders an HTML-canvas face
from a per-gear spec. Without one, Edit shows a generic knob grid and the gear
photo is blank. Add:

```js
P.<vststem> = { w, h,
  knobs:[{id, cx, cy, r, style, cap}],   // id = the VST param ENUM index (0-based)
  switches:[{id, cx, cy, hs, dark}],
  names:[...],                            // label per param, in id order
  draw(d, vals){ /* box(), rr(), textC(), F.<font>, ledDot ... */ } };
```

`id` is the param enum index; `rbBuildCanvasModel` maps logical→real, dropping
the engine's prepended Buffer Size / Sample Rate (so controls aren't off-by-two).
Match the real panel's layout/labels (no real branding). Syntax-check:
`deno eval 'const c=await Deno.readTextFile("pedal_canvas.js"); new Function(c);'`.
Fonts/JS are served via `/asset/...` so a restart is needed. See
[[project_rig_builder_v2_canvas]].

---

## 10. Map it onto the live DB (so existing songs use the VST)

Registering the JSONs is **not enough for already-mapped songs**. Either:

- **Migration (fresh installs / next launch):** the one-shot
  `_migrate_assign_bundled_primary_once()` (routes.py ~618), guarded by a
  sentinel `bundled_primary_assigned_vN` in the `__rig_builder_master_pre__`
  preset's settings_json. Bump `N` so it re-runs; it backs up the DB, flips every
  AUTO piece whose gear has an installed bundled VST, preserves MANUAL picks.
- **Live repair (this machine, now):** backup `nam_tone.db` first, then in
  bundled Python set `routes._plugin_dir/_db_path/_conn`, and for each
  `preset_pieces` row with your `rs_gear_type`: `prim =
  _pick_installed_primary_vst(gear, _build_known_vst_lookup())`, `state =
  _compute_vst_state_for_piece(gear, prim["vst_path"], json.loads(params_json))`,
  `UPDATE preset_pieces SET vst_path, vst_format, vst_state`. (This is what
  applies the §8 knob mapping into stored state.) User restarts to reload.

Bundled Python:
`/Applications/Slopsmith.app/Contents/Resources/python/runtime/bin/python3.12`
with `PYTHONPATH=/Applications/Slopsmith.app/Contents/Resources/slopsmith/lib:.`.

---

## 11. Identifying an anonymized RS amp

RS codenames (CS/HT/LT/BT…) carry no brand. Extract panel art + manifest knobs
from `~/Downloads/psarc/gears.psarc`:
`tools/extract_gear_photos.py … --category amp` (DDS→PNG; needs bundled Python +
`PYTHONPATH=…/slopsmith/lib:tools`), and `read_psarc_entries(p,
["manifests/gears/gear_bass_amp_<code>.json"])` for `Attributes.Knobs`/`Name`/
`AssociatedCabKey`. The panel `Name` is itself a fake code, so the **user
confirms the real brand** from the rendered look, then you pick the parody name.
See [[reference_rs_amp_identification]].

---

## 12. Ship it — checklist

- [ ] schematic + panel photo in hand; brand confirmed; parody name chosen
- [ ] 5 source files; building blocks match the real circuit; one gain convention
- [ ] harness: no NaN, monotonic gain, tone directions correct
- [ ] loudness = **−14.00 dBFS** at `kDef`
- [ ] built, installed to `vst/amps/<NAME>.vst3`, `codesign --force --sign -`
- [ ] registered in the 3 `data/*.json` (case-sensitive gear key; 0–100 knobs)
- [ ] canvas face `P.<vststem>` added + deno syntax-check
- [ ] live DB mapped (backup first) or migration sentinel bumped
- [ ] **commit + push per amp** on `feat/amps-vst` so a colleague doesn't redo it
      ([[feedback_commit_per_pedal]]); exclude unrelated WIP (routes.py DI/cab,
      screen.js/html) from the commit
- [ ] tell the user to **quit + reopen Slopsmith** (and kill the orphan uvicorn
      backend — `pkill -f "uvicorn server:app"` — or it serves stale code)
