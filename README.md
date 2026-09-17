# NX-FanControl

Full control over your Nintendo Switch's fan: custom fan curves, named profiles,
per-game profiles that switch automatically, and your choice of which
temperature sensor drives the fan.

It has three parts:

- **Sysmodule** — runs in the background from boot and actually drives the fan.
- **Overlay** — a Tesla overlay for quick changes without leaving your game.
- **NXFanControl Manager** — a homebrew app with the full editor.

> **About this fork**
>
> This is a fork of [NX-FanControl](https://github.com/Lightos1/NX-FanControl).
> **Every change in this fork was written by Claude Code**, Anthropic's AI
> coding assistant. I'm not a developer: I described what I wanted, and tested
> each change on my own console (a Switch Lite). See [CHANGELOG.md](./CHANGELOG.md)
> for everything that changed from upstream v1.1.2.

---

## Features

- **Custom fan curves** — as many temperature/fan-speed points as you want, on a
  live graph.
- **Profiles** — up to 8 named profiles, each with its own handheld and docked
  curve.
- **Per-game profiles** — assign a profile to a game. It switches in when the
  game starts and your previous profile comes back when the game closes.
- **Presets** — start a new profile from a copy of an existing one, from
  **Default**, or from **Stock-like**.
- **Selectable curve sensor** — drive the fan from SoC, PCB, Skin, or the CPU,
  GPU or RAM die temperature (Horizon OC needed for those three).
- **Live monitoring** — every temperature and the fan speed, updated in real time.
- **Docked profiles** — a separate curve for when the console is docked.
- **Refresh settings** — control how often the sysmodule checks temperatures,
  settings and dock state.

---

## Installation

1. Download `NX-FanControl-v1.2.0.zip` from the
   [latest release](https://github.com/GoatHonks/NX-FanControl/releases/latest).
2. Extract it to the **root of your SD card**, merging with the folders already
   there.
3. **Reboot the console.**

The zip contains:

| File | What it is |
|---|---|
| `atmosphere/contents/00FF0000B378D640/` | The sysmodule, set to start at boot |
| `switch/.overlays/NX-FanControl.ovl` | The overlay |
| `switch/NX-FanControl/NXFanControl-Manager.nro` | The Manager app |

**Requirements:** Atmosphère. To use the overlay you also need nx-ovlloader
and an overlay menu such as Ultrahand. Horizon OC is optional, and only needed
for the CPU, GPU and RAM temperature sensors.

> **When updating, always reboot.** The sysmodule loads once at boot, so copying
> new files over it doesn't replace the copy that's already running.

Settings are stored in `config/NX-FanControl/config.ini` on the SD card.
Upgrading from an earlier version converts your existing config automatically.

---

## Using the overlay

The overlay is for quick changes while you play.

**Main screen**

| Item | What it does |
|---|---|
| **Enabled** | Turns custom fan control on or off |
| **Profile** | Shows the active profile. Opens the profile screen |
| **Per-Game Profiles** | Opens the per-game screen |
| **Settings** | Opens the settings screen |
| **State / Temp / Fan Speed** | Live dock state, the temperature of the sensor the curve uses, and fan speed |
| **Fan Curve** | Live graph of the curve in use |
| **Edit Handheld Curve / Edit Docked Curve** | Add, move and delete curve points |

**Profile screen** — select a profile to make it active, **+ Add Profile** to
make a copy of the active one, or hold **A** on **Delete Active Profile**.
Renaming is done in the Manager.

**Per-Game Profiles screen** — turn the feature on or off, and assign the game
that's currently running. Select the game to pick a profile. With only one
profile, it's assigned straight away.

**Settings screen**

| Setting | What it does |
|---|---|
| **Curve Sensor** | Opens a list of every sensor with its live reading |
| **Docked Profiles** | Uses each profile's docked curve while docked |
| **High Refresh Temp** | Above this temperature, the faster interval is used |
| **Low Temp Interval** | How often the fan is updated while cool |
| **High Temp Interval** | How often the fan is updated while hot |
| **Config Refresh Interval** | How often the sysmodule checks for changed settings |
| **Enable Refresh Interval** | How often it checks the Enabled switch while off |
| **Docked Refresh Interval** | How often it checks whether the console is docked |

---

## Using NXFanControl Manager

The Manager is the full editor. Launch it from the Homebrew Menu. It can do
everything the overlay can, plus renaming, reordering and presets, which need
the Switch's keyboard (an overlay can't open it).

The header shows all six temperatures, fan speed and dock state live, with the
sensor driving the curve highlighted. It also shows your console model, and
whether the sysmodule and Horizon OC are running.

| Button | Profiles | Fan Curve | Games | Settings |
|---|---|---|---|---|
| D-Pad Up/Down | Select profile | Select point | Select game | Select setting |
| D-Pad Left/Right | — | Fan speed −/+ | — | Adjust value |
| A | Make active | Add point | Per-game profiles on/off | Toggle |
| X | Rename | Delete point | Remove assignment | — |
| Y | New profile | Handheld / docked curve | — | — |
| L / R | Move up / down | Temperature −/+ | — | — |
| MINUS | Delete profile | — | — | — |
| ZL / ZR | Switch tab | Switch tab | Switch tab | Switch tab |
| PLUS | Exit | Exit | Exit | Exit |

The **Games** tab lists every assigned game as `Game Name [Title ID]`. Games
that have been archived or deleted show only their title ID.

---

## Profiles

A profile is a named pair of curves: one for handheld, one for docked. Switching
profiles swaps both. Changes take effect within about 2 seconds, with no reboot.

**Default profile.** The first time this version runs, the Default profile is
filled with the Stock-like curves so there's something sensible out of the box.
If Default already held a curve, it's replaced — copy it to a new profile first
if you want to keep it. After that, Default is an ordinary profile: edit,
rename or delete it freely, and your changes are never reset.

**Presets.** When creating a profile in the Manager you can start from a copy of
the highlighted profile, from **Default**, or from **Stock-like**.

**Stock-like is an approximation, not Nintendo's actual curve.** Nintendo's
stock fan table is based on *skin* temperature, while curves here usually use
the hotter *SoC* sensor. The preset keeps the shape of the stock curve — a ~20%
floor, a long plateau, then a ramp — with the steps shifted to SoC
temperatures. It also ramps to 100% at the top, where stock handheld stops at
60%.

**Limits.** Up to 8 profiles. Names can be up to 24 bytes, which is 24
characters in English, and fewer in languages like Japanese.

---

## Per-game profiles

1. Launch the game.
2. Open the overlay and go to **Per-Game Profiles**.
3. Select the game and pick a profile. This turns **Per-Game Profiles** on if it
   was off.

From then on:

- **Game starts** — the active profile switches to the game's profile.
- **Game closes** — the profile you had before comes back.
- **You change profile by hand while playing** — your choice is kept when the
  game closes.
- **Reboot or crash mid-game** — your previous profile is still restored.

Changes while a game is running take effect within about 2 seconds. Up to 64
games can be assigned. To remove one, use the overlay's Per-Game Profiles
screen while the game is running, or the Manager's **Games** tab at any time.

---

## Temperature sensors

| Sensor | What it measures | Needs |
|---|---|---|
| **SoC** | TMP451 sensor at the processor package. The default | — |
| **PCB** | TMP451's own reading, near the board | — |
| **Skin** | Nintendo's estimate of the console's outside temperature. Much cooler than SoC; the stock fan curve uses this one | — |
| **CPU** | Temperature inside the CPU | Horizon OC |
| **GPU** | Temperature inside the GPU | Horizon OC |
| **RAM** | Temperature inside the memory controller (PLLX on Mariko models) | Horizon OC |

Choose one under **Settings → Curve Sensor** in the overlay or Manager.

If the chosen sensor stops responding, the fan uses SoC until it's back,
rather than being left unmanaged. Without Horizon OC, CPU/GPU/RAM show **N/A**.

**Re-tune your curve when you change sensor.** CPU and GPU run hotter than SoC
and react to load sooner, while Skin runs much cooler, so the same temperature
points behave very differently.

**Why CPU/GPU/RAM need Horizon OC:** those sensors can only be read with
hardware access that homebrew apps aren't given, however they're launched.
Horizon OC's sysmodule can read them, so NX-FanControl asks it for the values.

---

## Common issues

### Fan always stays on

The Switch has its own fan controller, Nintendo's `tc` sysmodule, and it keeps
running alongside NX-FanControl. NX-FanControl updates the fan far more often,
so it normally wins, but `tc` still applies its own curve underneath.

A suggested fix is to change `tc`'s curve through Atmosphère by adding these
lines to `atmosphere/config/system_settings.ini`, then rebooting:

```ini
[tc]
use_configurations_on_fwdbg=u8!0x1
tskin_rate_table_console_on_fwdbg=str!"[[-1000000, 40000, 0, 0], [36000, 43000, 51, 51], [43000, 49000, 51, 128], [49000, 54000, 128, 255], [54000, 1000000, 255, 255]]"
tskin_rate_table_handheld_on_fwdbg=str!"[[-1000000, 40000, 0, 0], [36000, 43000, 51, 51], [43000, 49000, 51, 128], [49000, 54000, 128, 255], [54000, 1000000, 255, 255]]"
holdable_tskin=u32!0xEA60
touchable_tskin=u32!0xEA60
```

Before you do:

- **Add these lines; don't replace the file.** Atmosphère reads only one
  `system_settings.ini`, so copying a new file over yours deletes every setting
  you had.
- **Know what it changes.** It makes `tc`'s curve more aggressive (100% fan at
  about 54 °C skin temperature, where stock handheld stops at 60%), and raises
  the skin temperature limits Nintendo uses for heat protection to 60 °C.

