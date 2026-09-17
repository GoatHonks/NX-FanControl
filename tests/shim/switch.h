/* Minimal host-side stand-in for libnx's <switch.h>, so the portable parts of
 * the config and sensor layers can be unit-tested off-device. */
#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
#include <cstdint>
#endif

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

typedef u32 Result;
typedef int I2cDevice;

#define R_SUCCEEDED(res) ((res) == 0)
#define R_FAILED(res)    ((res) != 0)

enum { I2cDevice_Tmp451 = 0 };

typedef enum {
    SetSysProductModel_Invalid = 0,
    SetSysProductModel_Nx      = 1,
    SetSysProductModel_Copper  = 2,
    SetSysProductModel_Iowa    = 3,
    SetSysProductModel_Hoag    = 4,
    SetSysProductModel_Calcio  = 5,
    SetSysProductModel_Aula    = 6,
} SetSysProductModel;

#ifdef __cplusplus
extern "C" {
#endif

/* tc (thermal control) service, stubbed by the test harness. */
Result tcInitialize(void);
void   tcExit(void);
Result tcGetSkinTemperatureMilliC(s32 *skinTemp);

/* Tick helpers, stubbed by the test harness. */
u64 armGetSystemTick(void);
u64 armGetSystemTickFreq(void);
u64 armTicksToNs(u64 ticks);

#ifdef __cplusplus
}
#endif
