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
#ifdef CONFIG_NVS_ENCRYPTION
#include "nvs_encrypted_partition.hpp"
#endif
#include <Storage/PartitionTable.h>

namespace nvs
{
PartitionManager partitionManager;

Partition* PartitionManager::lookup_partition(const char* label)
{
	Partition* partition{};

	auto part = ::Storage::PartitionTable().find(label);
	if(!part) {
		mLastError = ESP_ERR_NOT_FOUND;
	} else if(!part.verify(::Storage::Partition::SubType::Data::nvs)) {
		mLastError = ESP_ERR_NOT_SUPPORTED;
	} else if(part.isEncrypted()) {
		mLastError = ESP_ERR_NVS_WRONG_ENCRYPTION;
	} else {
		partition = new(std::nothrow) Partition(part);
		mLastError = partition ? ESP_OK : ESP_ERR_NO_MEM;
	}

	return partition;
}

#ifdef CONFIG_NVS_ENCRYPTION
Partition* PartitionManager::lookup_encrypted_partition(const char* label, const nvs_sec_cfg_t& cfg)
{
	Partition* partition{};

	auto part = ::Storage::PartitionTable().find(label);
	if(!part) {
		mLastError = ESP_ERR_NOT_FOUND;
	} else if(!part.verify(::Storage::Partition::SubType::Data::nvs)) {
		mLastError = ESP_ERR_NOT_SUPPORTED;
	} else if(part.isEncrypted()) {
		mLastError = ESP_ERR_NVS_WRONG_ENCRYPTION;
	} else {
		auto enc_p = new(std::nothrow) NVSEncryptedPartition(part);

		if(enc_p == nullptr) {
			mLastError = ESP_ERR_NO_MEM;
		} else {
			mLastError = enc_p->init(cfg);
			if(mLastError != ESP_OK) {
				delete enc_p;
			} else {
				partition = enc_p;
			}
		}
	}

	return partition;
}

#endif // CONFIG_NVS_ENCRYPTION

bool PartitionManager::init_partition(const char* partition_label)
{
	if(strlen(partition_label) > NVS_PART_NAME_MAX_SIZE) {
		mLastError = ESP_ERR_INVALID_ARG;
		return false;
	}

	auto storage = lookup_storage_from_name(partition_label);
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

	nvs_partition_list.push_back(p);

	return true;
}

bool PartitionManager::init_custom(Partition* partition, uint32_t baseSector, uint32_t sectorCount)
{
	auto storage = lookup_storage_from_name(partition->name());
	if(storage == nullptr) {
		storage = new(std::nothrow) Storage(*partition);

		if(storage == nullptr) {
			mLastError = ESP_ERR_NO_MEM;
			delete partition;
		} else {
			mLastError = storage->init(baseSector, sectorCount);
			if(mLastError == ESP_OK) {
				nvs_storage_list.push_back(storage);
			} else {
				delete storage;
			}
		}
	} else {
		// Storage was already initialized, delete un-needed partition
		for(auto it = nvs_partition_list.begin(); it != nvs_partition_list.end(); ++it) {
			if(partition == it) {
				nvs_partition_list.erase(it);
				delete partition;
				break;
			}
		}

		mLastError = storage->init(baseSector, sectorCount);
	}

	return mLastError == ESP_OK;
}

#ifdef CONFIG_NVS_ENCRYPTION
bool PartitionManager::secure_init_partition(const char* part_name, const nvs_sec_cfg_t* cfg)
{
	if(strlen(part_name) > NVS_PART_NAME_MAX_SIZE) {
		mLastError = ESP_ERR_INVALID_ARG;
		return false;
	}

	auto storage = lookup_storage_from_name(part_name);
	if(storage != nullptr) {
		mLastError = ESP_OK;
		return true;
	}

	Partition* p;
	if(cfg != nullptr) {
		p = partition_lookup::lookup_encrypted_partition(part_name, *cfg);
	} else {
		p = partition_lookup::lookup_partition(part_name);
	}
	if(p == nullptr) {
		return false;
	}

	if(!init_custom(p, 0, p->size() / SPI_FLASH_SEC_SIZE)) {
		delete p;
		return false;
	}

	nvs_partition_list.push_back(p);
	return true;
}
#endif // CONFIG_NVS_ENCRYPTION

bool PartitionManager::deinit_partition(const char* partition_label)
{
	Storage* storage = lookup_storage_from_name(partition_label);
	if(!storage) {
		mLastError = ESP_ERR_NVS_NOT_INITIALIZED;
		return false;
	}

	/* Clean up handles related to the storage being deinitialized */
	for(auto it = nvs_handles.begin(); it != nvs_handles.end(); ++it) {
		if(it->mStoragePtr == storage) {
			it->valid = false;
			nvs_handles.erase(it);
		}
	}

	/* Finally delete the storage and its partition */
	nvs_storage_list.erase(storage);
	delete storage;

	for(auto it = nvs_partition_list.begin(); it != nvs_partition_list.end(); ++it) {
		if(it->name() == partition_label) {
			Partition* p = it;
			nvs_partition_list.erase(it);
			delete p;
			break;
		}
	}

	mLastError = ESP_OK;
	return true;
}

HandlePtr PartitionManager::open(const char* part_name, const char* ns_name, nvs_open_mode_t open_mode)
{
	if(part_name == nullptr || ns_name == nullptr) {
		mLastError = ESP_ERR_INVALID_ARG;
		return nullptr;
	}

	if(nvs_storage_list.empty()) {
		mLastError = ESP_ERR_NVS_NOT_INITIALIZED;
		return nullptr;
	}

	Storage* sHandle = lookup_storage_from_name(part_name);
	if(sHandle == nullptr) {
		mLastError = ESP_ERR_NVS_PART_NOT_FOUND;
		return nullptr;
	}

	uint8_t nsIndex;
	mLastError = sHandle->createOrOpenNamespace(ns_name, open_mode == NVS_READWRITE, nsIndex);
	if(mLastError != ESP_OK) {
		return nullptr;
	}

	auto handle = new(std::nothrow) Handle(open_mode == NVS_READONLY, nsIndex, sHandle);

	if(handle == nullptr) {
		mLastError = ESP_ERR_NO_MEM;
	} else {
		nvs_handles.push_back(handle);
		mLastError = ESP_OK;
	}

	return HandlePtr(handle);
}

void PartitionManager::close_handle(Handle* handle)
{
	if(handle == nullptr) {
		return;
	}

	for(auto it = nvs_handles.begin(); it != nvs_handles.end(); ++it) {
		if(it == intrusive_list<Handle>::iterator(handle)) {
			nvs_handles.erase(it);
			break;
		}
	}
}

size_t PartitionManager::open_handles_size()
{
	return nvs_handles.size();
}

Storage* PartitionManager::lookup_storage_from_name(const String& name)
{
	auto it = find_if(begin(nvs_storage_list), end(nvs_storage_list),
					  [=](Storage& e) -> bool { return e.partition().name() == name; });

	if(it == end(nvs_storage_list)) {
		return nullptr;
	}
	return it;
}

} // namespace nvs
