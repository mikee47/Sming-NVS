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

#include "include/Nvs/Item.hpp"

#ifdef ARCH_ESP32
#include <esp32/rom/crc.h>
#else
#include "crc.h"
#endif

String toString(nvs::ItemType itemType)
{
	using Type = nvs::ItemType;
	switch(itemType) {
	case Type::U8:
		return F("U8");
	case Type::I8:
		return F("I8");
	case Type::U16:
		return F("U16");
	case Type::I16:
		return F("I16");
	case Type::U32:
		return F("U32");
	case Type::I32:
		return F("I32");
	case Type::U64:
		return F("U64");
	case Type::I64:
		return F("I64");
	case Type::SZ:
		return F("STR");
	case Type::BLOB:
		return F("BLOB");
	case Type::BLOB_DATA:
		return F("BLOB_DATA");
	case Type::BLOB_IDX:
		return F("BLOB_IDX");
	case Type::ANY:
		return F("ANY");
	default:
		return F("UNK_") + unsigned(itemType);
	}
}

namespace nvs
{
uint32_t Item::calculateCrc32() const
{
	uint32_t result = 0xffffffff;
	const uint8_t* p = reinterpret_cast<const uint8_t*>(this);
	result = crc32_le(result, p + offsetof(Item, nsIndex), offsetof(Item, crc32) - offsetof(Item, nsIndex));
	result = crc32_le(result, p + offsetof(Item, key), sizeof(key));
	result = crc32_le(result, p + offsetof(Item, data), sizeof(data));
	return result;
}

uint32_t Item::calculateCrc32WithoutValue() const
{
	uint32_t result = 0xffffffff;
	const uint8_t* p = reinterpret_cast<const uint8_t*>(this);
	result = crc32_le(result, p + offsetof(Item, nsIndex), offsetof(Item, datatype) - offsetof(Item, nsIndex));
	result = crc32_le(result, p + offsetof(Item, key), sizeof(key));
	result = crc32_le(result, p + offsetof(Item, chunkIndex), sizeof(chunkIndex));
	return result;
}

uint32_t Item::calculateCrc32(const uint8_t* data, size_t size)
{
	uint32_t result = 0xffffffff;
	result = crc32_le(result, data, size);
	return result;
}

} // namespace nvs
