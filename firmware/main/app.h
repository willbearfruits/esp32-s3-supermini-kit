// The two applications this firmware can be built as. app_main() does the
// common bring-up and then hands over to one of these; they never return.
#pragma once
#include "driver/i2c_master.h"
void app_instrument_run(void);
void app_looper_run(i2c_master_bus_handle_t bus);
void app_test_run(i2c_master_bus_handle_t bus);
void app_jam_run(i2c_master_bus_handle_t bus);
