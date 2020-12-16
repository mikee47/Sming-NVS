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
	::Storage::Partition findPartition(const String& label);

	PartitionPtr lookupPartition(const String& label);
#ifdef ENABLE_NVS_ENCRYPTION
	PartitionPtr lookupEncryptedPartition(const String& label, const nvs_sec_cfg_t& cfg);
#endif

	bool initPartition(const String& partition_label = NVS_DEFAULT_PART_NAME);

	bool initPartition(PartitionPtr& partition);

#ifdef ENABLE_NVS_ENCRYPTION
	bool secure_init_partition(const String& part_name, const nvs_sec_cfg_t* cfg);
#endif

	bool deinitPartition(const String& partition_label = NVS_DEFAULT_PART_NAME);

	Storage* lookupStorage(const String& part_name);

	HandlePtr open(const String& part_name, const String& ns_name, OpenMode open_mode);

	HandlePtr open(const String& ns_name, OpenMode open_mode)
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
