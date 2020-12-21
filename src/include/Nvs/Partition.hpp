// Copyright 2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <Storage/Partition.h>
#include "esp_err.h"

#define ESP_ERR_FLASH_OP_FAIL (ESP_ERR_FLASH_BASE + 1)

/**
 * @brief Key for encryption and decryption
 */
struct EncryptionKey {
	static constexpr uint8_t keySize{32}; // AES-256

	uint8_t eky[keySize]; /*!<  XTS encryption and decryption key*/
	uint8_t tky[keySize]; /*!<  XTS tweak key */
};

namespace nvs
{
/**
 * Implementation of Partition for NVS.
 *
 * It is implemented as an intrusive_list_node to easily store instances of it. NVSStorage and NVSPage take pointer
 * references of this class to abstract their partition operations.
 */
class Partition
{
public:
	using Flags = Storage::Partition::Flags;
	using Info = Storage::Partition::Info;

	Partition(const Storage::Partition& part) : mPartition(part)
	{
	}

	Partition(Storage::Device& device, const Info& info) : mPartition(device, info)
	{
	}

	virtual ~Partition()
	{
	}

	bool operator==(const String& part_name) const
	{
		return mPartition == part_name;
	}

	String name() const
	{
		return mPartition.name();
	}

	size_t size() const
	{
		return mPartition.size();
	}

	virtual esp_err_t read(size_t offset, void* dst, size_t size)
	{
		return read_raw(offset, dst, size);
	}

	esp_err_t read_raw(size_t offset, void* dst, size_t size)
	{
		return mPartition.read(offset, dst, size) ? ESP_OK : ESP_ERR_FLASH_OP_FAIL;
	}

	virtual esp_err_t write(size_t offset, const void* src, size_t size)
	{
		return write_raw(offset, src, size);
	}

	esp_err_t write_raw(size_t offset, const void* src, size_t size)
	{
		return mPartition.write(offset, src, size) ? ESP_OK : ESP_ERR_FLASH_OP_FAIL;
	}

	esp_err_t erase_range(size_t offset, size_t size)
	{
		return mPartition.erase_range(offset, size) ? ESP_OK : ESP_ERR_FLASH_OP_FAIL;
	}

	Storage::Partition getPart() const
	{
		return mPartition;
	}

private:
	Storage::Partition mPartition;
};

using PartitionPtr = std::unique_ptr<Partition>;

} // namespace nvs
