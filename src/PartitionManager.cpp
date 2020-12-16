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

::Storage::Partition PartitionManager::findPartition(const String& name)
{
	auto part = ::Storage::PartitionTable().find(name);
	if(!part) {
		mLastError = ESP_ERR_NOT_FOUND;
	} else if(!part.verify(::Storage::Partition::SubType::Data::nvs)) {
		mLastError = ESP_ERR_NOT_SUPPORTED;
		part = ::Storage::Partition{};
	} else if(part.isEncrypted()) {
		mLastError = ESP_ERR_NVS_WRONG_ENCRYPTION;
		part = ::Storage::Partition{};
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
PartitionPtr PartitionManager::lookupEncryptedPartition(const String& name, const nvs_sec_cfg_t& cfg)
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
	auto storage = lookupStorage(name);
	if(storage != nullptr) {
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

	auto storage = lookupStorage(partition->name());
	if(storage == nullptr) {
		storage = new(std::nothrow) Storage(partition);

		if(storage == nullptr) {
			mLastError = ESP_ERR_NO_MEM;
			return false;
		}

		if(storage->init()) {
			storage_list.push_back(storage);
			mLastError = ESP_OK;
			return true;
		}

		mLastError = storage->lastError();
		delete storage;
		return false;
	}

	// Storage was already initialized, don't need partition copy
	partition.reset();

	if(storage->init()) {
		mLastError = ESP_OK;
		return true;
	}

	mLastError = storage->lastError();
	return false;
}

#ifdef ENABLE_NVS_ENCRYPTION
bool PartitionManager::secureInitPartition(const String& name, const nvs_sec_cfg_t* cfg)
{
	auto storage = lookupStorage(name);
	if(storage != nullptr) {
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

bool PartitionManager::deinitPartition(const String& partition_label)
{
	Storage* storage = lookupStorage(partition_label);
	if(!storage) {
		return true;
	}

	if(!storage->checkNoHandlesInUse()) {
		mLastError = storage->lastError();
		return false;
	}

	storage_list.erase(storage);
	delete storage;
	mLastError = ESP_OK;
	return true;
}

HandlePtr PartitionManager::openHandle(const String& partName, const String& nsName, OpenMode openMode)
{
	auto storage = lookupStorage(partName);
	if(storage == nullptr) {
		return nullptr;
	}

	auto handle = storage->openHandle(nsName, openMode);
	mLastError = storage->lastError();
	return handle;
}

Storage* PartitionManager::lookupStorage(const String& partName)
{
	if(partName.length() > ::Storage::Partition::nameSize) {
		mLastError = ESP_ERR_INVALID_ARG;
		return nullptr;
	}

	if(storage_list.empty()) {
		mLastError = ESP_ERR_NVS_NOT_INITIALIZED;
		return nullptr;
	}

	auto it = find(begin(storage_list), end(storage_list), partName);
	mLastError = it ? ESP_OK : ESP_ERR_NOT_FOUND;
	return static_cast<Storage*>(it);
}

} // namespace nvs
