// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "Handle.hpp"
#include "Storage.hpp"
#include "Partition.hpp"

namespace nvs {

class PartitionManager {
public:
    virtual ~PartitionManager() { }

    esp_err_t init_partition(const char *partition_label);

    esp_err_t init_custom(Partition* partition, uint32_t baseSector, uint32_t sectorCount);

#ifdef CONFIG_NVS_ENCRYPTION
    esp_err_t secure_init_partition(const char *part_name, nvs_sec_cfg_t* cfg);
#endif

    esp_err_t deinit_partition(const char *partition_label);

    Storage* lookup_storage_from_name(const String& name);

    esp_err_t open_handle(const char *part_name, const char *ns_name, nvs_open_mode_t open_mode, Handle*& handle);

    size_t open_handles_size();

protected:
    friend Handle;

    esp_err_t close_handle(Handle* handle);

    intrusive_list<Handle> nvs_handles;
    intrusive_list<nvs::Storage> nvs_storage_list;
    intrusive_list<nvs::Partition> nvs_partition_list;
};

extern PartitionManager partitionManager;

} // nvs
