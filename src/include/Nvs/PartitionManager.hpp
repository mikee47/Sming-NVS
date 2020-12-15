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
#ifdef ENABLE_NVS_ENCRYPTION
#include "EncryptedPartition.hpp"
#endif

namespace nvs
{
class PartitionManager
{
public:
	::Storage::Partition find_partition(const char* label);

	PartitionPtr lookup_partition(const char* label);
#ifdef ENABLE_NVS_ENCRYPTION
	PartitionPtr lookup_encrypted_partition(const char* label, const nvs_sec_cfg_t& cfg);
#endif

	bool init_partition(const char* partition_label = NVS_DEFAULT_PART_NAME);

	bool init_custom(PartitionPtr& partition, uint32_t baseSector, uint32_t sectorCount);

#ifdef ENABLE_NVS_ENCRYPTION
	bool secure_init_partition(const char* part_name, const nvs_sec_cfg_t* cfg);
#endif

	bool deinit_partition(const char* partition_label = NVS_DEFAULT_PART_NAME);

	Storage* lookup_storage(const String& part_name);

	HandlePtr open(const char* part_name, const char* ns_name, OpenMode open_mode);

	HandlePtr open(const char* ns_name, OpenMode open_mode)
	{
		return open(NVS_DEFAULT_PART_NAME, ns_name, open_mode);
	}

	esp_err_t lastError() const
	{
		return mLastError;
	}

protected:
	intrusive_list<nvs::Storage> storage_list;
	esp_err_t mLastError{ESP_OK};
};

extern PartitionManager partitionManager;

} // namespace nvs
