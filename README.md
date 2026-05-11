# H2Oh-no — garden irrigation controller (ESPHome)

**ESPHome** firmware for an irrigation controller built around **BK7231N** (e.g. Tuya CBU module / `generic-bk7231n-qfn32-tuya`). Device name in config: **h2oh_no**, friendly name: **H2Oh-no**.

## What it does

- Drives **8 valves** via a **74HC595** shift register (`valves_hub`).
- Drives **8 status LEDs** on a second 595 chain (`leds_hub`).
- Panel **OK**: short tap toggles selected valve; **hold ~2–4.5 s and release** triggers **Emergency STOP**; **hold ≥ 5 s** toggles **service mode** (with buzzer/LED cue on enter). **Up/Down/OK** restart a **30 s** idle timer: if **no panel button** is pressed for 30 s, the controller **leaves manual selection** (`sel=-1` in diagnostics). The next **Up** selects **valve 1** (without skipping it); the next **Down** selects **valve 8**. **Emergency stop** clears valves and sets selection back to valve 1.
- **Up/Down** also runs a **3-pulse** highlight on the newly selected LED: each pulse briefly **inverts** vs that valve’s real on/off state (dim-if-on / bright-if-off), then all LEDs are restored from the valve mask so active zones stay correctly lit.
- **Service mode**: onboard **status LED (P28)** uses a **triple short flash + long pause** pattern (easy to tell apart from ESPHome **error** / **warning** blinks). When not in service mode, that pin mirrors the stock ESPHome **status LED** patterns (error / warning), same timing as the built-in component used before.
- **Home Assistant** integration (API), **OTA**, fallback **Wi‑Fi AP** + captive portal.
- Valve logic, watchdogs, and service mode live in a **custom C++ component** (`h2oh_no_controller`); YAML keeps Wi‑Fi, HA entities, and buzzer scripts.

## Repository layout

| Path | Description |
|------|----------------|
| `h2oh_no.yaml` | Main ESPHome configuration |
| `components/h2oh_no_controller/` | External component: `__init__.py`, `h2oh_no_controller.h/.cpp` |
| `secrets.yaml` | Passwords and keys — **do not commit** (listed in `.gitignore`) |
| `secrets.yaml.example` | Template of secret names to copy |

## Secrets

```bash
cp secrets.yaml.example secrets.yaml
```

Fill in `secrets.yaml` with your network credentials and keys. Without it, `esphome compile` will fail because `h2oh_no.yaml` uses `!secret`.

## Build and flash

You need Python with **ESPHome** installed (e.g. `pip install esphome`).

```bash
python3 -m esphome compile h2oh_no.yaml
python3 -m esphome upload h2oh_no.yaml    # with device / network available
```

## Behaviour (short)

- **Watchdog**: one timer **per open valve** — when its limit expires (number entity “Valve N max runtime”), it closes **only that** valve. `NaN` / missing state defaults to **600 s**, clamped to **10–3600 s**.
- **Service mode** (long press OK): if anything tries to open a valve (e.g. from HA), the controller **immediately closes all** valves.
- **Allow multiple valves** (HA switch): when turned off with more than one valve ON, **the lowest-numbered open valve stays open** and the rest close. The same rule applies if HA tries to open multiple valves while multi-valve mode is off.
- **Boot**: `safe_boot` closes valves **without** sound. Full alarm (`beep_error`) runs on intentional **Emergency STOP** and other scripts that call it.

The **“H2Oh-no diagnostics”** text sensor (about every 5 s) prints valve mask, service flag, multi-valve flag, and panel selection index — handy when debugging from HA.

## Hardware (GPIO from `h2oh_no.yaml`)

- Valve 595: data **P16**, clock **P22**, latch **P20**
- LED 595: data **P9**, clock **P15**, latch **P17** — status LEDs use **`inverted: true`** on each output so power-on zero state does not light every LED (common active-low LED wiring to the shift register).
- Buzzer: **P14**
- Buttons: **P6** / **P7** / **P8** (pull-up, inverted)
- Onboard **status LED** (**P28**, optional in YAML via `status_led_pin`): driven by `h2oh_no_controller` — **triple flash + pause** in **service mode**, otherwise same **error/warning** patterns as ESPHome’s stock `status_led`.

Treat `h2oh_no.yaml` as the source of truth for pins and entities.

## Flash backups

`.bin` dumps in the repo are **hardware backups** of raw flash, not part of the ESPHome build pipeline.
