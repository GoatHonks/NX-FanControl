# NX-FanControl

**NX-FanControl** lets you fully customize your console’s fan curve.

---

## Features

* **Custom fan curve**: Set your own temperature-to-fan-speed points, with as many points as you need for more precise control.
* **Real-time monitoring**: Watch six temperature sources (SoC, PCB, Skin, and CPU/GPU/RAM via Horizon OC) plus fan speed, live on a graph.
* **Selectable curve sensor**: Drive the fan from whichever of those six sources suits you.
* **Per-game profiles**: Bind a profile to a game and it applies automatically when that game runs.
* **Profiles**: Create named profiles (up to 8), each with its own fan curve, and switch between them from the overlay. Every profile also keeps a separate docked curve, so "Docked Profiles" still applies within whichever profile is active.
* **NXFanControl Manager**: The main homebrew app — profile management with the system keyboard, full fan curve editing on a live graph, and every sysmodule setting. The overlay remains the quick in-game editor.
* **Refresh interval settings**: Control how often different parts of the sysmodule update:
    - **Docked check interval**: How often the sysmodule checks whether the device is docked.
    - **Custom control check interval**: How often the sysmodule checks whether custom fan control is turned on.
    - **Config reload interval**: How often the sysmodule re-reads your configuration.
    - **High-temperature refresh interval**: A faster update rate used once temperatures get high, for quicker response.
    - **Low-temperature refresh interval**: A slower update rate used when temperatures are low, to save resources.
* **High-temperature threshold**: The temperature at which the sysmodule switches to the faster (high-temperature) refresh interval.

---

## Profiles

A profile is a named set of fan curves. The active profile is stored in
`config.ini` and the sysmodule picks it up on its next config reload, so
switching takes effect within a couple of seconds without a reboot.

Each profile stores **both** a handheld and a docked curve, so switching
profiles swaps the pair together. "Docked Profiles" decides whether the docked
curve of the active profile is used.

**NXFanControl Manager** (`switch/NX-FanControl/`) is the main app: profiles,
full curve editing with a live graph, and every sysmodule setting. Renaming
lives here rather than in the overlay because Tesla overlays cannot open the
Switch's on-screen keyboard — the overlay loader has no way to launch a library
applet, which `swkbd` requires. The app runs as normal homebrew, so it has full
keyboard access.

**The overlay** is the quick in-game companion: switch profile, tweak a curve,
toggle fan control, without leaving your game.

| Manager | Profiles tab | Fan Curve tab | Games tab | Settings tab |
|---|---|---|---|---|
| D-Pad Up/Down | Move between profiles | Select point | Select game | Select setting |
| D-Pad Left/Right | — | Fan speed −/+ | — | Adjust value |
| A | Set active | Add point | Toggle per-game profiles | Toggle |
| X | Rename | Remove point | Unassign game | — |
| Y | New profile | Handheld / docked curve | — | — |
| L / R | Reorder | Temperature −/+ | — | — |
| MINUS | Delete profile | — | — | — |
| ZL / ZR | Switch tab | Switch tab | Switch tab | Switch tab |
| PLUS | Exit | Exit | Exit | Exit |

Existing configurations are upgraded automatically; see the note on the Default
profile below.

### The Default profile

Default is an ordinary profile you can edit, rename and delete like any other.
It is simply seeded with the Stock-like curves the first time the tool runs, so
there is something sensible there out of the box.

On first launch after upgrading, whatever curve previously sat in Default is
replaced by the Stock-like curves. If you want to keep a curve you had there,
copy it to a new profile before updating. After that first seeding, your edits
to Default persist and are never reset.

### Presets

When you create a profile in the Manager you can start it from a copy of an
existing profile, from **Default**, or from **Stock-like**.

**Stock-like is an approximation, not Nintendo's actual curve.** The stock fan
table in the `tc` sysmodule is indexed on *skin* temperature, while this tool
drives the fan from the TMP451 *SoC* sensor, which runs considerably hotter.
The preset preserves the shape of the stock curve — a ~20% floor, a long
plateau, then a ramp — with the breakpoints shifted into SoC terms. It also
ramps to 100% at the top, where stock handheld stops at 60%, because an
SoC-driven curve has to cover cases the skin sensor never sees.

### Per-game profiles

A profile can be bound to a specific game, so launching it switches the fan
curve automatically. Games with no binding use the profile you selected
manually.

Bindings are made **from the overlay**, because it is the only part of the tool
that runs while a game is in the foreground:

1. Launch the game
2. Open the overlay and select **Per-Game Profiles**, under **Profile**. The
   running game is listed by name
3. Select it and pick a profile (with only one profile, it's assigned
   directly). Assigning turns the **Per-Game Profiles** switch on if it was off

The Manager's **Games** tab lists every binding and can remove them. The
sysmodule re-checks the running title every couple of seconds.

---

## Temperature sensors

