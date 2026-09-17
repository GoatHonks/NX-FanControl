#include <fancontrol.hpp>

u64 GetRunningTitleId(void) {
    /* pm:dmnt gives the foreground application's process id; pm:info turns
     * that into a program id. Both services are opened per call so this works
     * whether or not the host process already holds them. */
    const bool dmntOpened = R_SUCCEEDED(pmdmntInitialize());
    if (!dmntOpened) {
        return 0;
    }
    ON_SCOPE_EXIT { pmdmntExit(); };

    u64 pid = 0;
    if (R_FAILED(pmdmntGetApplicationProcessId(&pid)) || pid == 0) {
        return 0;
    }

    const bool infoOpened = R_SUCCEEDED(pminfoInitialize());
    if (!infoOpened) {
        return 0;
    }
    ON_SCOPE_EXIT { pminfoExit(); };

    u64 programId = 0;
    if (R_FAILED(pminfoGetProgramId(&programId, pid))) {
        return 0;
    }

    return programId;
}
