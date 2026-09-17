#pragma once

#include <switch.h>

/* Program id of the foreground application, or 0 when none is running
 * (HOME menu, album, and so on). Requires pm:dmnt and pm:info access. */
u64 GetRunningTitleId(void);

/* The installed title's display name, in the console's language when
 * available. Returns false (out left empty) if the name can't be read, for
 * example when the game has been archived or deleted. */
bool GetTitleName(u64 titleId, char *out, size_t outSize);

/* Formats a title id the way the UI shows it: 16 upper-case hex digits. */
static inline void FormatTitleId(u64 titleId, char *out, size_t outSize) {
    snprintf(out, outSize, "%016lX", titleId);
}
