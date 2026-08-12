# voyager-ps firmware

[ZMK](https://zmk.dev) configuration for **voyager-ps**, a personal ErgoDox-style
split keyboard: two nice!nano v2 (nRF52840) halves, 5×7 matrix per side, one EC11
encoder per half, and 28 SK6812-mini-e underglow LEDs per side.

The hardware source — the ergogen layout, footprints, and the PCBs — lives in a
separate private repository. This repo is only the firmware.

> ### This config will not work on an unmodified board
>
> It assumes a **D0 ↔ D16 bodge**. The LED data line needs a high-frequency-capable
> pin, because low-frequency I/O on the nRF52840 causes BLE interference. Stock
> layout put matrix row *num* on D0 (P0.08) and left an unused NFC pin on D16
> (P0.10); this board moves row *num* to D16 and takes LED data on D0.
>
> Flash this onto a board without that rework and you get a **dead number row and
> dead LEDs**. See `boards/shields/voyager_ps/voyager_ps.dtsi` and
> `boards/shields/voyager_ps/boards/nice_nano_nrf52840_zmk.overlay`.

## Getting firmware

Every push builds via GitHub Actions. Download the artifact from the run, then
per half: hold reset to enter the bootloader and drag the `.uf2` onto the mounted
drive. The halves flash independently.

| Artifact | What it is |
|---|---|
| `voyager_ps_left`, `voyager_ps_right` | the normal build |
| `voyager_ps_left_niceview`, `voyager_ps_right_niceview` | with a nice_view display |
| `settings_reset` | wipes NVS on a unit with wedged settings — flash it, then reflash the normal artifact |

Board identifier is `nice_nano//zmk` (Zephyr 4.x HWMv2). The old `nice_nano_v2`
form is not recognised.

## ZMK is pinned

`config/west.yml` pins ZMK to a specific commit rather than tracking `main`,
because `zephyr/CMakeLists.txt` string-matches upstream source (below) and a
floating revision means an unrelated upstream refactor can break the build on a
push that only added a keybinding — and that the firmware on the keyboard cannot
be rebuilt from source.

To step forward, as its own commit and its own flash-and-test cycle:

```sh
gh api repos/zmkfirmware/zmk/commits/main --jq .sha
gh api "repos/zmkfirmware/zmk/compare/<old>...<new>" --jq '.files[].filename' | grep '^app/'
```

If that touches `app/src/rgb_underglow.c`, re-read `set_hsb()` before bumping.

The reusable build workflow is deliberately *not* pinned: it pulls
`checkout`/`cache`/`upload-artifact`, GitHub hard-deprecates old action majors, and
the toolchain container tag floats regardless. All source pinned, toolchain
floating.

## The RGB persistence patch

`zephyr/CMakeLists.txt` patches upstream ZMK at configure time so underglow
brightness, hue and saturation survive a reboot.

Upstream, `binding_convert_central_state_dependent_params` rewrites `RGB_BRI` /
`RGB_HUI` / `RGB_SAI` and friends into `RGB_COLOR_HSB_CMD` *before*
`binding_pressed` runs, so every keymap or encoder binding lands in
`zmk_rgb_underglow_set_hsb()` — which assigns `state.color` and returns without
scheduling a settings save. That is the only path a keymap can reach, so HSB is
lost on every power cycle. Only `RGB_TOG`/`RGB_ON`/`RGB_OFF` and effect selection
persist on their own.

The patch rewrites that `return 0;` into `return zmk_rgb_underglow_save_state();`.
It is idempotent, and it **fails the build loudly** if the needle no longer
matches, because a patch that quietly stops applying reintroduces the original
bug with no signal — which is how this regressed once before. If upstream ever
fixes it properly, the patch detects the already-correct form, skips, and can be
deleted.

Lighting persistence also depends on `CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE=1000`
(see `voyager_ps_left.conf`). At the stock 60 s, a power cycle within a minute of
a knob twist silently loses the setting.

## Layout

```
boards/shields/voyager_ps/
├── voyager_ps.dtsi              # matrix, encoders, sensors — shared
├── voyager_ps-layouts.dtsi      # physical layout (58 keys)
├── voyager_ps_{left,right}.overlay
├── voyager_ps_{left,right}.conf # must stay byte-identical; CI enforces it
├── voyager_ps.keymap            # shared across both halves
└── boards/
    └── nice_nano_nrf52840_zmk.overlay   # SPI3 + LED strip pinctrl
```

Four layers: `Base`, `Nav`, `Sym`, and `Adjust` — the last reached by holding both
thumb layer keys. Each knob has its own job per layer, and clockwise is always the
"more / forward" direction:

| layer | left knob | right knob | left push | right push |
|---|---|---|---|---|
| Base | volume up/down | cursor right/left | mute | Delete |
| Nav | page down/up | cursor down/up | — | — |
| Sym | next/prev track | screen brightness | — | — |
| Adjust | RGB brightness | RGB hue | RGB on/off | RGB on/off |

ZMK Studio is deliberately not supported — see the note in `voyager_ps.zmk.yml`.

## License

MIT, matching upstream ZMK.
