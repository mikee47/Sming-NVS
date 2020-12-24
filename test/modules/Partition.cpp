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

class PartitionTest : public TestGroup
{
public:
	PartitionTest() : TestGroup(_F("NVS Partitions"))
	{
	}

	void execute() override
	{
#ifdef ENABLE_NVS_ENCRYPTION
		TEST_CASE("encrypted partition read size must be item size")
		{
			char foo[32] = {};
			EncryptionKey key;
			memset(key.eky, 0x11, key.keySize);
			memset(key.tky, 0x22, key.keySize);
			EncryptedPartitionFixture fix(key);

			CHECK(fix.part->read(0, foo, sizeof(foo) - 1) == ESP_ERR_INVALID_SIZE);
		}

		TEST_CASE("encrypted partition write size must be mod item size")
		{
			char foo[64] = {};
			EncryptionKey key;
			memset(key.eky, 0x11, key.keySize);
			memset(key.tky, 0x22, key.keySize);
			EncryptedPartitionFixture fix(key);

			CHECK(fix.part->write(0, foo, sizeof(foo) - 1) == ESP_ERR_INVALID_SIZE);
			CHECK(fix.part->write(0, foo, sizeof(foo)) == ESP_OK);
			CHECK(fix.part->write(0, foo, sizeof(foo) * 2) == ESP_OK);
		}
#endif
	}
};

void REGISTER_TEST(Partition)
{
	registerGroup<PartitionTest>();
}
