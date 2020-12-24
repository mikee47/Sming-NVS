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
#include <Nvs/PartitionManager.hpp>

using namespace nvs;

class InitializationTest : public TestGroup
{
public:
	InitializationTest() : TestGroup(_F("NVS Initialization"))
	{
	}

	void execute() override
	{
		TEST_CASE("nvs_flash_init_partition_ptr inits one partition")
		{
			PartitionEmulationFixture f(6, 3, "test");

			REQUIRE(!partitionManager.lookupContainer("test"));
			REQUIRE(partitionManager.openContainer(f.part.ptr()));
			CHECK(partitionManager.lookupContainer("test"));
			CHECK(partitionManager.closeContainer("test"));
		}
	}
};

void REGISTER_TEST(initialization)
{
	registerGroup<InitializationTest>();
}
