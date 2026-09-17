#include <fancontrol.hpp>

/* The vendored Horizon OC client is C and its headers carry no linkage
 * guards, so the declarations have to be wrapped here. */
extern "C" {
#include <hocclk/client/ipc.h>
}

namespace {

    /* Horizon OC's context carries every reading in one struct, so it is
     * fetched once and reused for a short window rather than dispatching an
     * IPC call per sensor per frame. */
    constexpr u64 HocCacheNs = 250'000'000ULL;

    /* How long to wait before trying Horizon OC again after a failure. The
     * sysmodule runs for weeks across many sleep cycles, so a one-shot attempt
     * would mean a session that breaks once never comes back, and a Horizon OC
     * that starts after us is never noticed. */
    constexpr u64 HocRetryNs = 10'000'000'000ULL;

    bool          g_hocOpen     = false;
    HocClkContext g_hocContext  = {};
    bool          g_hocValid    = false;
    u64           g_hocFetched  = 0;
    u64           g_hocLastTry  = 0;
    bool          g_hocTried    = false;

    /* Ticks elapsed, treating a counter that has gone backwards as "long ago"
     * rather than wrapping into a huge value. */
    u64 ElapsedNs(u64 now, u64 since) {
        return now >= since ? armTicksToNs(now - since) : UINT64_MAX;
    }

    void CloseHoc(void) {
        if (g_hocOpen) {
            hocclkIpcExit();
            g_hocOpen = false;
        }
        g_hocValid = false;
    }

    bool EnsureHocOpen(u64 now) {
        if (g_hocOpen) {
            return true;
        }

        /* Back off between attempts so a missing Horizon OC does not mean an
         * IPC connect on every single read. */
        if (g_hocTried && ElapsedNs(now, g_hocLastTry) < HocRetryNs) {
            return false;
        }

        g_hocTried   = true;
        g_hocLastTry = now;
        g_hocOpen    = R_SUCCEEDED(hocclkIpcInitialize());
        return g_hocOpen;
    }

    bool RefreshHocContext(void) {
        const u64 now = armGetSystemTick();

        if (!EnsureHocOpen(now)) {
            return false;
        }

        if (g_hocValid && ElapsedNs(now, g_hocFetched) < HocCacheNs) {
            return true;
        }

        if (R_SUCCEEDED(hocclkIpcGetCurrentContext(&g_hocContext))) {
            g_hocValid   = true;
            g_hocFetched = now;
            return true;
        }

        /* The session is no longer answering - drop it so the next attempt
         * reconnects instead of failing forever. */
        CloseHoc();
        return false;
    }

    bool ReadHocTemperature(HocClkThermalSensor which, float *outCelsius) {
        if (!RefreshHocContext()) {
            return false;
        }

        const s32 milliC = g_hocContext.temps[which];
        /* Horizon OC reports 0 for a sensor it could not read. */
        if (milliC == 0) {
            return false;
        }

        *outCelsius = static_cast<float>(milliC) / 1000.0f;
        return true;
    }

    bool ReadSkinTemperature(float *outCelsius) {
        /* tc is opened per call so this works from the sysmodule, the overlay
         * and the manager without any of them owning the session. */
        if (R_FAILED(tcInitialize())) {
            return false;
        }
        ON_SCOPE_EXIT { tcExit(); };

        s32 milliC = 0;
        if (R_FAILED(tcGetSkinTemperatureMilliC(&milliC))) {
            return false;
        }

        *outCelsius = static_cast<float>(milliC) / 1000.0f;
        return true;
    }

}

const char *GetSensorName(FanSensor sensor) {
    switch (sensor) {
        case FanSensor_Pcb:  return "PCB";
        case FanSensor_Skin: return "Skin";
        case FanSensor_Cpu:  return "CPU";
        case FanSensor_Gpu:  return "GPU";
        case FanSensor_Ram:  return "RAM";
        default:             return "SoC";
    }
}

const char *GetSensorDescription(FanSensor sensor) {
    switch (sensor) {
        case FanSensor_Pcb:  return "TMP451 local channel, near the board";
        case FanSensor_Skin: return "Console exterior, what Nintendo's curve uses";
        case FanSensor_Cpu:  return "CPU die temperature - needs Horizon OC";
        case FanSensor_Gpu:  return "GPU die temperature - needs Horizon OC";
        case FanSensor_Ram:  return "RAM die temperature - needs Horizon OC";
        default:             return "TMP451 remote diode, hottest and most responsive";
    }
}

bool SensorNeedsHorizonOc(FanSensor sensor) {
    return sensor == FanSensor_Cpu || sensor == FanSensor_Gpu || sensor == FanSensor_Ram;
}

bool IsHorizonOcAvailable(void) {
    return RefreshHocContext();
}

bool ReadSensor(FanSensor sensor, float *outCelsius) {
    if (outCelsius == NULL) {
        return false;
    }

    switch (sensor) {
        case FanSensor_Pcb:
            return R_SUCCEEDED(Tmp451GetPcbTemp(outCelsius));
        case FanSensor_Skin:
            return ReadSkinTemperature(outCelsius);
        case FanSensor_Cpu:
            return ReadHocTemperature(HocClkThermalSensor_CPU, outCelsius);
        case FanSensor_Gpu:
            return ReadHocTemperature(HocClkThermalSensor_GPU, outCelsius);
        case FanSensor_Ram:
            return ReadHocTemperature(HocClkThermalSensor_MEM, outCelsius);
        default:
            return R_SUCCEEDED(Tmp451GetSocTemp(outCelsius));
    }
}

float ReadSensorOrNegative(FanSensor sensor) {
    float value = 0.0f;
    return ReadSensor(sensor, &value) ? value : -1.0f;
}
