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

#ifndef ESP_PARTITION_HPP_
#define ESP_PARTITION_HPP_

#include <Storage/Partition.h>
#include "intrusive_list.h"

#define ESP_ERR_FLASH_OP_FAIL    (ESP_ERR_FLASH_BASE + 1)

namespace nvs
{
/**
 * Implementation of Partition for NVS.
 *
 * It is implemented as an intrusive_list_node to easily store instances of it. NVSStorage and NVSPage take pointer
 * references of this class to abstract their partition operations.
 */
class NVSPartition : public ::Storage::Partition, public intrusive_list_node<NVSPartition>
{
public:
	NVSPartition(const Partition& part) : Partition(part)
	{
	}

	virtual ~NVSPartition()
	{
	}

	virtual esp_err_t read(size_t offset, void* dst, size_t size)
	{
		return read_raw(offset, dst, size);
	}

	esp_err_t read_raw(size_t offset, void* dst, size_t size)
	{
		return Partition::read(offset, dst, size) ? ESP_OK : ESP_ERR_FLASH_OP_FAIL;
	}

	virtual esp_err_t write(size_t offset, const void* src, size_t size)
	{
		return write_raw(offset, src, size);
	}

	esp_err_t write_raw(size_t offset, const void* src, size_t size)
	{
		return Partition::write(offset, src, size) ? ESP_OK : ESP_ERR_FLASH_OP_FAIL;
	}
};

} // namespace nvs

#endif // ESP_PARTITION_HPP_
