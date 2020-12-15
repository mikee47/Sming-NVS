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

#include <cstdlib>
#include "include/Nvs/Handle.hpp"
#include "include/Nvs/PartitionManager.hpp"

#define CHECK_VALID()                                                                                                  \
	if(mStorage == nullptr) {                                                                                          \
		return ESP_ERR_NVS_INVALID_HANDLE;                                                                             \
	}

#define CHECK_WRITE()                                                                                                  \
	CHECK_VALID()                                                                                                      \
	if(mReadOnly) {                                                                                                    \
		return ESP_ERR_NVS_READ_ONLY;                                                                                  \
	}

namespace nvs
{
Handle::~Handle()
{
	if(mStorage != nullptr) {
		mStorage->invalidate_handle(this);
	}
}

esp_err_t Handle::set_typed_item(ItemType datatype, const char* key, const void* data, size_t dataSize)
{
	CHECK_WRITE()
	return mStorage->writeItem(mNsIndex, datatype, key, data, dataSize);
}

esp_err_t Handle::get_typed_item(ItemType datatype, const char* key, void* data, size_t dataSize)
{
	CHECK_VALID()
	return mStorage->readItem(mNsIndex, datatype, key, data, dataSize);
}

esp_err_t Handle::set_string(const char* key, const char* str)
{
	CHECK_WRITE()
	return mStorage->writeItem(mNsIndex, nvs::ItemType::SZ, key, str, strlen(str) + 1);
}

esp_err_t Handle::set_blob(const char* key, const void* blob, size_t len)
{
	CHECK_WRITE()
	return mStorage->writeItem(mNsIndex, nvs::ItemType::BLOB, key, blob, len);
}

esp_err_t Handle::get_string(const char* key, char* out_str, size_t len)
{
	CHECK_VALID()
	return mStorage->readItem(mNsIndex, nvs::ItemType::SZ, key, out_str, len);
}

esp_err_t Handle::get_blob(const char* key, void* out_blob, size_t len)
{
	CHECK_VALID()
	return mStorage->readItem(mNsIndex, nvs::ItemType::BLOB, key, out_blob, len);
}

esp_err_t Handle::get_item_size(ItemType datatype, const char* key, size_t& size)
{
	CHECK_VALID()
	return mStorage->getItemDataSize(mNsIndex, datatype, key, size);
}

esp_err_t Handle::erase_item(const char* key)
{
	CHECK_WRITE()
	return mStorage->eraseItem(mNsIndex, key);
}

esp_err_t Handle::erase_all()
{
	CHECK_WRITE()
	return mStorage->eraseNamespace(mNsIndex);
}

esp_err_t Handle::commit()
{
	CHECK_VALID()
	return ESP_OK;
}

esp_err_t Handle::get_used_entry_count(size_t& used_entries)
{
	used_entries = 0;

	CHECK_VALID()

	size_t used_entry_count;
	esp_err_t err = mStorage->calcEntriesInNamespace(mNsIndex, used_entry_count);
	if(err == ESP_OK) {
		used_entries = used_entry_count;
	}

	return err;
}

void Handle::debugDump()
{
	assert(mStorage != nullptr);
	return mStorage->debugDump();
}

esp_err_t Handle::fillStats(nvs_stats_t& nvsStats)
{
	CHECK_VALID()
	return mStorage->fillStats(nvsStats);
}

esp_err_t Handle::calcEntriesInNamespace(size_t& usedEntries)
{
	CHECK_VALID()
	return mStorage->calcEntriesInNamespace(mNsIndex, usedEntries);
}

} // namespace nvs
