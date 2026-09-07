// VL53L0X distance for a hand-wave control. Optional. Minimal init after
// ST's reference sequence, continuous back-to-back ranging.
#pragma once
#include <stdbool.h>
#include "driver/i2c_master.h"
bool tof_init(i2c_master_bus_handle_t bus);
bool tof_present(void);
int  tof_poll(void);     // mm, -1 when nothing new or out of range
