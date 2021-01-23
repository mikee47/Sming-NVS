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

#include <Nvs/nvs.h>
#include <Nvs/Partition.hpp>
#ifdef ENABLE_NVS_ENCRYPTION
#include <Nvs/EncryptedPartition.hpp>
#endif
#include <Storage/partition_info.h>
#include "FlashEmulator.h"

template <typename PartitionClass> class PartitionEmulatorTemplate : public PartitionClass
{
public:
	PartitionEmulatorTemplate(FlashEmulator& emu, uint32_t address, uint32_t size,
							  const char* partition_name = NVS_DEFAULT_PART_NAME)
		: PartitionClass(emu, info), info{partition_name,
										  Storage::Partition::Type::data,
										  uint8_t(Storage::Partition::SubType::Data::nvs),
										  address,
										  size,
										  0}
	{
		assert(partition_name);
		assert(size);
		emu.createPartition(info);
	}

	nvs::PartitionPtr ptr()
	{
		return nvs::PartitionPtr(new PartitionClass(*this));
	}

private:
	Storage::Partition::Info info;
};

class PartitionEmulator : public PartitionEmulatorTemplate<nvs::Partition>
{
public:
	using PartitionEmulatorTemplate::PartitionEmulatorTemplate;
};

struct PartitionEmulationFixture {
	PartitionEmulationFixture(uint32_t start_sector = 0, uint32_t sector_size = 1,
							  const char* partition_name = NVS_DEFAULT_PART_NAME)
		: emu(start_sector + sector_size),
		  part(emu, start_sector * SPI_FLASH_SEC_SIZE, sector_size * SPI_FLASH_SEC_SIZE, partition_name)
	{
	}

	~PartitionEmulationFixture()
	{
	}

	FlashEmulator emu;
	PartitionEmulator part;
};

#ifdef ENABLE_NVS_ENCRYPTION
class EncryptedPartitionEmulator : public PartitionEmulatorTemplate<nvs::EncryptedPartition>
{
public:
	using PartitionEmulatorTemplate::PartitionEmulatorTemplate;
};

struct EncryptedPartitionFixture {
	EncryptedPartitionFixture(const EncryptionKey& key, uint32_t start_sector = 0, uint32_t sector_size = 1,
							  const char* partition_name = NVS_DEFAULT_PART_NAME)
		: emu(start_sector + sector_size),
		  part(emu, start_sector * SPI_FLASH_SEC_SIZE, sector_size * SPI_FLASH_SEC_SIZE, partition_name)

	{
		assert(part.init(key) == ESP_OK);
	}

	FlashEmulator emu;
	EncryptedPartitionEmulator part;
};
#endif
