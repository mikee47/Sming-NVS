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

namespace nvs
{
class PartitionManager
{
public:
	Partition* lookup_partition(const char* label);
#ifdef CONFIG_NVS_ENCRYPTION
	Partition* lookup_encrypted_partition(const char* label, const nvs_sec_cfg_t& cfg);
#endif

	bool init_partition(const char* partition_label);

	bool init_custom(Partition* partition, uint32_t baseSector, uint32_t sectorCount);

#ifdef CONFIG_NVS_ENCRYPTION
	esp_err_t secure_init_partition(const char* part_name, const nvs_sec_cfg_t& cfg);
#endif

	bool deinit_partition(const char* partition_label);

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
	friend Handle;
	friend Partition;

	::Storage::Partition find_partition(const char* label);

	// Called from Partition destructor
	void remove_partition(Partition* partition);

	intrusive_list<nvs::Storage> storage_list;
	intrusive_list<nvs::Partition> partition_list;
	esp_err_t mLastError{ESP_OK};
};

extern PartitionManager partitionManager;

} // namespace nvs
