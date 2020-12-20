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
/**
 * @brief Maintains list of all open storage containers.
 */
class PartitionManager
{
public:
	/**
	 * @brief Locate named partition and verify it's a data/nvs type.
	 */
	Storage::Partition findPartition(const String& name);

	/**
	 * @name Create NVS partition object for given storage partition
	 * @{
	 */
	PartitionPtr lookupPartition(const String& name);
#ifdef ENABLE_NVS_ENCRYPTION
	PartitionPtr lookupPartition(const String& name, const EncryptionKey& cfg);
#endif
	/** @} */

	/**
	 * @name Open storage container
	 * @{
	 */
	bool openContainer(const String& name);

	bool openContainer(PartitionPtr& partition);

#ifdef ENABLE_NVS_ENCRYPTION
	bool openContainer(const String& name, const EncryptionKey* cfg);
#endif
	/** @} */

	/**
	 * @brief Close a storage container
	 * @param name Name of partition
	 * @retval bool true on success, false if partition doesn't exist or has open handles
	 */
	bool closeContainer(const String& name);

	/**
	 * @brief Get container for given partition
	 * @param name Name of partition
	 * @retval Container* Object owned by PartitionManager
	 */
	Container* lookupContainer(const String& name);

	/**
	 * @brief Open a storage handle for a specified partition/namespace
	 * @param partName Name of partition
	 * @param nsName Namespace
	 * @param openMode
	 * @retval HandlePtr
	 */
	HandlePtr openHandle(const String& partName, const String& nsName, OpenMode openMode);

	/**
	 * @brief Fetch error code for last operation
	 */
	esp_err_t lastError() const
	{
		return mLastError;
	}

protected:
	intrusive_list<nvs::Container> container_list;
	esp_err_t mLastError{ESP_OK};
};

extern PartitionManager partitionManager;

/**
 * @brief Creating Handle object for a specified partition/namespace
 * @param partName Name of partition
 * @param nsName Namespace
 * @param openMode
 * @retval HandlePtr
 */
inline HandlePtr openHandle(const String& partName, const String& nsName, OpenMode openMode)
{
	return partitionManager.openHandle(partName, nsName, openMode);
}

/**
 * @brief Open and register storage container for given partition
 * @param name Name of partition
 * @retval bool true on success, false if, for example, partition doesn't exist
 *
 * Partition manager tracks all open containers.
 */
inline bool openContainer(const String& name)
{
	return partitionManager.openContainer(name);
}

/**
 * @brief Close a container
 * @param name Name of partition
 * @retval bool true on success, false if, for example, there are open handles on the container
 */
inline bool closeContainer(const String& name)
{
	return partitionManager.closeContainer(name);
}

} // namespace nvs
