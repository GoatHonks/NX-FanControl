#pragma once

#include <switch.h>
#include "pwm.h"

#define SysFanControlID 0x00FF0000B378D640

/* Returns the sysmodule's process id, or 0 when it is not running. */
u64 IsRunning();

bool InitializeSensors();
float GetSOCTemperature();
float GetFanSpeed();
void CloseSensors();
