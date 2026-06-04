# JC90 — "Ronald JC-90"

Bundled RigBuilder amp for Rocksmith's `Amp_CS90`. Models the **Roland JC-90
"Jazz Chorus"** — the full front panel, 1:1. Parody brand **"Ronald"**; the
in-app face must never read "Roland".

Local reference (modelled component-by-component):

- `amps/Roland JC-90 (CS-90)/JC-90.pdf` (S. Nagata)

## Panel (10 controls)

A **solid-state** amp (M5218 op-amps + transistor power amp — no tubes): a
clean, high-headroom preamp with a diode-clipping distortion, a passive tone
stack, a spring reverb, and the signature analogue BBD **stereo chorus**.

- **Distortion** (diode-clip drive — clean at 0)
- **Volume**
- **Hi-Treble**, **Treble**, **Middle**, **Bass** (the EQUALIZER section)
- **Reverb** (spring)
- **Rate**, **Depth** + **Chorus** 3-way (Vibrato / Off / Chorus)

The chorus opens the stereo image (dry on one side, the pitch-modulated wet on
the other — the famous wide Jazz Chorus shimmer).

## Rocksmith mapping

`Amp_CS90` exposes Gain/Bass/Mid/Treble/Pres. **Gain → Distortion** (clean at 0
→ the gritty solid-state drive, matching the JC-120 Ch1/Ch2 captures);
Bass/Mid/Treble → tone stack, Pres → Hi-Treble. Reverb + Chorus sit OFF for
songs (Rocksmith adds those via its own pedals/racks) and stay editable by hand
(`_static` in `rs_knob_to_vst_param.json`).
