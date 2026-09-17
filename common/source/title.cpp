#include <fancontrol.hpp>

/* The sysmodule closes its service-manager session once start-up is done, and
 * every service below has to be obtained through sm. Each lookup therefore
 * opens sm for itself. sm is reference counted, so this is harmless in the
 * overlay and the manager, which already hold it. Without this the sysmodule
 * could never see the running game, and per-game profiles never switched. */

u64 GetRunningTitleId(void) {
    if (R_FAILED(smInitialize())) {
        return 0;
    }
    ON_SCOPE_EXIT { smExit(); };

    /* pm:dmnt gives the foreground application's process id; pm:info turns
     * that into a program id. */
    if (R_FAILED(pmdmntInitialize())) {
        return 0;
    }
    ON_SCOPE_EXIT { pmdmntExit(); };

    u64 pid = 0;
    if (R_FAILED(pmdmntGetApplicationProcessId(&pid)) || pid == 0) {
        return 0;
    }

    if (R_FAILED(pminfoInitialize())) {
        return 0;
    }
    ON_SCOPE_EXIT { pminfoExit(); };

    u64 programId = 0;
    if (R_FAILED(pminfoGetProgramId(&programId, pid))) {
        return 0;
    }

    return programId;
}

bool GetTitleName(u64 titleId, char *out, size_t outSize) {
    if (out == NULL || outSize == 0 || titleId == 0) {
        return false;
    }
    out[0] = 0;

    if (R_FAILED(smInitialize())) {
        return false;
    }
    ON_SCOPE_EXIT { smExit(); };

    if (R_FAILED(nsInitialize())) {
        return false;
    }
    ON_SCOPE_EXIT { nsExit(); };

    /* The control data carries the 256x256 icon, which makes it far too big
     * for the stack in an overlay. */
    NsApplicationControlData *data = static_cast<NsApplicationControlData *>(malloc(sizeof(NsApplicationControlData)));
    if (data == NULL) {
        return false;
    }
    ON_SCOPE_EXIT { free(data); };

    u64 actualSize = 0;
    if (R_FAILED(nsGetApplicationControlData(NsApplicationControlSource_Storage, titleId, data,
                                             sizeof(NsApplicationControlData), &actualSize))) {
        return false;
    }

    /* Prefer the entry for the console's language; fall back to the first
     * entry, which every title has. */
    const char *name = data->nacp.lang[0].name;
    if (R_SUCCEEDED(setInitialize())) {
        NacpLanguageEntry *entry = NULL;
        if (R_SUCCEEDED(nacpGetLanguageEntry(&data->nacp, &entry)) && entry != NULL && entry->name[0] != 0) {
            name = entry->name;
        }
        setExit();
    }

    if (name[0] == 0) {
        return false;
    }

    snprintf(out, outSize, "%s", name);
    return true;
}
