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

#pragma once

#include <vector>
#include <cassert>
#include <algorithm>
#include <Storage/CustomDevice.h>
#include <esp_spi_flash.h>
#include <Print.h>

class FlashEmulator : public Storage::CustomDevice
{
public:
	struct Stat {
		size_t readOps;
		size_t writeOps;
		size_t readBytes;
		size_t writeBytes;
		size_t eraseOps;
		size_t totalTime;
	};

	FlashEmulator(size_t sectorCount)
	{
		resize(sectorCount);
	}

	FlashEmulator(const char* filename)
	{
		load(filename);
	}

	FlashEmulator(const FlashString& fstr)
	{
		load(fstr);
	}

	~FlashEmulator()
	{
		delete[] mData;
		delete[] mEraseCnt;
	}

	String getName() const override
	{
		return F("FlashEmulator");
	}

	size_t getBlockSize() const override
	{
		return SPI_FLASH_SEC_SIZE;
	}

	size_t getSize() const override
	{
		return mSize;
	}

	Type getType() const override
	{
		return Type::sysmem;
	}

	bool read(uint32_t address, void* buffer, size_t len) override;
	bool write(uint32_t address, const void* data, size_t len) override;
	bool erase_range(uint32_t address, size_t len) override;

	void randomize()
	{
		os_get_random(mData, mSize);
	}

	size_t getSectorCount() const
	{
		return mSize / SPI_FLASH_SEC_SIZE;
	}

	const uint8_t* bytes() const
	{
		return mData;
	}

	const uint32_t* words() const
	{
		return reinterpret_cast<const uint32_t*>(mData);
	}

	void load(const char* filename);
	void load(const FlashString& fstr);
	void save(const char* filename);

	void clearStats()
	{
		mStat = Stat{};
	}

	const Stat& stat() const
	{
		return mStat;
	}

	/**
	 * @brief Fail write operation after given number of WORDs
	 */
	void failAfter(uint32_t count)
	{
		mFailCountdown = 1 + count;
	}

	size_t getSectorEraseCount(uint32_t sector) const
	{
		assert(sector < getSectorCount());
		return mEraseCnt[sector];
	}

	void printTo(Print& p, const char* msg);

	void reset();

protected:
	void resize(size_t sectorCount);
	static size_t getReadOpTime(uint32_t bytes);
	static size_t getWriteOpTime(uint32_t bytes);
	static size_t getEraseOpTime();

	uint8_t* mData{nullptr};
	uint32_t* mEraseCnt{nullptr};
	size_t mSize{0};

	mutable Stat mStat{};

	size_t mFailCountdown{0};
};
