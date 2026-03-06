#pragma once

#include <stdbool.h>

#include "esp_err.h"

esp_err_t carmood_time_sync_once(void);
bool carmood_time_sync_is_synced(void);