These lines come from the `[tc]` section of
[Dominatorul's Easy-Setup `system_settings.ini`](https://github.com/dominatorul/Easy-Setup/blob/main/data/Optimizer/EmuNAND/system_settings.ini).
That full file also changes about 80 unrelated settings (telemetry, background
downloads, cloud saves, USB 3.0 and more), so only use the whole file if you
want all of those.

### Changes don't seem to apply after updating

Reboot the console. See [Installation](#installation).

### CPU / GPU / RAM show N/A

Horizon OC isn't installed or isn't running. The Manager's header says whether
it was detected.

### Something else isn't working

The sysmodule writes what it's doing to `config/NX-FanControl/log.txt`,
including profile switches and sensor problems. The log starts fresh at every
boot.

---

## Building from source

Install the [devkitPro toolchain](https://devkitpro.org/wiki/Getting_Started)
with the `switch-dev` group, plus these packages:

```bash
pacman -S switch-curl switch-zlib switch-mbedtls switch-libjson-c switch-sdl2 switch-sdl2_ttf switch-sdl2_gfx zip
```

The first four are needed by the overlay library, the SDL2 packages by the
Manager, and `zip` to package the release.

Then:

```bash
git clone https://github.com/GoatHonks/NX-FanControl.git --recurse-submodules
cd NX-FanControl
./build.sh
```

The files are placed in `dist/`, along with `dist/NX-FanControl.zip`.

### Tests

The config, profile, preset, per-game and sensor-selection logic has a test
suite that runs on your PC with a normal `gcc` (not devkitPro):

```bash
./tests/run.sh
```

---

## Credits

- **Zathawo** — original NX-FanControl
- **Dominatorul** — fork
- **Lightos_** — NX-FanControl, the upstream this fork is based on
- **CTCaer** — TMP451 temperature driver
- **ppkantorski** — libultrahand / Tesla overlay library
- **CompuPhase** — minIni
- **Souldbminer, Lightos_ and Horizon OC contributors** — Horizon OC IPC client,
  used for the CPU/GPU/RAM temperatures
- **Status-Monitor-Overlay** — reference for how those temperatures are obtained
- **Claude Code** (Anthropic) — wrote all the changes in this fork

## License

NX-FanControl as a whole is distributed under the **GNU General Public License,
version 2** — see [LICENSE](./LICENSE). It links GPLv2 components (the
libultrahand overlay library, CTCaer's TMP451 driver and the Horizon OC IPC
client), which requires the combined work to carry the same licence.

The original authors' code remains available under the **MIT License**,
preserved unchanged in [LICENSE.MIT](./LICENSE.MIT).

[COPYING.md](./COPYING.md) lists every component and library, and its licence.
