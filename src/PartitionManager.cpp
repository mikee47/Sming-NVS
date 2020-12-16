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
#include <Storage/PartitionTable.h>

namespace nvs
{
PartitionManager partitionManager;

::Storage::Partition PartitionManager::find_partition(const char* label)
{
	auto part = ::Storage::PartitionTable().find(label);
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

PartitionPtr PartitionManager::lookup_partition(const char* label)
{
	auto part = find_partition(label);
	if(!part) {
		return nullptr;
	}

	PartitionPtr partition(new(std::nothrow) Partition(part));
	mLastError = partition ? ESP_OK : ESP_ERR_NO_MEM;
	return partition;
}

#ifdef ENABLE_NVS_ENCRYPTION
PartitionPtr PartitionManager::lookup_encrypted_partition(const char* label, const nvs_sec_cfg_t& cfg)
{
	auto part = find_partition(label);
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

bool PartitionManager::init_partition(const char* partition_label)
{
	auto storage = lookup_storage(partition_label);
	if(storage != nullptr) {
		return true;
	}
	if(mLastError == ESP_ERR_INVALID_ARG) {
		return false;
	}

	static_assert(SPI_FLASH_SEC_SIZE != 0, "Invalid SPI_FLASH_SEC_SIZE");

	auto p = lookup_partition(partition_label);
	if(!p) {
		return false;
	}

	return init_partition(p);
}

bool PartitionManager::init_partition(PartitionPtr& partition)
{
	auto storage = lookup_storage(partition->name());
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
bool PartitionManager::secure_init_partition(const char* part_name, const nvs_sec_cfg_t* cfg)
{
	auto storage = lookup_storage(part_name);
	if(storage != nullptr) {
		return true;
	}
	if(mLastError == ESP_ERR_INVALID_ARG) {
		return false;
	}

	PartitionPtr p;
	if(cfg != nullptr) {
		p = lookup_encrypted_partition(part_name, *cfg);
	} else {
		p = lookup_partition(part_name);
	}
	if(!p) {
		return false;
	}

	return init_partition(p);
}
#endif // ENABLE_NVS_ENCRYPTION

bool PartitionManager::deinit_partition(const char* partition_label)
{
	Storage* storage = lookup_storage(partition_label);
	if(!storage) {
		return false;
	}

	storage_list.erase(storage);
	delete storage;
	mLastError = ESP_OK;
	return true;
}

HandlePtr PartitionManager::open(const char* part_name, const char* ns_name, OpenMode open_mode)
{
	auto storage = lookup_storage(part_name);
	if(storage == nullptr) {
		return nullptr;
	}

	auto handle = storage->open_handle(ns_name, open_mode);
	mLastError = storage->lastError();
	return handle;
}

Storage* PartitionManager::lookup_storage(const String& part_name)
{
	if(part_name.length() > ::Storage::Partition::nameSize) {
		mLastError = ESP_ERR_INVALID_ARG;
		return nullptr;
	}

	if(storage_list.empty()) {
		mLastError = ESP_ERR_NVS_NOT_INITIALIZED;
		return nullptr;
	}

	auto it = find(begin(storage_list), end(storage_list), part_name);
	mLastError = it ? ESP_OK : ESP_ERR_NOT_FOUND;
	return static_cast<Storage*>(it);
}

} // namespace nvs
