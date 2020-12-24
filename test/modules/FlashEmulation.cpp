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

bool wordsEmpty(const uint32_t* word, size_t n)
{
	return std::all_of(word, word + n, [](uint32_t v) { return v == 0xffffffff; });
}

class FlashEmulationTest : public TestGroup
{
public:
	FlashEmulationTest() : TestGroup(_F("Flash Emulation"))
	{
	}

	void execute() override
	{
		TEST_CASE("flash starts with all bytes == 0xff")
		{
			FlashEmulator f(4);

			uint32_t sector[SPI_FLASH_SEC_SIZE / 4];

			for(int i = 0; i < 4; ++i) {
				CHECK(f.read(0, sector, sizeof(sector)));
				CHECK(wordsEmpty(sector, sizeof(sector) / 4));
			}
		}

		TEST_CASE("invalid writes are checked")
		{
			FlashEmulator f(1);

			uint32_t val = 0;
			CHECK(f.write(0, &val, 4));
			val = 1;
			CHECK(!f.write(0, &val, 4));
		}

		TEST_CASE("out of bounds writes fail")
		{
			FlashEmulator f(4);
			uint32_t vals[8]{0};
			CHECK(f.write(0, &vals, sizeof(vals)));

			CHECK(f.write(4 * 4096 - sizeof(vals), &vals, sizeof(vals)));

			CHECK(!f.write(4 * 4096 - sizeof(vals) + 4, &vals, sizeof(vals)));
		}

		TEST_CASE("after erase the sector is set to 0xff")
		{
			FlashEmulator f(4);
			uint32_t val1{0xab00cd12};
			CHECK(f.write(0, &val1, sizeof(val1)));
			uint32_t val2{0x5678efab};
			CHECK(f.write(4096 - 4, &val2, sizeof(val2)));

			CHECK_EQ(f.words()[0], val1);
			CHECK(wordsEmpty(f.words() + 1, 4096 / 4 - 2));
			CHECK_EQ(f.words()[4096 / 4 - 1], val2);

			CHECK(f.erase_range(0, SPI_FLASH_SEC_SIZE));

			CHECK_EQ(f.words()[0], 0xffffffff);
			CHECK(wordsEmpty(f.words() + 1, 4096 / 4 - 2));
			CHECK_EQ(f.words()[4096 / 4 - 1], 0xffffffff);
		}

		TEST_CASE("EMU raw read function works")
		{
			FlashEmulator f(4);
			uint32_t value{0xdeadbeef};
			uint32_t read_value{0};
			CHECK(f.write(0, &value, sizeof(value)));

			CHECK(f.read(0, &read_value, sizeof(&read_value)));

			CHECK_EQ(read_value, value);
		}

		TEST_CASE("EMU raw write function works")
		{
			FlashEmulator f(4);
			uint32_t value{0xdeadbeef};
			uint32_t read_value{0};
			CHECK(f.write(0, &value, sizeof(value)));

			CHECK(f.read(0, &read_value, sizeof(&read_value)));

			CHECK_EQ(read_value, value);
		}

		TEST_CASE("read/write/erase operation times are calculated correctly")
		{
			FlashEmulator f(1);
			auto& stat = f.stat();
			uint8_t data[512];
			f.read(0, data, 4);
			CHECK_EQ(stat.totalTime, 7U);
			CHECK_EQ(stat.readOps, 1U);
			CHECK_EQ(stat.readBytes, 4U);
			f.clearStats();
			f.read(0, data, 8);
			CHECK_EQ(stat.totalTime, 5U);
			CHECK_EQ(stat.readOps, 1U);
			CHECK_EQ(stat.readBytes, 8U);
			f.clearStats();
			f.read(0, data, 16);
			CHECK_EQ(stat.totalTime, 6U);
			CHECK_EQ(stat.readOps, 1U);
			CHECK_EQ(stat.readBytes, 16U);
			f.clearStats();
			f.read(0, data, 128);
			CHECK_EQ(stat.totalTime, 18U);
			CHECK_EQ(stat.readOps, 1U);
			CHECK_EQ(stat.readBytes, 128U);
			f.clearStats();
			f.read(0, data, 256);
			CHECK_EQ(stat.totalTime, 32U);
			f.clearStats();
			f.read(0, data, (128 + 256) / 2);
			CHECK_EQ(stat.totalTime, (18U + 32) / 2);
			f.clearStats();

			f.write(0, data, 4);
			CHECK_EQ(stat.totalTime, 19U);
			CHECK_EQ(stat.writeOps, 1U);
			CHECK_EQ(stat.writeBytes, 4U);
			f.clearStats();
			CHECK_EQ(stat.writeOps, 0U);
			CHECK_EQ(stat.writeBytes, 0U);
			f.write(0, data, 8);
			CHECK_EQ(stat.totalTime, 23U);
			f.clearStats();
			f.write(0, data, 16);
			CHECK_EQ(stat.totalTime, 35U);
			CHECK_EQ(stat.writeOps, 1U);
			CHECK_EQ(stat.writeBytes, 16U);
			f.clearStats();
			f.write(0, data, 128);
			CHECK_EQ(stat.totalTime, 205U);
			f.clearStats();
			f.write(0, data, 256);
			CHECK_EQ(stat.totalTime, 417U);
			f.clearStats();
			f.write(0, data, (128 + 256) / 2);
			CHECK_EQ(stat.totalTime, (205U + 417) / 2);
			f.clearStats();

			f.erase_range(0, SPI_FLASH_SEC_SIZE);
			CHECK_EQ(stat.eraseOps, 1U);
			CHECK_EQ(stat.totalTime, 37142U);
		}
	}
};

void REGISTER_TEST(FlashEmulation)
{
	registerGroup<FlashEmulationTest>();
}
