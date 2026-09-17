#pragma once

#include <switch.h>

/* Program id of the foreground application, or 0 when none is running
 * (HOME menu, album, and so on). Requires pm:dmnt and pm:info access. */
u64 GetRunningTitleId(void);
