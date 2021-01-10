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

#include "FlashEmulator.h"

// timing data for ESP8266, 160MHz CPU frequency, 80MHz flash requency
// all values in microseconds
// values are for block sizes starting at 4 bytes and going up to 4096 bytes
static constexpr size_t readTimes[]{7, 5, 6, 7, 11, 18, 32, 60, 118, 231, 459};
static constexpr size_t writeTimes[]{19, 23, 35, 57, 106, 205, 417, 814, 1622, 3200, 6367};
static constexpr size_t blockEraseTime{37142};

static size_t timeInterp(uint32_t bytes, const size_t lut[])
{
	const int lut_size = sizeof(readTimes) / sizeof(readTimes[0]);
	int lz = __builtin_clz(bytes / 4);
	int log_size = 32 - lz;
	size_t x2 = 1 << (log_size + 2);
	size_t y2 = lut[std::min(log_size, lut_size - 1)];
	size_t x1 = 1 << (log_size + 1);
	size_t y1 = lut[log_size - 1];
	return (bytes - x1) * (y2 - y1) / (x2 - x1) + y1;
}

size_t FlashEmulator::getReadOpTime(uint32_t bytes)
{
	return timeInterp(bytes, readTimes);
}

size_t FlashEmulator::getWriteOpTime(uint32_t bytes)
{
	return timeInterp(bytes, writeTimes);
}

size_t FlashEmulator::getEraseOpTime()
{
	return blockEraseTime;
}

bool FlashEmulator::read(uint32_t address, void* buffer, size_t len)
{
	if(address > mSize || address + len > mSize) {
		debug_w("invalid flash operation detected: read 0x%08x, 0x%08x", address, len);
		return false;
	}

	memcpy(buffer, mData + address, len);

	++mStat.readOps;
	mStat.readBytes += len;
	mStat.totalTime += getReadOpTime(len);
	return true;
}

bool FlashEmulator::write(uint32_t address, const void* data, size_t len)
{
	if(address > mSize || address + len > mSize) {
		debug_w("invalid flash operation detected: write 0x%08x, 0x%08x", address, len);
		return false;
	}

	assert(address % 4 == 0);
	assert(len % 4 == 0);

	for(size_t i = 0; i < len; i += 4) {
		if(mFailCountdown != 0 && mFailCountdown-- == 1) {
			return false;
		}

		uint32_t sv;
		memcpy(&sv, &static_cast<const uint8_t*>(data)[i], 4);
		size_t pos = address + i;
		uint32_t& dv = *reinterpret_cast<uint32_t*>(&mData[pos]);

		if(((~dv) & sv) != 0) { // are we trying to set some 0 bits to 1?
			debug_w("invalid flash operation detected: dst = 0x%08x, size = 0x%04x, i = %u", pos, len, i);
			return false;
		}

		dv = sv;
	}

	++mStat.writeOps;
	mStat.writeBytes += len;
	mStat.totalTime += getWriteOpTime(len);
	return true;
}

bool FlashEmulator::erase_range(uint32_t address, size_t len)
{
	if(address % SPI_FLASH_SEC_SIZE != 0 || len % SPI_FLASH_SEC_SIZE != 0) {
		debug_e("Flash erase alignment error: 0x%08x, 0x%08x", address, len);
		return false;
	}

	size_t endAddress = address + len;
	if(address >= mSize || endAddress > mSize) {
		debug_w("invalid flash operation detected: erase 0x%08x, 0x%08x", address, len);
		return false;
	}

	if(mFailCountdown != 0 && mFailCountdown-- == 1) {
		return false;
	}

	std::fill_n(mData + address, len, 0xff);

	auto sector = address / SPI_FLASH_SEC_SIZE;
	auto sectorCount = len / SPI_FLASH_SEC_SIZE;
	for(unsigned i = 0; i < sectorCount; ++i) {
		++mEraseCnt[sector++];
	}
	mStat.eraseOps += sectorCount;
	mStat.totalTime += sectorCount * getEraseOpTime();
	return true;
}

void FlashEmulator::load(const char* filename)
{
	FILE* f = fopen(filename, "rb");
	fseek(f, 0, SEEK_END);
	auto size = ftell(f);
	assert(size % SPI_FLASH_SEC_SIZE == 0);
	// At least one page should be free
	size += SPI_FLASH_SEC_SIZE;
	size_t n_sectors = size / SPI_FLASH_SEC_SIZE;
	resize(n_sectors);
	fseek(f, 0, SEEK_SET);
	auto s = fread(mData, SPI_FLASH_SEC_SIZE, n_sectors, f);
	assert(s == n_sectors);
	fclose(f);
}

void FlashEmulator::load(const FlashString& fstr)
{
	auto size = fstr.length();
	assert(size % SPI_FLASH_SEC_SIZE == 0);
	// At least one page should be free
	size += SPI_FLASH_SEC_SIZE;
	auto n_sectors = size / SPI_FLASH_SEC_SIZE;
	resize(n_sectors);
	fstr.readFlash(0, reinterpret_cast<char*>(mData), size);
}

void FlashEmulator::save(const char* filename)
{
	FILE* f = fopen(filename, "wb");
	auto n_sectors = getSectorCount();
	auto s = fwrite(mData, SPI_FLASH_SEC_SIZE, n_sectors, f);
	assert(s == n_sectors);
	fclose(f);
}

void FlashEmulator::printTo(Print& p, const char* msg)
{
	p.print(_F("Time to "));
	p.print(msg);
	p.print(": ");
	p.print(mStat.totalTime);
	p.print(" us (");
	p.print(mStat.eraseOps);
	p.print("E ");
	p.print(mStat.writeOps);
	p.print("W ");
	p.print(mStat.readOps);
	p.print("R ");
	p.print(mStat.writeBytes);
	p.print("Wb ");
	p.print(mStat.readBytes);
	p.print("Rb)");
	p.println();
}

void FlashEmulator::reset()
{
	std::fill_n(mData, mSize, 0xff);
	std::fill_n(mEraseCnt, getSectorCount(), 0);
	clearStats();
	mFailCountdown = 0;
}

void FlashEmulator::resize(size_t sectorCount)
{
	delete mData;
	delete mEraseCnt;
	mSize = sectorCount * SPI_FLASH_SEC_SIZE;
	mData = new uint8_t[mSize];
	mEraseCnt = new uint32_t[sectorCount];
	reset();
}
