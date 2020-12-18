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

#include "include/Nvs/PartitionManager.hpp"
#include <Storage/PartitionTable.h>

namespace nvs
{
PartitionManager partitionManager;

Storage::Partition PartitionManager::findPartition(const String& name)
{
	auto part = partitionTable.find(name);
	if(!part) {
		mLastError = ESP_ERR_NVS_PART_NOT_FOUND;
	} else if(!part.verify(Storage::Partition::SubType::Data::nvs)) {
		mLastError = ESP_ERR_NOT_SUPPORTED;
		part = Storage::Partition{};
	} else if(part.isEncrypted()) {
		mLastError = ESP_ERR_NVS_WRONG_ENCRYPTION;
		part = Storage::Partition{};
	} else {
		mLastError = ESP_OK;
	}

	return part;
}

PartitionPtr PartitionManager::lookupPartition(const String& name)
{
	auto part = findPartition(name);
	if(!part) {
		return nullptr;
	}

	PartitionPtr partition(new(std::nothrow) Partition(part));
	mLastError = partition ? ESP_OK : ESP_ERR_NO_MEM;
	return partition;
}

#ifdef ENABLE_NVS_ENCRYPTION
PartitionPtr PartitionManager::lookupPartition(const String& name, const EncryptionKey& cfg)
{
	auto part = findPartition(name);
	if(!part) {
		return nullptr;
	}

	auto enc_p = new(std::nothrow) EncryptedPartition(part);

	if(enc_p == nullptr) {
		mLastError = ESP_ERR_NO_MEM;
	} else {
		mLastError = enc_p->init(cfg);
		if(mLastError != ESP_OK) {
			delete enc_p;
			enc_p = nullptr;
		}
	}

	return PartitionPtr(enc_p);
}

#endif // ENABLE_NVS_ENCRYPTION

bool PartitionManager::initPartition(const String& name)
{
	auto container = lookupContainer(name);
	if(container != nullptr) {
		// Already initialised
		return true;
	}
	if(mLastError == ESP_ERR_INVALID_ARG) {
		return false;
	}

	static_assert(SPI_FLASH_SEC_SIZE != 0, "Invalid SPI_FLASH_SEC_SIZE");

	auto p = lookupPartition(name);
	if(!p) {
		return false;
	}

	return initPartition(p);
}

bool PartitionManager::initPartition(PartitionPtr& partition)
{
	if(!partition) {
		mLastError = ESP_ERR_INVALID_ARG;
		return false;
	}

	auto container = lookupContainer(partition->name());
	if(container == nullptr) {
		container = new(std::nothrow) Container(partition);

		if(container == nullptr) {
			mLastError = ESP_ERR_NO_MEM;
			return false;
		}

		if(container->init()) {
			container_list.push_back(container);
			mLastError = ESP_OK;
			return true;
		}

		mLastError = container->lastError();
		delete container;
		return false;
	}

	// Container was already initialized, don't need partition copy
	partition.reset();

	if(container->init()) {
		mLastError = ESP_OK;
		return true;
	}

	mLastError = container->lastError();
	return false;
}

#ifdef ENABLE_NVS_ENCRYPTION
bool PartitionManager::secureInitPartition(const String& name, const EncryptionKey* cfg)
{
	auto container = lookupContainer(name);
	if(container != nullptr) {
		return true;
	}
	if(mLastError == ESP_ERR_INVALID_ARG) {
		return false;
	}

	auto p = cfg ? lookupEncryptedPartition(name, *cfg) : lookupPartition(name);
	if(!p) {
		return false;
	}

	return initPartition(p);
}
#endif // ENABLE_NVS_ENCRYPTION

bool PartitionManager::deinitPartition(const String& name)
{
	Container* container = lookupContainer(name);
	if(!container) {
		return true;
	}

	if(!container->checkNoHandlesInUse()) {
		mLastError = container->lastError();
		return false;
	}

	container_list.erase(container);
	delete container;
	mLastError = ESP_OK;
	return true;
}

HandlePtr PartitionManager::openHandle(const String& partName, const String& nsName, OpenMode openMode)
{
	auto container = lookupContainer(partName);
	if(container == nullptr) {
		return nullptr;
	}

	auto handle = container->openHandle(nsName, openMode);
	mLastError = container->lastError();
	return handle;
}

Container* PartitionManager::lookupContainer(const String& partName)
{
	if(partName.length() > Storage::Partition::nameSize) {
		mLastError = ESP_ERR_INVALID_ARG;
		return nullptr;
	}

	auto it = find(begin(container_list), end(container_list), partName);
	mLastError = it ? ESP_OK : ESP_ERR_NVS_NOT_INITIALIZED;
	return static_cast<Container*>(it);
}

} // namespace nvs
