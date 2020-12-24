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

#if defined(ARCH_ESP32) && DEBUG_VERBOSE_LEVEL == DBG
#include <esp_log.h>
#define LOG_INFO(fmt, ...) ESP_LOGI("nvs", fmt, __VA_ARGS__)
#else
#define LOG_INFO(fmt, ...)
#endif

int nvs_errno;

using namespace nvs;

#define GET_HANDLE(key)                                                                                                \
	LOG_INFO("%s(0x%08x, \"%s\")", __FUNCTION__, c_handle, key);                                                       \
	auto handle = getHandle(c_handle);                                                                                 \
	if(c_handle < 0x10000 || c_handle > INT32_MAX) {                                                                   \
		return ESP_ERR_NVS_INVALID_HANDLE;                                                                             \
	}

#define CHECK_PTR(arg)                                                                                                 \
	if((arg) == nullptr) {                                                                                             \
		return ESP_ERR_INVALID_ARG;                                                                                    \
	}

#define HANDLE_METHOD(method, ...) return handle->method(__VA_ARGS__) ? ESP_OK : nvs_errno

Handle* getHandle(nvs_handle_t c_handle)
{
	return reinterpret_cast<Handle*>(c_handle);
}

esp_err_t nvs_flash_init(void)
{
	return partitionManager.openContainer(NVS_DEFAULT_PART_NAME) ? ESP_OK : nvs_errno;
}

esp_err_t nvs_flash_deinit(void)
{
	return nvs_flash_deinit_partition(NVS_DEFAULT_PART_NAME);
}

esp_err_t nvs_flash_deinit_partition(const char* partition_label)
{
	return partitionManager.closeContainer(partition_label) ? ESP_OK : nvs_errno;
}

esp_err_t nvs_flash_erase_partition(const char* part_name)
{
	partitionManager.closeContainer(part_name);

	auto part = partitionManager.findPartition(part_name);
	if(!part) {
		return ESP_ERR_NVS_PART_NOT_FOUND;
	}

	return part.erase_range(0, part.size()) ? ESP_OK : ESP_ERR_FLASH_OP_FAIL;
}

esp_err_t nvs_flash_erase(void)
{
	return nvs_flash_erase_partition(NVS_DEFAULT_PART_NAME);
}

esp_err_t nvs_open_from_partition(const char* part_name, const char* name, nvs_open_mode_t open_mode,
								  nvs_handle_t* out_handle)
{
	auto handle = partitionManager.openHandle(part_name, name, open_mode);
	if(!handle) {
		return nvs_errno;
	}

	*out_handle = reinterpret_cast<nvs_handle_t>(handle.release());
	LOG_INFO("%s(0x%08x)", __FUNCTION__, *out_handle);
	return ESP_OK;
}

esp_err_t nvs_open(const char* name, OpenMode open_mode, nvs_handle_t* out_handle)
{
	return nvs_open_from_partition(NVS_DEFAULT_PART_NAME, name, open_mode, out_handle);
}

void nvs_close(nvs_handle_t c_handle)
{
	LOG_INFO("%s(0x%08x)", __FUNCTION__, c_handle);
	delete getHandle(c_handle);
}

esp_err_t nvs_set_i8(nvs_handle_t c_handle, const char* key, int8_t value)
{
	GET_HANDLE(key);
	HANDLE_METHOD(setItem, key, value);
}

esp_err_t nvs_set_u8(nvs_handle_t c_handle, const char* key, uint8_t value)
{
	GET_HANDLE(key);
	HANDLE_METHOD(setItem, key, value);
}

esp_err_t nvs_set_i16(nvs_handle_t c_handle, const char* key, int16_t value)
{
	GET_HANDLE(key);
	HANDLE_METHOD(setItem, key, value);
}

esp_err_t nvs_set_u16(nvs_handle_t c_handle, const char* key, uint16_t value)
{
	GET_HANDLE(key);
	HANDLE_METHOD(setItem, key, value);
}

