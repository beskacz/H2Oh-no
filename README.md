# H2Oh-no — garden irrigation controller (ESPHome)

**ESPHome** replacement firmware for the **Tuya / Smart Life Wi‑Fi 8‑zone irrigation controller** sold with a **24 V power supply**. Inside the enclosure you get a **BK7231N** module; this repo builds with **`bk72xx`** / **`generic-bk7231n-qfn32-tuya`**. Device name in config: **h2oh_no**, friendly name: **H2Oh-no**.

Firmware sources live under **`src/`** (YAML + custom component); repo root keeps **secrets** (`secrets.yaml`), docs, git metadata, and flash backups.

## Target hardware

| | |
|---|---|
| **Product** | Tuya / Smart Life **8‑zone** irrigation controller + **24 V** PSU bundle (panel with **Up / Down / OK**, buzzer, zone LEDs, **8** valve outputs). |
| **MCU / ESPHome** | **BK7231N** (`generic-bk7231n-qfn32-tuya`) |
| **Vendor specs (listing)** | Wi‑Fi **2.4 GHz** (802.11 b/g/n); **230 V AC** supply input; **24 V AC** output to controller and valves; enclosure about **211 × 139 × 20 mm**. |

### Photos

![Smart Life / Tuya Wi‑Fi 8-zone irrigation controller — product overview](docs/images/controller-product-overview.png)

![Front panel — status icons, zones 1–8, five-button pad (directions + center OK)](docs/images/controller-front-panel.png)

The same supplier range includes **4 / 6 / 16** zone models — **different PCBs**. This firmware is **only** aligned with the **8‑zone** unit unless you re‑verify GPIO.

## Using the controller (after flashing)

When the unit is on your **2.4 GHz Wi‑Fi** and **Home Assistant** has discovered it (ESPHome), you run **eight zones** from the **phone/computer** or from the **buttons on the case**.

### Front panel

1. **Pick a zone** with **Up** and **Down**. The **1–8** lights show which zones are **on**; when you move to another zone, its light **blinks a few times** so you see what you selected.
2. **Quick press the middle button (OK)** — **starts or stops** watering on that zone.
3. **Hold OK ~2–4 seconds, then release** — **everything stops** (all zones off) and the buzzer plays an **alarm-style** pattern.
4. **Hold OK ~5 seconds** — turns **service mode** on or off (beeps / LEDs when you **enter**). In service mode the controller **won’t leave valves open**, even if something in Home Assistant asks — handy while you work on wiring or valves.

If you **don’t press anything for 30 seconds**, the panel **forgets** which zone you had highlighted. After that, **Up** starts again from **zone 1**, and **Down** from **zone 8**.

### Small status LED (on the board)

- In **service mode** it does a **three quick flashes, then a long pause** — on purpose, so you don’t mix it up with a normal “fault” blink.
- The rest of the time it behaves like a normal **ESPHome status LED** (e.g. warning/error patterns if the firmware needs to tell you something).

### In Home Assistant

You get **a switch per zone**, **how long each zone may run** before it turns off by itself, a toggle for **one zone only vs several at once**, an **Emergency STOP** control, and optional **diagnostics** text if you want to watch live state.

### Wi‑Fi problems

If home Wi‑Fi fails, ESPHome can open a **fallback hotspot** and **captive portal** so you can fix the password — same flow as other ESPHome devices.

## Behaviour & limits (detail)

These rules sit behind the scenes; you mostly notice them as “zone turned off by itself” or “only one zone stayed on”.

- **Per-zone timer:** Each open zone uses its **Valve N max runtime** from Home Assistant. When time is up, **only that zone** turns off. You can usually set **about 10 seconds up to one hour** per zone from HA unless defaults were changed in YAML.
- **Service mode:** Any attempt to open a valve (panel or HA) is **undone immediately** — all valves stay closed.
- **Allow multiple valves** (HA): When **off**, only **one** zone may run; if several were on, **the lowest-number zone stays** and the others close.
- **Power-on:** Valves start **closed**; **no** startup beep. Intentional **Emergency STOP** still uses the **alarm** beeps.

The **“H2Oh-no diagnostics”** text sensor refreshes about every **5 seconds** with a short status line (which zones are on, service mode, multi-zone setting, panel selection) — useful when debugging from HA.

## Repository layout

| Path | Description |
|------|----------------|
| `docs/images/` | Product photos used in this README |
| `src/h2oh_no.yaml` | Main ESPHome configuration |
| `src/components/h2oh_no_controller/` | External component: `__init__.py`, `h2oh_no_controller.h/.cpp` |
| `src/secrets.yaml` | ESPHome stub only — merges in root `secrets.yaml` (committed; **no secrets here**) |
| `secrets.yaml` | **Your** Wi‑Fi / API / OTA keys — repo **root**, **gitignored** |
| `secrets.yaml.example` | Copy this to `secrets.yaml` at repo root |

## Secrets

**Your passwords and keys belong in `secrets.yaml` at the repository root.** Copy from `secrets.yaml.example` and fill in every field.

**Why `src/secrets.yaml` exists:** ESPHome loads a file named exactly `secrets.yaml` from **the same directory as the config file**, so alongside `src/h2oh_no.yaml` it only looks at `src/secrets.yaml`. That file is **not** where you edit secrets — it’s a tiny committed stub that re-exports the root file (same idea as the [ESPHome FAQ](https://esphome.io/guides/faq.html) “include parent secrets” pattern):

```yaml
<<: !include ../secrets.yaml
```

Run these from the **repo root**:

```bash
cp secrets.yaml.example secrets.yaml
# edit secrets.yaml
```

If root `secrets.yaml` is missing or incomplete, `python3 -m esphome compile src/h2oh_no.yaml` fails on `!secret` lookups.

## Build and flash

You need Python with **ESPHome** installed (e.g. `pip install esphome`).

Run commands from the **repository root** (or pass absolute paths):

```bash
python3 -m esphome compile src/h2oh_no.yaml
python3 -m esphome upload src/h2oh_no.yaml    # with device / network available
```

## Hardware pinout (GPIO from `src/h2oh_no.yaml`)

Maps for the **8‑zone** controller above — treat YAML as source of truth.

- Valve 595: data **P16**, clock **P22**, latch **P20**
- LED 595: data **P9**, clock **P15**, latch **P17** — status LEDs use **`inverted: true`** on each output so power-on zero state does not light every LED (common active-low LED wiring to the shift register).
- Buzzer: **P14**
- Buttons: **P6** / **P7** / **P8** (pull-up, inverted)
- Onboard **status LED** (**P28**, optional in YAML via `status_led_pin`): driven by `h2oh_no_controller` — **triple flash + pause** in **service mode**, otherwise same **error/warning** patterns as ESPHome’s stock `status_led`.

Treat `src/h2oh_no.yaml` as the source of truth for pins and entities.

## Flash backups

`.bin` dumps in the repo root are **hardware backups** of raw flash, not part of the ESPHome build pipeline.
