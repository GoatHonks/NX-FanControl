#pragma once

#include <switch.h>

/* Temperature sources the fan curve can be driven from.
 *
 * SoC and PCB are the two channels of the TMP451, a physical I2C sensor.
 * Skin is Nintendo's own computed exterior-temperature estimate, read from
 * the tc sysmodule; it is what the stock fan curve is indexed on, and it runs
 * much cooler than the SoC reading. All three come from first-party services
 * and work on any setup.
 *
 * CPU, GPU and RAM are the Tegra SOC_THERM die sensors. They read hotter than
 * SoC because they measure junction temperature inside the silicon, and they
 * lead it under load. They sit behind MMIO that homebrew cannot map, so they
 * are read over IPC from Horizon OC's sysmodule and are only available when
 * that is installed and running.
 */
enum FanSensor {
    FanSensor_Soc = 0,
    FanSensor_Pcb,
    FanSensor_Skin,
    FanSensor_Cpu,
    FanSensor_Gpu,
    FanSensor_Ram,
    FanSensor_Count,
};

const char *GetSensorName(FanSensor sensor);
const char *GetSensorDescription(FanSensor sensor);

/* True for sensors that depend on Horizon OC being present. */
bool SensorNeedsHorizonOc(FanSensor sensor);

/* Whether Horizon OC's IPC service is reachable right now. */
bool IsHorizonOcAvailable(void);

/* Reads a sensor in degrees Celsius. Returns false when unavailable. */
bool ReadSensor(FanSensor sensor, float *outCelsius);

/* Convenience for UI: returns the temperature or -1.0f when unavailable. */
float ReadSensorOrNegative(FanSensor sensor);
