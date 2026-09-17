# Changelog

All changes in this fork were written by Claude Code (Anthropic's AI coding
assistant). The fork's maintainer directed the work through prompts and tested
every change on a Switch Lite; they are not a developer. See the README.

## v1.2.0

Compared with upstream NX-FanControl v1.1.2.

### Added

**Profiles**
- Named fan profiles, up to 8. Each one has its own handheld **and** docked
  curve, so switching profiles swaps both together.
- Create, rename, reorder and delete profiles. Renaming uses the Switch's own
  keyboard, in the Manager app.
- Curve presets when creating a profile: a copy of an existing profile,
  **Default**, or **Stock-like** (an approximation of Nintendo's stock fan
  behaviour).
- Existing configs are upgraded automatically.

**Per-game profiles**
- Assign a profile to a game from the overlay. When that game starts, the
  active profile switches to it; when the game closes, the previous profile is
  restored.
- Picking a different profile by hand while the game runs is respected and
  kept when the game closes.
- The profile to restore is saved, so a reboot or crash mid-game still
  restores it.
- Assigning, changing or clearing a game while it's running takes effect
  straight away.
- Games are shown by name, with their title ID.

**Temperature sensors**
- Choose which sensor drives the fan curve: **SoC**, **PCB** or **Skin** (all
  built into the console), or **CPU**, **GPU** or **RAM** die temperature (needs
  Horizon OC).
- If the selected sensor stops responding, the fan falls back to SoC instead of
  being left unmanaged.

**NXFanControl Manager (new app)**
- A full homebrew app with a live fan curve graph, and tabs for Profiles, Fan
  Curve, Games and Settings.
- Shows all six temperatures, fan speed and dock state live, with the sensor
  driving the curve highlighted.
- Shows the console model, and whether the sysmodule and Horizon OC are running.

**Overlay**
- Profile screen: switch, add and delete profiles.
- Per-Game Profiles screen: turn the feature on or off and assign the running
  game to a profile.
- Curve Sensor picker in Settings, listing every sensor with its live reading.
- The main screen shows the temperature of the sensor the curve is using.
- Deleting a profile needs a hold, so it can't happen by accident.

**Project**
- Host-side test suite (500 checks) for the config, profile, preset, per-game
  and sensor-selection logic: `tests/run.sh`.

### Changed

- The **Default** profile is seeded with the Stock-like curves the first time the
  new version runs. It stays fully editable, and your edits are never reset.
- All displayed temperatures and percentages round to the nearest whole number
  (39.6 °C shows as 40 °C), so every screen agrees with every other.
- The "Fan always stays on" advice in the README now lists only the fan-related
  Atmosphère settings, and warns not to overwrite an existing
  `system_settings.ini`.
- `build.sh` also builds minIni and the Manager, and stops at the first error
  instead of packaging stale files.
- Version numbers are consistent (1.2.0), and the overlay reads its version from
  the build.
- Licensing: the combined work is distributed under GPLv2, as its GPL
  components require. The original MIT grant is preserved in `LICENSE.MIT`, and
  `COPYING.md` lists every component and its licence.

### Fixed

These issues were present in upstream v1.1.2:

- **Config changes could be missed.** Settings were only reloaded when the
  config file's timestamp changed, and FAT32 stores timestamps in 2-second
  steps, so a second quick change could be ignored. Every save now also bumps a
  revision number.
- **An interrupted save could lose the whole config.** Losing power between
  removing the old config and writing the new one left nothing in place. The
  interrupted save is now completed on the next save or start-up.
- **`build.sh` could report success after a failed build.**

### Notes

- After updating, **reboot the console**. The sysmodule only loads at boot, so
  copying new files doesn't replace the copy already running.
- CPU, GPU and RAM temperatures need Horizon OC installed and running. Without
  it they show N/A, and a curve set to one of them uses SoC.
