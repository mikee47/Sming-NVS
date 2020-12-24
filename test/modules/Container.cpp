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

class ContainerTest : public TestGroup
{
public:
	ContainerTest() : TestGroup(_F("NVS Container"))
	{
	}

	void execute() override
	{
		TEST_CASE("Storage iterator recognizes blob with VerOffset::VER_1_OFFSET", "[nvs_storage]")
		{
			PartitionEmulationFixture f(0, 3, "test");
			auto container = partitionManager.openContainer(f.part.ptr());
			REQUIRE(container != nullptr);

			uint8_t blob[] = {0x0, 0x1, 0x2, 0x3};
			uint8_t blob_new[] = {0x3, 0x2, 0x1, 0x0};
			uint8_t ns_index;
			REQUIRE(container->createOrOpenNamespace("test_ns", true, ns_index));

			CHECK(container->writeItem(ns_index, ItemType::BLOB, "test_blob", blob, sizeof(blob)));

			// changing provokes a blob with version offset 1 (VerOffset::VER_1_OFFSET)
			CHECK(container->writeItem(ns_index, ItemType::BLOB, "test_blob", blob_new, sizeof(blob_new)));

			// Central check: does the iterator recognize the blob with version 1?
			auto it = container->find("test_ns");
			REQUIRE(it);

			CHECK(it->nsName() == "test_ns");
			CHECK(it->key() == "test_blob");
			debug_i("itemType = %s", toString(it->type()).c_str());
			CHECK(it->type() == ItemType::BLOB_DATA);

			delete container;
		}
	}
};

void REGISTER_TEST(Container)
{
	registerGroup<ContainerTest>();
}
