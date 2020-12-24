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

class CxxApiTest : public TestGroup
{
public:
	CxxApiTest() : TestGroup(_F("NVS CXX API"))
	{
	}

	void execute() override
	{
		TEST_CASE("NVSHandle CXX api open invalid arguments")
		{
			PartitionEmulationFixture f(6, 3, "test");
			REQUIRE(partitionManager.openContainer(f.part.ptr()));

			auto handle = partitionManager.openHandle("test", nullptr, NVS_READWRITE);
			CHECK(!handle);
			CHECK_EQ(nvs_errno, ESP_ERR_INVALID_ARG);

			partitionManager.closeContainer("test");
		}

		TEST_CASE("NVSHandle CXX api open partition uninitialized")
		{
			auto handle = partitionManager.openHandle("test", "ns_1", NVS_READWRITE);
			CHECK(!handle);
			CHECK(nvs_errno == ESP_ERR_NVS_NOT_INITIALIZED || nvs_errno == ESP_ERR_NVS_PART_NOT_FOUND);
		}

		TEST_CASE("NVSHandle CXX api open successful")
		{
			PartitionEmulationFixture f(6, 3, "test");
			REQUIRE(partitionManager.openContainer(f.part.ptr()));

			auto handle = partitionManager.openHandle("test", "ns_1", NVS_READWRITE);
			CHECK(handle);

			CHECK(partitionManager.handleCount() == 1);

			handle.reset();

			CHECK(partitionManager.handleCount() == 0);

			partitionManager.closeContainer("test");
		}

		TEST_CASE("NVSHandle CXX api open default part successful")
		{
			PartitionEmulationFixture f(6, 3);
			REQUIRE(partitionManager.openContainer(f.part.ptr()));

			CHECK(partitionManager.handleCount() == 0);

			auto handle = partitionManager.openHandle(NVS_DEFAULT_PART_NAME, "ns_1", NVS_READWRITE);
			CHECK(handle);

			CHECK(partitionManager.handleCount() == 1);

			handle.reset();

			CHECK(partitionManager.handleCount() == 0);

			CHECK(partitionManager.closeContainer(NVS_DEFAULT_PART_NAME));
		}

		TEST_CASE("NVSHandle CXX api open default part ns NULL")
		{
			PartitionEmulationFixture f(6, 3);
			REQUIRE(partitionManager.openContainer(f.part.ptr()));

			REQUIRE(partitionManager.handleCount() == 0);

			auto handle = partitionManager.openHandle(NVS_DEFAULT_PART_NAME, nullptr, NVS_READWRITE);
			REQUIRE(!handle);
			REQUIRE_EQ(nvs_errno, ESP_ERR_INVALID_ARG);

			REQUIRE(partitionManager.handleCount() == 0);

			CHECK(partitionManager.closeContainer(NVS_DEFAULT_PART_NAME));
		}

		TEST_CASE("NVSHandle CXX api read/write string")
		{
			PartitionEmulationFixture f(6, 3);
			REQUIRE(partitionManager.openContainer(f.part.ptr()));

			REQUIRE(partitionManager.handleCount() == 0);

			auto handle = partitionManager.openHandle(NVS_DEFAULT_PART_NAME, "test_ns", NVS_READWRITE);
			REQUIRE(handle);

			REQUIRE(partitionManager.handleCount() == 1);

			DEFINE_FSTR_LOCAL(test_string, "test string");
			REQUIRE(handle->setString("test", test_string) == true);
			REQUIRE(handle->commit() == true);
			REQUIRE_EQ(handle->getString("test"), test_string);

			handle.reset();

			REQUIRE(partitionManager.closeContainer(NVS_DEFAULT_PART_NAME));
		}

		TEST_CASE("NVSHandle CXX api read/write blob")
		{
			PartitionEmulationFixture f(6, 3);
			REQUIRE(partitionManager.openContainer(f.part.ptr()));

			REQUIRE(partitionManager.handleCount() == 0);

			auto handle = partitionManager.openHandle(NVS_DEFAULT_PART_NAME, "test_ns", NVS_READWRITE);
			REQUIRE(handle);

			REQUIRE(partitionManager.handleCount() == 1);

			static constexpr size_t blobSize{6};
			const char blob[blobSize] = {15, 16, 17, 18, 19};
			char read_blob[blobSize] = {0};
			REQUIRE(handle->setBlob("test", blob, blobSize));
			REQUIRE(handle->commit());
			REQUIRE(handle->getBlob("test", read_blob, blobSize));
			REQUIRE(memcmp(blob, read_blob, blobSize) == 0);

			handle.reset();

			REQUIRE(partitionManager.closeContainer(NVS_DEFAULT_PART_NAME));
		}
	}
};

void REGISTER_TEST(cxx_api)
{
	registerGroup<CxxApiTest>();
}
