// The two applications this firmware can be built as. app_main() does the
// common bring-up and then hands over to one of these; they never return.
#pragma once
void app_instrument_run(void);
void app_looper_run(void);
