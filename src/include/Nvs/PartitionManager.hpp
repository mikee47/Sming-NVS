// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "Handle.hpp"
#include "Container.hpp"
#include "Partition.hpp"
#ifdef ENABLE_NVS_ENCRYPTION
#include "EncryptedPartition.hpp"
#endif

namespace nvs
{
class PartitionManager
{
public:
	Storage::Partition findPartition(const String& name);

	PartitionPtr lookupPartition(const String& name);
#ifdef ENABLE_NVS_ENCRYPTION
	PartitionPtr lookupPartition(const String& name, const EncryptionKey& cfg);
#endif

	bool initPartition(const String& name);

	bool initPartition(PartitionPtr& partition);

#ifdef ENABLE_NVS_ENCRYPTION
	bool secureInitPartition(const String& name, const EncryptionKey* cfg);
#endif

	bool deinitPartition(const String& name);

	Container* lookupContainer(const String& name);

	HandlePtr openHandle(const String& partName, const String& nsName, OpenMode openMode);

	esp_err_t lastError() const
	{
		return mLastError;
	}

protected:
	intrusive_list<nvs::Container> container_list;
	esp_err_t mLastError{ESP_OK};
};

extern PartitionManager partitionManager;

inline HandlePtr openHandle(const String& partName, const String& nsName, OpenMode openMode)
{
	return partitionManager.openHandle(partName, nsName, openMode);
}

inline bool initPartition(const String& name)
{
	return partitionManager.initPartition(name);
}

inline bool deinitPartition(const String& name)
{
	return partitionManager.deinitPartition(name);
}

} // namespace nvs
