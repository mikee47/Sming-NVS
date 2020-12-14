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

#include "include/Nvs/PartitionManager.hpp"
#include "partition_lookup.hpp"

#ifdef CONFIG_NVS_ENCRYPTION
#include "nvs_encrypted_partition.hpp"
#endif // CONFIG_NVS_ENCRYPTION

namespace nvs {

PartitionManager partitionManager;

esp_err_t PartitionManager::init_partition(const char *partition_label)
{
    if (strlen(partition_label) > NVS_PART_NAME_MAX_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t size;
    Storage* mStorage;

    mStorage = lookup_storage_from_name(partition_label);
    if (mStorage) {
        return ESP_OK;
    }

    assert(SPI_FLASH_SEC_SIZE != 0);

    Partition *p = nullptr;
    esp_err_t result = partition_lookup::lookup_nvs_partition(partition_label, p);

    if (result != ESP_OK) {
        goto error;
    }

    result = init_custom(p, 0, p->size() / SPI_FLASH_SEC_SIZE);
    if (result != ESP_OK) {
        goto error;
    }

    nvs_partition_list.push_back(p);

    return ESP_OK;

error:
    delete p;
    return result;
}

esp_err_t PartitionManager::init_custom(Partition* partition, uint32_t baseSector, uint32_t sectorCount)
{
    Storage* new_storage = nullptr;
    Storage* storage = lookup_storage_from_name(partition->name());
    if (storage == nullptr) {
        new_storage = new (std::nothrow) Storage(*partition);

        if (new_storage == nullptr) {
            return ESP_ERR_NO_MEM;
        }

        storage = new_storage;
    } else {
        // if storage was initialized already, we don't need partition and hence delete it
        for (auto it = nvs_partition_list.begin(); it != nvs_partition_list.end(); ++it) {
            if (partition == it) {
                nvs_partition_list.erase(it);
                delete partition;
                break;
            }
        }
    }

    esp_err_t err = storage->init(baseSector, sectorCount);
    if (new_storage != nullptr) {
        if (err == ESP_OK) {
            nvs_storage_list.push_back(new_storage);
        } else {
            delete new_storage;
        }
    }
    return err;
}

#ifdef CONFIG_NVS_ENCRYPTION
esp_err_t PartitionManager::secure_init_partition(const char *part_name, nvs_sec_cfg_t* cfg)
{
    if (strlen(part_name) > NVS_PART_NAME_MAX_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    Storage* mStorage;

    mStorage = lookup_storage_from_name(part_name);
    if (mStorage != nullptr) {
        return ESP_OK;
    }

    Partition *p;
    esp_err_t result;
    if (cfg != nullptr) {
        result = partition_lookup::lookup_nvs_encrypted_partition(part_name, cfg, p);
    } else {
        result = partition_lookup::lookup_nvs_partition(part_name, &p);
    }

    if (result != ESP_OK) {
        return result;
    }

    uint32_t size = p->size();

    result = init_custom(p, 0, size / SPI_FLASH_SEC_SIZE);
    if (result != ESP_OK) {
        delete p;
        return result;
    }

    nvs_partition_list.push_back(p);

    return ESP_OK;
}
#endif // CONFIG_NVS_ENCRYPTION

esp_err_t PartitionManager::deinit_partition(const char *partition_label)
{
    Storage* storage = lookup_storage_from_name(partition_label);
    if (!storage) {
        return ESP_ERR_NVS_NOT_INITIALIZED;
    }

    /* Clean up handles related to the storage being deinitialized */
    for (auto it = nvs_handles.begin(); it != nvs_handles.end(); ++it) {
        if (it->mStoragePtr == storage) {
            it->valid = false;
            nvs_handles.erase(it);
        }
    }

    /* Finally delete the storage and its partition */
    nvs_storage_list.erase(storage);
    delete storage;

    for (auto it = nvs_partition_list.begin(); it != nvs_partition_list.end(); ++it) {
        if (it->name() == partition_label) {
            Partition *p = it;
            nvs_partition_list.erase(it);
            delete p;
            break;
        }
    }

    return ESP_OK;
}

esp_err_t PartitionManager::open_handle(const char *part_name,
        const char *ns_name,
        nvs_open_mode_t open_mode,
        Handle*& handle)
{
	handle = nullptr;

    if (nvs_storage_list.empty()) {
        return ESP_ERR_NVS_NOT_INITIALIZED;
    }

    Storage* sHandle = lookup_storage_from_name(part_name);
    if (sHandle == nullptr) {
        return ESP_ERR_NVS_PART_NOT_FOUND;
    }

    uint8_t nsIndex;
    esp_err_t err = sHandle->createOrOpenNamespace(ns_name, open_mode == NVS_READWRITE, nsIndex);
    if (err != ESP_OK) {
        return err;
    }

    handle = new (std::nothrow) Handle(open_mode==NVS_READONLY, nsIndex, sHandle);

    if (handle == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    nvs_handles.push_back(handle);

    return ESP_OK;
}

esp_err_t PartitionManager::close_handle(Handle* handle) {
    for (auto it = nvs_handles.begin(); it != nvs_handles.end(); ++it) {
        if (it == intrusive_list<Handle>::iterator(handle)) {
            nvs_handles.erase(it);
            return ESP_OK;
        }
    }

    return ESP_ERR_NVS_INVALID_HANDLE;
}

size_t PartitionManager::open_handles_size()
{
    return nvs_handles.size();
}

Storage* PartitionManager::lookup_storage_from_name(const String& name)
{
    auto it = find_if(begin(nvs_storage_list), end(nvs_storage_list), [=](Storage& e) -> bool {
        return e.partition().name() == name;
    });

    if (it == end(nvs_storage_list)) {
        return nullptr;
    }
    return it;
}

} // nvs
