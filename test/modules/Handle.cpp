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

class HandleTest : public TestGroup
{
public:
	HandleTest() : TestGroup(_F("NVS Handle"))
	{
	}

	void execute() override
	{
		TEST_CASE("Handle closes its reference in PartitionManager")
		{
			PartitionEmulationFixture f(0, 10, "test");
			CHECK(partitionManager.openContainer(f.part.ptr()));

			CHECK(partitionManager.handleCount() == 0);

			auto handle = partitionManager.openHandle("test", "ns_1", NVS_READWRITE);
			REQUIRE(handle);

			CHECK(partitionManager.handleCount() == 1);

			handle.reset();

			CHECK(partitionManager.handleCount() == 0);

			REQUIRE(partitionManager.closeContainer("test") == true);
		}

		TEST_CASE("Handle multiple open and closes with PartitionManager")
		{
			PartitionEmulationFixture f(0, 10, "test");
			CHECK(partitionManager.openContainer(f.part.ptr()));

			CHECK(partitionManager.handleCount() == 0);

			auto handle1 = partitionManager.openHandle("test", "ns_1", NVS_READWRITE);
			REQUIRE(handle1);

			CHECK(partitionManager.handleCount() == 1);

			auto handle2 = partitionManager.openHandle("test", "ns_1", NVS_READWRITE);
			REQUIRE(handle2);

			CHECK(partitionManager.handleCount() == 2);

			handle1.reset();

			CHECK(partitionManager.handleCount() == 1);

			handle2.reset();

			CHECK(partitionManager.handleCount() == 0);

			REQUIRE(partitionManager.closeContainer("test") == true);
		}

		TEST_CASE("Handle readonly fails")
		{
			PartitionEmulationFixture f(0, 10);

			partitionManager.closeContainer(NVS_DEFAULT_PART_NAME);

			CHECK(partitionManager.openContainer(f.part.ptr()));
			CHECK(partitionManager.handleCount() == 0);

			// first, creating namespace...
			auto handle_1 = partitionManager.openHandle(NVS_DEFAULT_PART_NAME, "ns_1", NVS_READWRITE);
			REQUIRE(handle_1);
			CHECK(partitionManager.handleCount() == 1);

			handle_1.reset();

			CHECK(partitionManager.handleCount() == 0);
			auto handle_2 = partitionManager.openHandle(NVS_DEFAULT_PART_NAME, "ns_1", NVS_READONLY);
			REQUIRE(handle_2);
			CHECK(!handle_2->setItem("key", 47));
			CHECK(nvs_errno == ESP_ERR_NVS_READ_ONLY);
			CHECK(partitionManager.handleCount() == 1);

			handle_2.reset();

			CHECK(partitionManager.handleCount() == 0);
			// without deinit it affects "nvs api tests"
			CHECK(partitionManager.closeContainer(NVS_DEFAULT_PART_NAME));
		}

		TEST_CASE("Handle set/get char")
		{
			enum class TestEnum : signed char {
				FOO = -1,
				BEER,
				BAR,
			};

			PartitionEmulationFixture f(0, 10);
			CHECK(partitionManager.openContainer(f.part.ptr()));

			auto handle = partitionManager.openHandle(NVS_DEFAULT_PART_NAME, "ns_1", NVS_READWRITE);
			REQUIRE(handle);

			char test_e = 'a';
			char test_e_read = 'z';

			CHECK(handle->setItem("key", test_e) == true);

			CHECK(handle->getItem("key", test_e_read) == true);
			CHECK(test_e == test_e_read);

			handle.reset();

			REQUIRE(partitionManager.closeContainer(NVS_DEFAULT_PART_NAME) == true);
		}

		TEST_CASE("Handle correctly sets/gets int enum")
		{
			enum class TestEnum : int {
				FOO,
				BAR,
			};

			PartitionEmulationFixture f(0, 10);
			CHECK(partitionManager.openContainer(f.part.ptr()));

			auto handle = partitionManager.openHandle(NVS_DEFAULT_PART_NAME, "ns_1", NVS_READWRITE);
			REQUIRE(handle);

			TestEnum test_e = TestEnum::BAR;
			TestEnum test_e_read = TestEnum::FOO;

			CHECK(handle->setItem("key", test_e) == true);

			CHECK(handle->getItem("key", test_e_read) == true);
			CHECK(test_e == test_e_read);

			handle.reset();

			REQUIRE(partitionManager.closeContainer(NVS_DEFAULT_PART_NAME) == true);
		}

		TEST_CASE("Handle correctly sets/gets int enum with negative values")
		{
			enum class TestEnum : int {
				FOO = -1,
				BEER,
				BAR,
			};

			PartitionEmulationFixture f(0, 10);
			CHECK(partitionManager.openContainer(f.part.ptr()));

			auto handle = partitionManager.openHandle(NVS_DEFAULT_PART_NAME, "ns_1", NVS_READWRITE);
			REQUIRE(handle);

			TestEnum test_e = TestEnum::FOO;
			TestEnum test_e_read = TestEnum::BEER;

			CHECK(handle->setItem("key", test_e) == true);

			CHECK(handle->getItem("key", test_e_read) == true);
			CHECK(test_e == test_e_read);

			handle.reset();

			REQUIRE(partitionManager.closeContainer(NVS_DEFAULT_PART_NAME) == true);
		}

		TEST_CASE("Handle correctly sets/gets uint8_t enum")
		{
			enum class TestEnum : uint8_t {
				FOO,
				BAR,
			};

			PartitionEmulationFixture f(0, 10);
			CHECK(partitionManager.openContainer(f.part.ptr()));

			auto handle = partitionManager.openHandle(NVS_DEFAULT_PART_NAME, "ns_1", NVS_READWRITE);
			REQUIRE(handle);

			TestEnum test_e = TestEnum::BAR;
			TestEnum test_e_read = TestEnum::FOO;

			CHECK(handle->setItem("key", test_e) == true);

			CHECK(handle->getItem("key", test_e_read) == true);
			CHECK(test_e == test_e_read);

			handle.reset();

			REQUIRE(partitionManager.closeContainer(NVS_DEFAULT_PART_NAME) == true);
		}

		TEST_CASE("Handle correctly sets/gets char enum")
		{
			enum class TestEnum : signed char {
				FOO = -1,
				BEER,
				BAR,
			};

			PartitionEmulationFixture f(0, 10);
			CHECK(partitionManager.openContainer(f.part.ptr()));

			auto handle = partitionManager.openHandle(NVS_DEFAULT_PART_NAME, "ns_1", NVS_READWRITE);
			REQUIRE(handle);

			TestEnum test_e = TestEnum::BAR;
			TestEnum test_e_read = TestEnum::FOO;

			CHECK(handle->setItem("key", test_e) == true);

			CHECK(handle->getItem("key", test_e_read) == true);
			CHECK(test_e == test_e_read);

			handle.reset();

			REQUIRE(partitionManager.closeContainer(NVS_DEFAULT_PART_NAME) == true);
		}
	}
};

void REGISTER_TEST(Handle)
{
	registerGroup<HandleTest>();
}
