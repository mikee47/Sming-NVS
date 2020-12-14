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
	if(strlen(partition_label) > NVS_PART_NAME_MAX_SIZE) {
		mLastError = ESP_ERR_INVALID_ARG;
		return false;
	}

	auto storage = lookup_storage(partition_label);
	if(storage != nullptr) {
		mLastError = ESP_OK;
		return true;
	}

	static_assert(SPI_FLASH_SEC_SIZE != 0, "Invalid SPI_FLASH_SEC_SIZE");

	auto p = lookup_partition(partition_label);
	if(!p) {
		return false;
	}

	if(!init_custom(p, 0, p->size() / SPI_FLASH_SEC_SIZE)) {
		return false;
	}

	partition_list.push_back(p.release());

	return true;
}

bool PartitionManager::init_custom(PartitionPtr& partition, uint32_t baseSector, uint32_t sectorCount)
{
	auto storage = lookup_storage(partition->name());
	if(storage == nullptr) {
		storage = new(std::nothrow) Storage(*partition);

		if(storage == nullptr) {
			mLastError = ESP_ERR_NO_MEM;
		} else {
			mLastError = storage->init(baseSector, sectorCount);
			if(mLastError == ESP_OK) {
				storage_list.push_back(storage);
			} else {
				delete storage;
			}
		}
	} else {
		// Storage was already initialized, don't need partition copy
		partition.reset();

		mLastError = storage->init(baseSector, sectorCount);
	}

	return mLastError == ESP_OK;
}

#ifdef ENABLE_NVS_ENCRYPTION
bool PartitionManager::secure_init_partition(const char* part_name, const nvs_sec_cfg_t* cfg)
{
	if(strlen(part_name) > NVS_PART_NAME_MAX_SIZE) {
		mLastError = ESP_ERR_INVALID_ARG;
		return false;
	}

	auto storage = lookup_storage(part_name);
	if(storage != nullptr) {
		mLastError = ESP_OK;
		return true;
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

	if(!init_custom(p, 0, p->size() / SPI_FLASH_SEC_SIZE)) {
		return false;
	}

	partition_list.push_back(p.release());
	return true;
}
#endif // ENABLE_NVS_ENCRYPTION

bool PartitionManager::deinit_partition(const char* partition_label)
{
	Storage* storage = lookup_storage(partition_label);
	if(!storage) {
		mLastError = ESP_ERR_NVS_NOT_INITIALIZED;
		return false;
	}

	storage_list.erase(storage);
	delete storage;

	for(auto it = partition_list.begin(); it != partition_list.end(); ++it) {
		if(it->name() == partition_label) {
			delete static_cast<Partition*>(it);
			break;
		}
	}

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

void PartitionManager::remove_partition(Partition* partition)
{
	assert(partition != nullptr);

	for(auto it = partition_list.begin(); it != partition_list.end(); ++it) {
		if(partition == it) {
			partition_list.erase(it);
			break;
		}
	}
}

Storage* PartitionManager::lookup_storage(const String& part_name)
{
	if(storage_list.empty()) {
		mLastError = ESP_ERR_NVS_NOT_INITIALIZED;
		return nullptr;
	}

	auto it = find_if(begin(storage_list), end(storage_list),
					  [=](Storage& e) -> bool { return e.partition().name() == part_name; });

	Storage* storage = it;
	mLastError = (storage == nullptr) ? ESP_ERR_NOT_FOUND : ESP_OK;
	return storage;
}

} // namespace nvs
