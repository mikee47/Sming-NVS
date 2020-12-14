#pragma once

#include "include/Nvs/esp_err.h"
#include "include/Nvs/Partition.hpp"

namespace nvs {

namespace partition_lookup {

esp_err_t lookup_nvs_partition(const char* label, Partition *&p);

#ifdef CONFIG_NVS_ENCRYPTION
esp_err_t lookup_nvs_encrypted_partition(const char* label, nvs_sec_cfg_t* cfg, Partition *&p);
#endif // CONFIG_NVS_ENCRYPTION

} // partition_lookup

} // nvs
