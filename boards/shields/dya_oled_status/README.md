# DYA OLED Status

A lightweight, DYA Studio friendly ZMK custom status screen shield.

## Usage

Use this shield instead of another custom display shield such as `nice_oled`:

```yaml
include:
  - board: seeeduino_xiao_ble
    shield: SAA_L_Base SAA_L_TB rgbled_adapter dya_oled_status
```

Do not combine it with another shield that also provides `zmk_display_status_screen()`.

## DYA Studio / Kconfig Options

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
CONFIG_DYA_OLED_STATUS_ANIMATION_CYBER_FACE=y
```

Peripheral battery display requires split central battery fetching:

```conf
CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING=y
```

## Layouts

- `CONFIG_DYA_OLED_STATUS_LANDSCAPE=y`: 128x64 style layout.
- `CONFIG_DYA_OLED_STATUS_PORTRAIT=y`: 64x128 style stacked layout.

The orientation is compile-time selectable so DYA Studio can expose it as a module setting.
