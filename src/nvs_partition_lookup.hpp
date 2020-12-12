#include "esp_err.h"
#include "nvs_partition.hpp"

#ifndef NVS_PARTITION_LOOKUP_HPP_
#define NVS_PARTITION_LOOKUP_HPP_

namespace nvs {

namespace partition_lookup {

esp_err_t lookup_nvs_partition(const char* label, NVSPartition *&p);

#ifdef CONFIG_NVS_ENCRYPTION
esp_err_t lookup_nvs_encrypted_partition(const char* label, nvs_sec_cfg_t* cfg, NVSPartition *&p);
#endif // CONFIG_NVS_ENCRYPTION

} // partition_lookup

} // nvs

#endif // NVS_PARTITION_LOOKUP_HPP_
