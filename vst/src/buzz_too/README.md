# BuzzToo

Bundled DPF/VST3 fuzz for Rocksmith `Pedal_BuzzToo`.

Reference: local `pedals/buzz 2.jpg`, an early Big Muff V1 schematic with
four NPN transistor stages, two silicon diode clipping stages, passive Big Muff
tone stack, and output volume. Rocksmith exposes only:

- `Gain`: sustain/drive into the two clipping stages.
- `Tone`: continuous Big Muff tone stack balance from thick/dark to bright.

The output volume is internally normalized because Rocksmith does not expose a
Buzz 2 output knob.

Build:

```sh
make -C vst/src/buzz_too DPF_PATH=/private/tmp/DPF \
  DPF_BUILD_DIR=/private/tmp/dpf-build/BuzzToo \
  DPF_TARGET_DIR=/private/tmp/dpf-bin
codesign --force --sign - /private/tmp/dpf-bin/BuzzToo.vst3
```

Copy `BuzzToo.vst3` to `rig_builder/vst/`.
