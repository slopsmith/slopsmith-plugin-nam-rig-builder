# FuzzWasHe - silicon FZ-3 style fuzz (bundled VST3)

Small two-knob VST for Rocksmith's `Pedal_FuzzWasHe`.

It models only the controls Rocksmith exposes:

- `Gain`: fuzz amount into the middle silicon transistor gain stage.
- `Tone`: Big Muff-style dark/bright balance after the fuzz core.

The local `pedals/Fuzz Was He.pdf` schematic is used as the character
reference. It is the Aion FX Argent Silicon Fuzz, based on the Boss FZ-3:
silicon transistor stages, a Fuzz Face/Tone Bender-like core, and a Big
Muff-style tone network. This implementation is not a SPICE clone; it keeps the
audible cues needed in a Rocksmith rig slot while omitting the pedal's Volume
control.

## Build (macOS arm64)

```sh
make DPF_PATH=/private/tmp/DPF \
  DPF_BUILD_DIR=/private/tmp/dpf-build/FuzzWasHe \
  DPF_TARGET_DIR=/private/tmp/dpf-bin \
  PKG_CONFIG=false vst3
codesign --force --sign - /private/tmp/dpf-bin/FuzzWasHe.vst3
```

Copy `FuzzWasHe.vst3` to `rig_builder/vst/`.
