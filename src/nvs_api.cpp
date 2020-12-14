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

using namespace nvs;

Handle* getHandle(nvs_handle_t c_handle)
{
	return reinterpret_cast<Handle*>(c_handle);
}

esp_err_t nvs_flash_init(void)
{
	partitionManager.init_partition(NVS_DEFAULT_PART_NAME);
	return partitionManager.lastError();
}

esp_err_t nvs_flash_erase_partition(const char* part_name)
{
	partitionManager.deinit_partition(part_name);

	auto part = partitionManager.find_partition(part_name);
	if(!part) {
		return ESP_ERR_NOT_FOUND;
	}

	return part.erase_range(0, part.size()) ? ESP_OK : ESP_ERR_FLASH_OP_FAIL;
}

esp_err_t nvs_flash_erase(void)
{
	return nvs_flash_erase_partition(NVS_DEFAULT_PART_NAME);
}

esp_err_t nvs_open(const char* name, OpenMode open_mode, nvs_handle_t* out_handle)
{
	auto part = partitionManager.open(name, open_mode);
	*out_handle = reinterpret_cast<nvs_handle_t>(part.release());
	return partitionManager.lastError();
}

void nvs_close(nvs_handle_t c_handle)
{
	delete getHandle(c_handle);
}

esp_err_t nvs_set_i8(nvs_handle_t c_handle, const char* key, int8_t value)
{
	return getHandle(c_handle)->set_item(key, value);
}

esp_err_t nvs_set_u8(nvs_handle_t c_handle, const char* key, uint8_t value)
{
	return getHandle(c_handle)->set_item(key, value);
}

esp_err_t nvs_set_i16(nvs_handle_t c_handle, const char* key, int16_t value)
{
	return getHandle(c_handle)->set_item(key, value);
}

esp_err_t nvs_set_u16(nvs_handle_t c_handle, const char* key, uint16_t value)
{
	return getHandle(c_handle)->set_item(key, value);
}

esp_err_t nvs_set_i32(nvs_handle_t c_handle, const char* key, int32_t value)
{
	return getHandle(c_handle)->set_item(key, value);
}

esp_err_t nvs_set_u32(nvs_handle_t c_handle, const char* key, uint32_t value)
{
	return getHandle(c_handle)->set_item(key, value);
}

esp_err_t nvs_set_i64(nvs_handle_t c_handle, const char* key, int64_t value)
{
	return getHandle(c_handle)->set_item(key, value);
}

esp_err_t nvs_set_u64(nvs_handle_t c_handle, const char* key, uint64_t value)
{
	return getHandle(c_handle)->set_item(key, value);
}

esp_err_t nvs_set_str(nvs_handle_t c_handle, const char* key, const char* value)
{
	return getHandle(c_handle)->set_string(key, value);
}

esp_err_t nvs_set_blob(nvs_handle_t c_handle, const char* key, const void* value, size_t length)
{
	return getHandle(c_handle)->set_blob(key, value, length);
}

esp_err_t nvs_get_i8(nvs_handle_t c_handle, const char* key, int8_t* out_value)
{
	if(out_value == nullptr) {
		return ESP_ERR_INVALID_ARG;
	}
	return getHandle(c_handle)->get_item(key, *out_value);
}

esp_err_t nvs_get_u8(nvs_handle_t c_handle, const char* key, uint8_t* out_value)
{
	if(out_value == nullptr) {
		return ESP_ERR_INVALID_ARG;
	}
	return getHandle(c_handle)->get_item(key, *out_value);
}

esp_err_t nvs_get_i16(nvs_handle_t c_handle, const char* key, int16_t* out_value)
{
	if(out_value == nullptr) {
		return ESP_ERR_INVALID_ARG;
	}
	return getHandle(c_handle)->get_item(key, *out_value);
}

esp_err_t nvs_get_u16(nvs_handle_t c_handle, const char* key, uint16_t* out_value)
{
	if(out_value == nullptr) {
		return ESP_ERR_INVALID_ARG;
	}
	return getHandle(c_handle)->get_item(key, *out_value);
}

esp_err_t nvs_get_i32(nvs_handle_t c_handle, const char* key, int32_t* out_value)
{
	if(out_value == nullptr) {
		return ESP_ERR_INVALID_ARG;
	}
	return getHandle(c_handle)->get_item(key, *out_value);
}

esp_err_t nvs_get_u32(nvs_handle_t c_handle, const char* key, uint32_t* out_value)
{
	if(out_value == nullptr) {
		return ESP_ERR_INVALID_ARG;
	}
	return getHandle(c_handle)->get_item(key, *out_value);
}

esp_err_t nvs_get_i64(nvs_handle_t c_handle, const char* key, int64_t* out_value)
{
	if(out_value == nullptr) {
		return ESP_ERR_INVALID_ARG;
	}
	return getHandle(c_handle)->get_item(key, *out_value);
}

esp_err_t nvs_get_u64(nvs_handle_t c_handle, const char* key, uint64_t* out_value)
{
	if(out_value == nullptr) {
		return ESP_ERR_INVALID_ARG;
	}
	return getHandle(c_handle)->get_item(key, *out_value);
}

static esp_err_t nvs_get_str_or_blob(nvs_handle_t c_handle, nvs::ItemType type, const char* key, void* out_value,
									 size_t* length)
{
	auto handle = getHandle(c_handle);

	size_t dataSize;
	auto err = handle->get_item_size(type, key, dataSize);
	if(err != ESP_OK) {
		return err;
	}

	if(length == nullptr) {
		return ESP_ERR_NVS_INVALID_LENGTH;
	}

	if(out_value == nullptr) {
		*length = dataSize;
		return ESP_OK;
	}

	if(*length < dataSize) {
		*length = dataSize;
		return ESP_ERR_NVS_INVALID_LENGTH;
	}

	*length = dataSize;
	return handle->get_typed_item(type, key, out_value, dataSize);
}

esp_err_t nvs_get_str(nvs_handle_t c_handle, const char* key, char* out_value, size_t* length)
{
	return nvs_get_str_or_blob(c_handle, nvs::ItemType::SZ, key, out_value, length);
}

esp_err_t nvs_get_blob(nvs_handle_t c_handle, const char* key, void* out_value, size_t* length)
{
	return nvs_get_str_or_blob(c_handle, nvs::ItemType::BLOB, key, out_value, length);
}

esp_err_t nvs_erase_key(nvs_handle_t c_handle, const char* key)
{
	return getHandle(c_handle)->erase_item(key);
}

esp_err_t nvs_erase_all(nvs_handle_t c_handle)
{
	return getHandle(c_handle)->erase_all();
}

esp_err_t nvs_commit(nvs_handle_t c_handle)
{
	return getHandle(c_handle)->commit();
}
