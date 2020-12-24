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

#include <NvsTest.h>
#include <Nvs/Handle.hpp>
#include <Nvs/PartitionManager.hpp>

using namespace nvs;

class PartitionManagerTest : public TestGroup
{
public:
	PartitionManagerTest() : TestGroup(_F("NVS Partition Manager"))
	{
	}

	void execute() override
	{
		TEST_CASE("Partition manager initializes storage")
		{
			PartitionEmulationFixture f(0, 10, "test");

			REQUIRE(partitionManager.openContainer(f.part.ptr()));
			CHECK(partitionManager.lookupContainer("test"));
			REQUIRE(partitionManager.closeContainer("test"));
		}

		TEST_CASE("Partition manager de-initializes storage")
		{
			PartitionEmulationFixture f(0, 10, "test");

			REQUIRE(partitionManager.openContainer(f.part.ptr()));
			CHECK(partitionManager.lookupContainer("test"));
			CHECK(partitionManager.closeContainer("test"));
			CHECK(!partitionManager.lookupContainer("test"));
		}

		TEST_CASE("Partition manager initializes multiple partitions")
		{
			FlashEmulator emu(10);
			PartitionEmulator part_0(emu, 0, 3 * SPI_FLASH_SEC_SIZE, "test1");
			PartitionEmulator part_1(emu, 6 * SPI_FLASH_SEC_SIZE, 3 * SPI_FLASH_SEC_SIZE, "test2");

			REQUIRE(partitionManager.openContainer(part_0.ptr()));
			REQUIRE(partitionManager.openContainer(part_1.ptr()));

			auto container1 = partitionManager.lookupContainer("test1");
			REQUIRE(container1);
			auto container2 = partitionManager.lookupContainer("test2");
			REQUIRE(container2);

			REQUIRE(container1 != container2);

			REQUIRE(partitionManager.closeContainer("test1"));
			REQUIRE(partitionManager.closeContainer("test2"));
		}
	}
};

void REGISTER_TEST(PartitionManager)
{
	registerGroup<PartitionManagerTest>();
}
