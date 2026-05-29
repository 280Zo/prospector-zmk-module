# Prospector ZMK Module

This is a [ZMK module](https://zmk.dev/docs/features/modules) that provides custom status screen support for the [Prospector](https://github.com/carrefinho/prospector) display dongle.

![Four status screen layouts for Prospector](docs/images/status-screen-update-hero.png)

> [!IMPORTANT]
> This branch is a work-in-progress and is only compatible with the Zephyr 4.1 version of ZMK (current main).

## Table of Contents

- [Features](#features)
- [Installation](#installation)
- [Status Screens](#status-screens)
- [Usage](#usage)
- [Display Page Controls](#display-page-controls)
- [Diagnostics Screen](#diagnostics-screen)
- [Hardware Wiring](#hardware-wiring)
- [Brightness Controls](#brightness-controls)
- [Idle Timeout (Display + Backlight)](#idle-timeout-display--backlight)
- [Configuration](#configuration)
- [Troubleshooting](#troubleshooting)
- [Known Issues](#known-issues)
- [To-Do](#to-do)

## Features

- Four status screen layouts to choose from
- Display blanks and backlight turns off on idle/sleep, then restores on activity
- Active layer display
- Peripheral battery status
- BLE profile and output indicator
- Active modifier display
- Caps word indicator

## Installation

Your ZMK keyboard should be set up with a dongle as central.

Add this module to your `config/west.yml` with these new entries under `remotes` and `projects`:

```yaml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: carrefinho                            # <--- add this
      url-base: https://github.com/carrefinho     # <--- and this
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main
      import: app/west.yml
    - name: prospector-zmk-module                 # <--- and these
      remote: carrefinho                          # <---
      revision: feat/new-status-screens           # <---
  self:
    path: config
```

Then add the `prospector_adapter` shield to the dongle in your `build.yaml`:

```yaml
---
include:
  - board: xiao_ble//zmk
    shield: [YOUR KEYBOARD SHIELD]_dongle prospector_adapter
```

For more information on ZMK Modules and building locally, see [the ZMK docs page on modules.](https://zmk.dev/docs/features/modules)

## Status Screens

Classic is used by default. To choose a different screen, add one of the following to your `.conf` file:

```ini
CONFIG_PROSPECTOR_STATUS_SCREEN_RADII=y
CONFIG_PROSPECTOR_STATUS_SCREEN_FIELD=y
CONFIG_PROSPECTOR_STATUS_SCREEN_OPERATOR=y
```

To capture Operator screen renders for documentation, see
[Operator Status Render](docs/operator-status-render.md). The render script
compiles the Operator LVGL widgets locally and keeps external ZMK/Zephyr/LVGL
dependencies in an ignored local cache.

## Usage

For split keyboards, the peripheral battery widget arranges sub-widgets in pairing order. After flashing the dongle, pair the left side first, then the right side. For more than two peripherals, pair them left to right.

The layer display shows the `display-name` property when available, falling back to the layer index otherwise. To add a `display-name` to a keymap layer:

```dts
keymap {
  compatible = "zmk,keymap";
  base {
    display-name = "Base";           # <--- add this
    bindings = <
      ...
    >;
  }
}
```

## Display Page Controls

Prospector exposes a `&pdp` behavior for switching display pages. Add this include to the top of
your keymap:

```dts
#include <dt-bindings/prospector/display_page.h>
```

Then bind keys like this:

```dts
bindings = <
  &pdp PDP_NEXT         /* switch to the next display page */
  &pdp PDP_PREV         /* switch to the previous display page */
  &pdp PDP_STATUS       /* switch directly to the theme status page */
  &pdp PDP_DIAGNOSTICS  /* switch directly to the diagnostics page */
>;
```

The CST816S touch screen can also switch pages with swipe gestures. Swipe right from the status
screen to show diagnostics. Swipe left from diagnostics to return to the status screen.

## Diagnostics Screen

The diagnostics screen is a compact runtime dashboard for the Prospector dongle. It is shown as a
display overlay, so the normal ZMK status screen remains the primary display page.

| Section | Description |
| --- | --- |
| `KEYS PRESSED` | Session key counter based on ZMK keycode press events. The value is capped internally and displayed compactly, for example `9999`, `12.3k`, `123k`, `1.2M`, or `999M+`. It resets on reboot. |
| `BRIGHT` | Current Prospector backlight brightness. `A 80%` means ambient-light auto mode; `M 80%` means manual or fixed mode. |
| `ALS` | Last cached ambient light reading from the APDS9960 brightness sampler, shown in lux. Non-ALS builds, or builds with no valid sample yet, show `N/A`. |
| `MEM` | LVGL/Zephyr display heap usage as `used/free` KiB, for example `12/20k`. This is the runtime pool used by the diagnostics overlay and status screen. |
| `PERIPHERALS` | Placeholder area for future split peripheral diagnostics such as RSSI and connection interval. |
| `FIRMWARE` | ZMK application version from Zephyr's generated `APP_VERSION_STRING`, shown as `zmk <version>`. |
| `UPTIME` | Time since boot, capped to a fixed-width `999d 23h 59m` format. |

## Hardware Wiring

Follow the pin-out diagram and tables to connect the electronics. You can solder
everything together or you can use mechanical connections for easy removal &
modification.

Display - Both SKU: 24382 and SKU: 27057

| Signal Name | nice!nano Pin | nice!nano GPIO | XIAO Pin | XIAO GPIO | Function | Prospector Wiring Note |
| --- | --- | --- | --- | --- | --- | --- |
| VCC | VCC | - | 3V3 | - | 3.3V Power | Direct to controller 3.3V |
| GND | GND | - | GND | - | Ground | Direct to controller GND |
| LCD_DIN | D4 | P0.22 | D10 | P1.15 | SPI MOSI | Data from controller to display |
| LCD_CLK | D3 | P0.20 | D8 | P1.13 | SPI SCK | Serial clock |
| LCD_CS | D7 | P0.11 | D9 | P1.14 | SPI chip select | Active low |
| LCD_DC | D5 | P0.24 | D7 | P1.12 | Data/command | High=data, low=command |
| LCD_RST | D6 | P1.00 | D3 | P0.29 | Display reset | Active low reset |
| LCD_BL | D1 | P0.06 | D6 | P1.11 | Backlight PWM | PWM brightness control |

Display Touch

> [!IMPORTANT]
> The nice!nano touch SCL pin changed from the original wiring. Early builds
> used `D21 / P0.31` for `TP_SCL`, but that pin proved unreliable on some cheaper
> Pro Micro-compatible controller boards. Current firmware uses `D15 / P1.13`
> for `TP_SCL`. If you already built Prospector with the original firmware and
> want to update, you'll need to move the `TP_SCL` wire from `D21` to `D15`.

| Signal Name | nice!nano Pin | nice!nano GPIO | XIAO Pin | XIAO GPIO | Function | Prospector Wiring Note |
| --- | --- | --- | --- | --- | --- | --- |
| TP_SDA | D20 | P0.29 | D4 | P0.04 | I2C SDA | Shared I2C data line |
| TP_SCL | D15 | P1.13 | D5 | P0.05 | I2C SCL | Shared I2C clock line |
| TP_RST | D0 | P0.08 | D1 | P0.03 | Touch reset | Active low reset |
| TP_IRQ | D8 | P1.04 | D0 | P0.02 | Touch interrupt | Active low interrupt |

APDS9960 Ambient Light Sensor (optional, needed for auto-brightness):

| APDS9960 Pin | nice!nano Pin | nice!nano GPIO | XIAO Pin | XIAO GPIO | Function | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| INT | D2 | P0.17 | D2 | P0.28 | Interrupt | Active low with pull-up |
| SCL | D15 | P1.13 | D5 | P0.05 | I2C SCL | Shared with touch SCL if touch is used |
| SDA | D20 | P0.29 | D4 | P0.04 | I2C SDA | Shared with touch SDA if touch is used |
| VCC / VIN | VCC / 3V3 | - | 3V3 | - | Power | Use 3.3V |
| GND | GND | - | GND | - | Ground | Common ground |
| VL / LED / 3Vo | - | - | - | - | Breakout-dependent | Unused by this firmware |

## Brightness Controls

Prospector exposes a `&pbl` behavior for display backlight control. Add this include to the top of
your keymap:

```dts
#include <dt-bindings/prospector/brightness.h>
```

Then bind keys like this:

```dts
bindings = <
  &pbl PBL_TOG  /* toggle ambient sensor/manual mode */
  &pbl PBL_INC  /* switch to manual mode and brighten */
  &pbl PBL_DEC  /* switch to manual mode and dim */
>;
```

Additional commands are available:

```dts
&pbl PBL_AUTO      /* force ambient sensor mode */
&pbl PBL_MANUAL    /* force manual mode at the current brightness */
&pbl PBL_SET 75    /* switch to manual mode and set 75% brightness */
```

`PBL_INC` and `PBL_DEC` use `CONFIG_PROSPECTOR_BRIGHTNESS_STEP`, which defaults to `10`.

## Idle Timeout (Display + Backlight)

`CONFIG_ZMK_IDLE_TIMEOUT` controls when ZMK transitions from `ACTIVE` to `IDLE` after no input activity.

To blank the display at idle, also enable:

```ini
CONFIG_ZMK_DISPLAY_BLANK_ON_IDLE=y
```

When that timeout is reached:
- ZMK display blanking turns the screen off.
- Prospector backlight is set to `0`.

`CONFIG_ZMK_DISPLAY_BLANK_ON_IDLE` affects display blanking only. Backlight shutoff is handled by this module on `IDLE`/`SLEEP` activity events.

When activity returns:
- The display is unblanked by ZMK.
- Backlight is restored by this module:
  - fixed mode: `CONFIG_PROSPECTOR_FIXED_BRIGHTNESS`
  - ambient mode: current sensor-driven brightness

Set the timeout in your dongle `.conf` file (value is in milliseconds):

```ini
# 60 seconds of inactivity before IDLE
CONFIG_ZMK_IDLE_TIMEOUT=60000
CONFIG_ZMK_DISPLAY_BLANK_ON_IDLE=y
```

Optional: if you also use ZMK sleep behavior, this module handles both `IDLE` and `SLEEP` by keeping the backlight off until activity resumes.

## Configuration

To customize, add config options to your `.conf` file:
```ini
CONFIG_PROSPECTOR_USE_AMBIENT_LIGHT_SENSOR=n
CONFIG_PROSPECTOR_FIXED_BRIGHTNESS=80
```

### General
| Name | Description | Default |
| ---- | ----------- | ------- |
| `CONFIG_PROSPECTOR_ROTATE_DISPLAY_180` | Rotate the display 180 degrees | n |
| `CONFIG_PROSPECTOR_USE_AMBIENT_LIGHT_SENSOR` | Use ambient light sensor for auto brightness | y |
| `CONFIG_PROSPECTOR_FIXED_BRIGHTNESS` | Fixed display brightness when not using ambient light sensor | 50 (1-100) |
| `CONFIG_PROSPECTOR_BRIGHTNESS_STEP` | Brightness percentage-point step for `&pbl PBL_INC` / `&pbl PBL_DEC` | 10 |
| `CONFIG_PROSPECTOR_LAYER_NAME_UPPERCASE` | Convert layer names to uppercase (Operator and Radii only) | y |
| `CONFIG_ZMK_IDLE_TIMEOUT` | Inactivity time before ZMK enters `IDLE` (backlight turns off; display blanking also requires `CONFIG_ZMK_DISPLAY_BLANK_ON_IDLE`) | ZMK default |

> [!NOTE]
> Prospector display backlight is forced to `0` when ZMK enters `IDLE` or `SLEEP`, and restored when activity returns to `ACTIVE`.
> In fixed mode it restores to `CONFIG_PROSPECTOR_FIXED_BRIGHTNESS`; in ambient-light mode it restores to the current sensor-driven brightness.

When ambient-light brightness is enabled, Prospector uses a module-owned APDS9960 patch driver
(`CONFIG_PROSPECTOR_APDS9960`). It keeps the normal Zephyr sensor API in use while narrowing the
driver to the display backlight's ALS-only needs. This patch avoids the stock driver's proximity
setup and handles an interrupt edge case where the APDS9960 INT line can already be active before
the fetch path starts waiting.

Do not also enable Zephyr's APDS9960 driver; leave `CONFIG_APDS9960=n` so only one driver binds to
the `avago,apds9960` devicetree node.

### Modifiers
| Name | Description | Default |
| ---- | ----------- | ------- |
| `CONFIG_PROSPECTOR_SHOW_MODIFIERS` | Display modifier key indicators | y |
| `CONFIG_PROSPECTOR_SHOW_INACTIVE_MODIFIERS` | Show inactive modifiers dimmed (Classic and Field only) | y |
| `CONFIG_PROSPECTOR_MODIFIER_ORDER` | Order of modifiers: G=GUI, A=Alt, C=Ctrl, S=Shift | "GACS" |

### Field-specific
| Name | Description | Default |
| ---- | ----------- | ------- |
| `CONFIG_PROSPECTOR_ANIMATION_WPM_REFERENCE` | WPM value at which animation reaches max speed | 70 |
| `CONFIG_PROSPECTOR_ANIMATION_INTENSITY_DECAY_SEC` | Seconds for lines to fade out after typing stops | 30 |
| `CONFIG_PROSPECTOR_ANIMATION_FLOW_DECAY_SEC` | Seconds for line directions and length to settle | 300 |

## Troubleshooting

### RAM overflow error

If you encounter a `region 'RAM' overflowed` error when building, add the following to your `.conf` file to reduce the display buffer size:

```ini
CONFIG_LV_Z_VDB_SIZE=25
```

## Known Issues

- One peripheral may fail to register key presses after connecting to the dongle; reset the affected peripheral to fix. https://github.com/zmkfirmware/zmk/issues/3156
- Operator, Radii: battery display only supports up to three peripherals

## To-Do

- Operator: per-profile BLE status
- Radii: document and improve color theme customization
- OS-specific modifier styles
- Caps lock indication