esp_err_t nvs_set_i32(nvs_handle_t c_handle, const char* key, int32_t value)
{
	GET_HANDLE(key);
	HANDLE_METHOD(setItem, key, value);
}

esp_err_t nvs_set_u32(nvs_handle_t c_handle, const char* key, uint32_t value)
{
	GET_HANDLE(key);
	HANDLE_METHOD(setItem, key, value);
}

esp_err_t nvs_set_i64(nvs_handle_t c_handle, const char* key, int64_t value)
{
	GET_HANDLE(key);
	HANDLE_METHOD(setItem, key, value);
}

esp_err_t nvs_set_u64(nvs_handle_t c_handle, const char* key, uint64_t value)
{
	GET_HANDLE(key);
	HANDLE_METHOD(setItem, key, value);
}

esp_err_t nvs_set_str(nvs_handle_t c_handle, const char* key, const char* value)
{
	GET_HANDLE(key);
	HANDLE_METHOD(setString, key, value);
}

esp_err_t nvs_set_blob(nvs_handle_t c_handle, const char* key, const void* value, size_t length)
{
	GET_HANDLE(key);
	HANDLE_METHOD(setBlob, key, value, length);
}

esp_err_t nvs_get_i8(nvs_handle_t c_handle, const char* key, int8_t* out_value)
{
	GET_HANDLE(key);
	CHECK_PTR(out_value);
	HANDLE_METHOD(getItem, key, *out_value);
}

esp_err_t nvs_get_u8(nvs_handle_t c_handle, const char* key, uint8_t* out_value)
{
	GET_HANDLE(key);
	CHECK_PTR(out_value);
	HANDLE_METHOD(getItem, key, *out_value);
}

esp_err_t nvs_get_i16(nvs_handle_t c_handle, const char* key, int16_t* out_value)
{
	GET_HANDLE(key);
	CHECK_PTR(out_value);
	HANDLE_METHOD(getItem, key, *out_value);
}

esp_err_t nvs_get_u16(nvs_handle_t c_handle, const char* key, uint16_t* out_value)
{
	GET_HANDLE(key);
	CHECK_PTR(out_value);
	HANDLE_METHOD(getItem, key, *out_value);
}

esp_err_t nvs_get_i32(nvs_handle_t c_handle, const char* key, int32_t* out_value)
{
	GET_HANDLE(key);
	CHECK_PTR(out_value);
	HANDLE_METHOD(getItem, key, *out_value);
}

esp_err_t nvs_get_u32(nvs_handle_t c_handle, const char* key, uint32_t* out_value)
{
	GET_HANDLE(key);
	CHECK_PTR(out_value);
	HANDLE_METHOD(getItem, key, *out_value);
}

esp_err_t nvs_get_i64(nvs_handle_t c_handle, const char* key, int64_t* out_value)
{
	GET_HANDLE(key);
	CHECK_PTR(out_value);
	HANDLE_METHOD(getItem, key, *out_value);
}

esp_err_t nvs_get_u64(nvs_handle_t c_handle, const char* key, uint64_t* out_value)
{
	GET_HANDLE(key);
	CHECK_PTR(out_value);
	HANDLE_METHOD(getItem, key, *out_value);
}

static esp_err_t nvs_get_str_or_blob(nvs_handle_t c_handle, nvs::ItemType type, const char* key, void* out_value,
									 size_t* length)
{
	GET_HANDLE(key);

	size_t dataSize;
	if(!handle->getItemDataSize(type, key, dataSize)) {
		return nvs_errno;
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
	HANDLE_METHOD(getItem, type, key, out_value, dataSize);
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
	GET_HANDLE(key);
	HANDLE_METHOD(eraseItem, key);
}

esp_err_t nvs_erase_all(nvs_handle_t c_handle)
{
	GET_HANDLE("");
	HANDLE_METHOD(eraseAll);
}

esp_err_t nvs_commit(nvs_handle_t c_handle)
{
	GET_HANDLE("");
	HANDLE_METHOD(commit);
}