| Source | What it is |
|---|---|
| **SoC** | TMP451 remote diode at the SoC package. Hottest and most responsive — the default. |
| **PCB** | TMP451 local channel, the sensor chip's own die, near the board. |
| **Skin** | Nintendo's computed exterior-temperature estimate, read from the `tc` sysmodule. Runs far cooler than SoC; it is what the stock fan curve is indexed on. |
| **CPU** | Tegra SOC_THERM CPU die temperature. Needs Horizon OC. |
| **GPU** | Tegra SOC_THERM GPU die temperature. Needs Horizon OC. |
| **RAM** | Tegra SOC_THERM memory die temperature (PLLX on Mariko). Needs Horizon OC. |

Pick the source under **Settings → Curve sensor**, in either the Manager or the
overlay. The overlay opens a list of all six with their live readings so you can
choose directly; the Manager shows all six in its header at once, with the one
driving the fan highlighted, which is the easier place to compare them while
shaping a curve. The overlay's main screen shows only the sensor in use.

If the selected sensor stops responding, the sysmodule falls back to SoC rather
than leaving the fan unmanaged.

SoC, PCB and Skin come from first-party services and work on any setup.

**CPU / GPU / RAM need Horizon OC.** Those sensors live in the Tegra SOC_THERM
block behind MMIO that a homebrew application cannot map — hbloader is granted
no `map_io` capability, and that holds however the app is launched (title
forwarding changes available memory, not kernel permissions). Horizon OC's
sysmodule *can* read them, so NX-FanControl asks it over IPC. Install Horizon
OC and they appear and become selectable; without it they show **N/A** and a
curve set to one of them falls back to the SoC sensor.

They read hotter than SoC and lead it under load, so a curve driven from CPU or
GPU reacts earlier — but the numbers sit on a different scale, so re-tune your
points rather than reusing SoC thresholds.

---

## Compiling

Before building, ensure you have the [**devkitPro toolchain**](https://devkitpro.org/wiki/Getting_Started) installed and properly set up.

Clone the repository (including submodules) and build:

```bash
git clone https://github.com/YOUR_USERNAME/YOUR_FORK.git --recurse-submodules
cd YOUR_FORK
./build.sh
```

Building the overlay also needs the `switch-curl`, `switch-zlib`,
`switch-mbedtls` and `switch-libjson-c` portlibs, and the Manager needs
`switch-sdl2`, `switch-sdl2_ttf` and `switch-sdl2_gfx`.

### Tests

The config, profile, preset and sensor-selection logic has host-side tests that
run on your PC with a normal `gcc` (not devkitPro):

```bash
./tests/run.sh
```

---

## Common Issues & Fixes

### Fan always stays on

**Background:** the Switch has its own fan controller, Nintendo's `tc`
sysmodule, and it keeps running alongside NX-FanControl. NX-FanControl updates
the fan far more often, so it normally wins, but `tc` still applies its own
curve underneath.

**Suggested fix:** change `tc`'s curve through Atmosphère by adding these lines
to `atmosphere/config/system_settings.ini`, then reboot:

```ini
[tc]
use_configurations_on_fwdbg=u8!0x1
tskin_rate_table_console_on_fwdbg=str!"[[-1000000, 40000, 0, 0], [36000, 43000, 51, 51], [43000, 49000, 51, 128], [49000, 54000, 128, 255], [54000, 1000000, 255, 255]]"
tskin_rate_table_handheld_on_fwdbg=str!"[[-1000000, 40000, 0, 0], [36000, 43000, 51, 51], [43000, 49000, 51, 128], [49000, 54000, 128, 255], [54000, 1000000, 255, 255]]"
holdable_tskin=u32!0xEA60
touchable_tskin=u32!0xEA60
```

Before you do:

- **Add these lines, don't replace the file.** Atmosphère reads only one
  `system_settings.ini`. If you already have one, copying a new file over it
  deletes every setting you had.
- **Know what it changes.** It makes `tc`'s own curve more aggressive (100% fan
  at about 54°C skin temperature, where stock handheld stops at 60%), and raises
  the skin temperature limits Nintendo uses for heat protection to 60°C.

These lines come from the `[tc]` section of
[Dominatorul's Easy-Setup `system_settings.ini`](https://github.com/dominatorul/Easy-Setup/blob/main/data/Optimizer/EmuNAND/system_settings.ini).
That full file also changes about 80 unrelated system settings (telemetry,
background downloads, cloud saves, USB 3.0 and more), so only use the whole
file if you want all of those.

---

## Credits
* **Zathawo** - Upstream
* **Dominatorul** - Original fork
* **CtCaer** - TMP451 temperature driver (GPLv2)
* **ppkantorski** - libultrahand / Tesla overlay library (GPLv2)
* **CompuPhase** - minIni (Apache 2.0)
* **Souldbminer, Lightos_ and Horizon OC contributors** - clock-manager IPC client, used for the CPU/GPU/RAM die temperatures (GPLv2)

## License

NX-FanControl as a whole is distributed under the **GNU General Public License,
version 2** — see [LICENSE](./LICENSE). It links GPLv2 components (the
libultrahand overlay library, CtCaer's TMP451 driver and the Horizon OC IPC
client), which requires the
combined work to carry the same licence.

The original authors' code remains available under the **MIT License**, which is
preserved unchanged in [LICENSE.MIT](./LICENSE.MIT).

[COPYING.md](./COPYING.md) lists every component and its licence, including the
Apache 2.0 linking exception that covers minIni.
