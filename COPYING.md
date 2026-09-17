# Licensing

## Summary

**NX-FanControl as a whole is distributed under the GNU General Public License,
version 2** — see [LICENSE](./LICENSE).

It has to be. The built binaries link against components that are themselves
GPLv2 (the libultrahand/Tesla overlay library and CtCaer's TMP451 temperature
driver), and GPLv2 requires the combined work to be distributed under the same
terms.

The original NX-FanControl code was published under the MIT licence, and that
grant is unchanged and preserved in [LICENSE.MIT](./LICENSE.MIT). MIT permits
inclusion in a GPL work, so relicensing the *combined* distribution as GPLv2 is
both permitted and required here. Anyone who wants the original authors' code
under MIT can still take it under those terms.

## Components and their licences

| Component | Licence | Notes |
|---|---|---|
| NX-FanControl original code | MIT | Zathawo, Dominatorul. See [LICENSE.MIT](./LICENSE.MIT). |
| `common/include/fancontrol/tmp451.hpp` | GPLv2 | SoC/PCB temperature driver, © 2018 CTCaer. |
| `overlay/lib/libultrahand` (submodule) | GPLv2 | Tesla overlay library, ppkantorski. |
| `common/libs/minIni` | Apache 2.0, with a linking exception | © CompuPhase. See below. |
| `common/libs/minIni/include/minIni.h` | GPLv2 + Beer-Ware | 37-line wrapper header added by the sys-clk / Horizon OC authors. |
| `common/libs/hocclk` | GPLv2 + Beer-Ware | Horizon OC clock-manager IPC client, used to read the CPU/GPU/RAM die temperatures. See its [NOTICE](./common/libs/hocclk/NOTICE). |

## A note on minIni

minIni is Apache 2.0, which is *not* generally considered compatible with
GPLv2. minIni ships an explicit exception that resolves this for binaries:

> As a special exception to the Apache License 2.0 ... you may link, statically
> or dynamically, the "Work" to other modules to produce an executable file
> containing portions of the "Work", and distribute that executable file in
> "Object" form under the terms of your choice ... This exception applies only
> to redistributions in "Object" form (not "Source" form) and only if no
> modifications have been made to the "Work".

So the compiled `.nsp` / `.ovl` / `.nro` may be distributed under GPLv2, and
minIni's own source stays under Apache 2.0 in `common/libs/minIni/` with its
LICENSE file intact. Do not modify the files under `common/libs/minIni/dev/` —
the exception only holds while the library is unmodified.

## If you contribute or fork

Keep every existing copyright and licence notice where it is. Third-party code
lives under `common/libs/` and `overlay/lib/` and keeps its own terms; treat
those directories as read-only.
