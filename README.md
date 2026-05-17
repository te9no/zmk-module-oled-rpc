# zmk-module-oled-rpc

`zmk-module-oled-rpc` is a ZMK external module that provides a custom OLED status screen plus a DYA Studio custom subsystem for runtime editing.

## Features

- Central battery display
- Peripheral battery display on split central builds
- Output status display
- Active layer display
- WPM display
- Active modifiers display
- HID indicators display: Num Lock, Caps Lock, Scroll Lock
- Runtime portrait/landscape switching through Studio RPC
- Runtime per-widget visibility switching through Studio RPC
- Selectable OLED animation, including the cyber face illustration animation
- Web UI under `web/` for editing the custom subsystem from DYA Studio

## Firmware Installation

Add the module to `config/west.yml`:

```yaml
manifest:
  remotes:
    - name: te9no
      url-base: https://github.com/te9no
  projects:
    - name: zmk-module-oled-rpc
      remote: te9no
      revision: main
```

For local development, pass the module through `ZMK_EXTRA_MODULES`:

```bash
west build -s zmk/app -b seeeduino_xiao_ble -- \
  -DZMK_CONFIG=/path/to/config \
  -DZMK_EXTRA_MODULES=/path/to/zmk-module-oled-rpc \
  -DSHIELD="SAA_L_Base rgbled_adapter dya_oled_status"
```

Use `dya_oled_status` instead of another custom display status shield:

```yaml
include:
  - board: seeeduino_xiao_ble
    shield: SAA_L_Base SAA_L_TB rgbled_adapter dya_oled_status
```

Do not combine it with `nice_oled`, `dongle_display`, or any other shield that also provides `zmk_display_status_screen()`.

## Studio RPC

The firmware exposes this custom subsystem when ZMK Studio RPC is enabled:

- Subsystem identifier: `dya__oled_status`
- Firmware option: `CONFIG_DYA_OLED_STATUS_STUDIO_RPC=y`
- UI URL in firmware: `https://te9no.github.io/zmk-module-oled-rpc/`

Runtime settings are persisted through Zephyr settings when `CONFIG_SETTINGS=y`.
Widget visibility and animation apply immediately. Orientation is saved immediately but is applied on the next reboot to avoid unsafe runtime display rotation on SSD1306/LVGL.

## Kconfig Defaults

These options control the firmware defaults and the compiled-in capabilities exposed to the Web UI:

```conf
CONFIG_DYA_OLED_STATUS_LANDSCAPE=y
# CONFIG_DYA_OLED_STATUS_PORTRAIT=y

CONFIG_DYA_OLED_STATUS_WIDGET_BATTERY_CENTRAL=y
CONFIG_DYA_OLED_STATUS_WIDGET_BATTERY_PERIPHERAL=y
CONFIG_DYA_OLED_STATUS_WIDGET_OUTPUT=y
CONFIG_DYA_OLED_STATUS_WIDGET_LAYER=y
CONFIG_DYA_OLED_STATUS_WIDGET_WPM=n
CONFIG_DYA_OLED_STATUS_WIDGET_MODIFIERS=y
CONFIG_DYA_OLED_STATUS_WIDGET_HID_INDICATORS=y
CONFIG_DYA_OLED_STATUS_STUDIO_RPC=y
CONFIG_DYA_OLED_STATUS_ANIMATION_CYBER_FACE=y
# CONFIG_DYA_OLED_STATUS_ANIMATION_OFF=y
```

Peripheral battery display requires ZMK split central battery fetching:

```conf
CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING=y
```

## Web UI

The Web UI source is in `web/`. It talks to the `dya__oled_status` custom subsystem and can edit:

- Orientation: landscape or portrait, applied after reboot
- Animation: off or cyber face
- Central battery visibility
- Peripheral battery visibility
- Output, layer, WPM, modifiers, and HID indicator visibility

Build it with:

```bash
cd web
npm install
npm run build
```

GitHub Actions publishes the Web UI to GitHub Pages on pushes to `main`.

Repository settings required on GitHub:

1. Open `Settings` -> `Pages`.
2. Set `Build and deployment` source to `GitHub Actions`.
3. Push to `main`; the `Deploy Web UI` workflow publishes `web/dist/`.
