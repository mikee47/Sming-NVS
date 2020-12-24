// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <Nvs/PartitionManager.hpp>
#include "RandomTest.hpp"
#include <Data/Stream/MemoryDataStream.h>
#include <NvsTest.h>

#define TEST_ESP_ERR(rc, res) CHECK_EQ(rc, res)
#define TEST_ESP_OK(rc) CHECK_EQ(rc, ESP_OK)

using namespace nvs;

#define IMPORT_TESTDATA(name) IMPORT_FSTR_LOCAL(name, PROJECT_DIR "/testData/" #name ".bin")

namespace testData
{
IMPORT_TESTDATA(part_old_blob_format)
IMPORT_TESTDATA(singlepage_blob)
IMPORT_TESTDATA(singlepage_blob_partition)
IMPORT_TESTDATA(multipage_blob)
IMPORT_TESTDATA(multipage_blob_partition)
IMPORT_TESTDATA(test_1_v1)
IMPORT_TESTDATA(test_1_v1_partition)
IMPORT_TESTDATA(test_1_v2)
IMPORT_TESTDATA(test_1_v2_partition)

} // namespace testData

class NvsTest : public TestGroup
{
public:
	NvsTest() : TestGroup(_F("NVS"))
	{
	}

	void dumpBytes(const uint8_t* data, size_t count)
	{
		for(uint32_t i = 0; i < count; ++i) {
			if(i % 32 == 0) {
				m_printf("%08x    ", i);
			}
			m_printf("%02x ", data[i]);
			if((i + 1) % 32 == 0) {
				m_printf("\n");
			}
		}
	}

	void execute() override
	{
		TEST_CASE("crc32 behaves as expected")
		{
			Item item1;
			item1.datatype = ItemType::I32;
			item1.nsIndex = 1;
			item1.crc32 = 0;
			item1.chunkIndex = 0xff;
			memset(item1.key, 0xbb, sizeof(item1.key));
			memset(item1.data, 0xaa, sizeof(item1.data));

			auto crc32_1 = item1.calculateCrc32();

			Item item2 = item1;
			item2.crc32 = crc32_1;

			CHECK(crc32_1 == item2.calculateCrc32());

			item2 = item1;
			item2.nsIndex = 2;
			CHECK(crc32_1 != item2.calculateCrc32());

			item2 = item1;
			item2.datatype = ItemType::U32;
			CHECK(crc32_1 != item2.calculateCrc32());

			item2 = item1;
			strncpy(item2.key, "foo", Item::MAX_KEY_LENGTH);
			CHECK(crc32_1 != item2.calculateCrc32());
		}

		TEST_CASE("Page starting with empty flash is in uninitialized state")
		{
			PartitionEmulationFixture f;
			Page page;
			CHECK(page.state() == Page::PageState::INVALID);
			CHECK(page.load(f.part, 0) == ESP_OK);
			CHECK(page.state() == Page::PageState::UNINITIALIZED);
		}

		TEST_CASE("Page can distinguish namespaces")
		{
			PartitionEmulationFixture f;
			Page page;
			CHECK(page.load(f.part, 0) == ESP_OK);
			int32_t val1 = 0x12345678;
			CHECK(page.writeItem(1, ItemType::I32, "intval1", &val1, sizeof(val1)) == ESP_OK);
			int32_t val2 = 0x23456789;
			CHECK(page.writeItem(2, ItemType::I32, "intval1", &val2, sizeof(val2)) == ESP_OK);

			int32_t readVal;
			CHECK(page.readItem(2, ItemType::I32, "intval1", &readVal, sizeof(readVal)) == ESP_OK);
			CHECK(readVal == val2);
		}

		TEST_CASE("Page reading with different type causes type mismatch error")
		{
			PartitionEmulationFixture f;
			Page page;
			CHECK(page.load(f.part, 0) == ESP_OK);
			int32_t val = 0x12345678;
			CHECK(page.writeItem(1, ItemType::I32, "intval1", &val, sizeof(val)) == ESP_OK);
			CHECK(page.readItem(1, ItemType::U32, "intval1", &val, sizeof(val)) == ESP_ERR_NVS_TYPE_MISMATCH);
		}

		TEST_CASE("Page when erased, it's state becomes UNITIALIZED")
		{
			PartitionEmulationFixture f;
			Page page;
			CHECK(page.load(f.part, 0) == ESP_OK);
			int32_t val = 0x12345678;
			CHECK(page.writeItem(1, ItemType::I32, "intval1", &val, sizeof(val)) == ESP_OK);
			CHECK(page.erase() == ESP_OK);
			CHECK(page.state() == Page::PageState::UNINITIALIZED);
		}

		TEST_CASE("Page when writing and erasing, used/erased counts are updated correctly")
		{
			PartitionEmulationFixture f;
			Page page;
			CHECK(page.load(f.part, 0) == ESP_OK);
			CHECK(page.getUsedEntryCount() == 0);
			CHECK(page.getErasedEntryCount() == 0);
			uint32_t foo1 = 0;
			CHECK(page.writeItem(1, "foo1", foo1) == ESP_OK);
			CHECK(page.getUsedEntryCount() == 1);
			CHECK(page.writeItem(2, "foo1", foo1) == ESP_OK);
			CHECK(page.getUsedEntryCount() == 2);
			CHECK(page.eraseItem<uint32_t>(2, "foo1") == ESP_OK);
			CHECK(page.getUsedEntryCount() == 1);
			CHECK(page.getErasedEntryCount() == 1);
			for(size_t i = 0; i < Page::ENTRY_COUNT - 2; ++i) {
				char name[16];
				m_snprintf(name, sizeof(name), "i%ld", (long int)i);
				CHECK(page.writeItem(1, name, i) == ESP_OK);
			}
			CHECK(page.getUsedEntryCount() == Page::ENTRY_COUNT - 1);
			CHECK(page.getErasedEntryCount() == 1);
			for(size_t i = 0; i < Page::ENTRY_COUNT - 2; ++i) {
				char name[16];
				m_snprintf(name, sizeof(name), "i%ld", (long int)i);
				CHECK(page.eraseItem(1, itemTypeOf<size_t>(), name) == ESP_OK);
			}
			CHECK(page.getUsedEntryCount() == 1);
			CHECK(page.getErasedEntryCount() == Page::ENTRY_COUNT - 1);
		}

		TEST_CASE("Page when page is full, adding an element fails")
		{
			PartitionEmulationFixture f;
			Page page;
			CHECK(page.load(f.part, 0) == ESP_OK);
			for(size_t i = 0; i < Page::ENTRY_COUNT; ++i) {
				char name[16];
				m_snprintf(name, sizeof(name), "i%ld", (long int)i);
				CHECK(page.writeItem(1, name, i) == ESP_OK);
			}
			CHECK(page.writeItem(1, "foo", 64UL) == ESP_ERR_NVS_PAGE_FULL);
		}

		TEST_CASE("Page maintains its seq number")
		{
			PartitionEmulationFixture f;
			{
				Page page;
				CHECK(page.load(f.part, 0) == ESP_OK);
				CHECK(page.setSeqNumber(123) == ESP_OK);
				int32_t val = 42;
				CHECK(page.writeItem(1, ItemType::I32, "dummy", &val, sizeof(val)) == ESP_OK);
			}
			{
				Page page;
				CHECK(page.load(f.part, 0) == ESP_OK);
				uint32_t seqno;
				CHECK(page.getSeqNumber(seqno) == ESP_OK);
				CHECK(seqno == 123);
			}
		}

		TEST_CASE("Page can write and read variable length data")
		{
			PartitionEmulationFixture f;
			Page page;
			CHECK(page.load(f.part, 0) == ESP_OK);
			const char str[] = "foobar1234foobar1234foobar1234foobar1234foobar1234foobar1234foobar1234foobar1234";
			size_t len = strlen(str);
			CHECK(page.writeItem(1, "stuff1", 42) == ESP_OK);
			CHECK(page.writeItem(1, "stuff2", 1) == ESP_OK);
			CHECK(page.writeItem(1, ItemType::SZ, "foobaar", str, len + 1) == ESP_OK);
			CHECK(page.writeItem(1, "stuff3", 2) == ESP_OK);
			CHECK(page.writeItem(1, ItemType::BLOB, "baz", str, len) == ESP_OK);
			CHECK(page.writeItem(1, "stuff4", 0x7abbccdd) == ESP_OK);

			char buf[sizeof(str) + 16];
			int32_t value;
			CHECK(page.readItem(1, "stuff1", value) == ESP_OK);
			CHECK(value == 42);
			CHECK(page.readItem(1, "stuff2", value) == ESP_OK);
			CHECK(value == 1);
			CHECK(page.readItem(1, "stuff3", value) == ESP_OK);
			CHECK(value == 2);
			CHECK(page.readItem(1, "stuff4", value) == ESP_OK);
			CHECK(value == 0x7abbccdd);

			memset(buf, 0xff, sizeof(buf));
			CHECK(page.readItem(1, ItemType::SZ, "foobaar", buf, sizeof(buf)) == ESP_OK);
			CHECK(memcmp(buf, str, strlen(str) + 1) == 0);

			memset(buf, 0xff, sizeof(buf));
			CHECK(page.readItem(1, ItemType::BLOB, "baz", buf, sizeof(buf)) == ESP_OK);
			CHECK(memcmp(buf, str, strlen(str)) == 0);
		}

		TEST_CASE("Page different key names are distinguished even if the pointer is the same")
		{
			PartitionEmulationFixture f;
			Page page;
			TEST_ESP_OK(page.load(f.part, 0));
			TEST_ESP_OK(page.writeItem(1, "i1", 1));
			TEST_ESP_OK(page.writeItem(1, "i2", 2));
			int32_t value;
			char keyname[10] = {0};
			for(int i = 0; i < 2; ++i) {
				strncpy(keyname, "i1", sizeof(keyname) - 1);
				TEST_ESP_OK(page.readItem(1, keyname, value));
				CHECK(value == 1);
				strncpy(keyname, "i2", sizeof(keyname) - 1);
				TEST_ESP_OK(page.readItem(1, keyname, value));
				CHECK(value == 2);
			}
		}

		TEST_CASE("Page validates key size")
		{
			PartitionEmulationFixture f(0, 4);
			Page page;
			TEST_ESP_OK(page.load(f.part, 0));
			// 16-character key fails
			TEST_ESP_ERR(page.writeItem(1, "0123456789123456", 1), ESP_ERR_NVS_KEY_TOO_LONG);
			// 15-character key is okay
			TEST_ESP_OK(page.writeItem(1, "012345678912345", 1));
		}

		TEST_CASE("Page validates blob size")
		{
			PartitionEmulationFixture f(0, 4);
			Page page;
			TEST_ESP_OK(page.load(f.part, 0));

			char buf[4096] = {0};
			// There are two potential errors here:
			// - not enough space in the page (because one value has been written already)
			// - value is too long
			// Check that the second one is actually returned.
			TEST_ESP_ERR(page.writeItem(1, ItemType::BLOB, "2", buf, Page::ENTRY_COUNT * Page::ENTRY_SIZE),
						 ESP_ERR_NVS_VALUE_TOO_LONG);
			// Should fail as well
			TEST_ESP_ERR(page.writeItem(1, ItemType::BLOB, "2", buf, Page::CHUNK_MAX_SIZE + 1),
						 ESP_ERR_NVS_VALUE_TOO_LONG);
			TEST_ESP_OK(page.writeItem(1, ItemType::BLOB, "2", buf, Page::CHUNK_MAX_SIZE));
		}

		TEST_CASE("Page handles invalid CRC of variable length items", "[nvs][cur]")
		{
			PartitionEmulationFixture f(0, 4);
			{
				Page page;
				TEST_ESP_OK(page.load(f.part, 0));
				char buf[128] = {0};
				TEST_ESP_OK(page.writeItem(1, ItemType::BLOB, "1", buf, sizeof(buf)));
			}
			// corrupt header of the item (64 is the offset of the first item in page)
			uint32_t overwrite_buf = 0;
			f.part.write(64, &overwrite_buf, 4);
			// load page again
			{
				Page page;
				TEST_ESP_OK(page.load(f.part, 0));
			}
		}

		TEST_CASE("HashList is cleaned up as soon as items are erased")
		{
			HashList hashlist;
			// Add items
			const size_t count = 128;
			for(size_t i = 0; i < count; ++i) {
				char key[16];
				m_snprintf(key, sizeof(key), "i%ld", (long int)i);
				Item item(1, ItemType::U32, 1, key);
				hashlist.insert(item, i);
			}
			debug_i("Added %u items, %u blocks", count, hashlist.getBlockCount());
			// Remove them in reverse order
			for(size_t i = count; i > 0; --i) {
				hashlist.erase(i - 1, true);
			}
			CHECK(hashlist.getBlockCount() == 0);
			// Add again
			for(size_t i = 0; i < count; ++i) {
				char key[16];
				m_snprintf(key, sizeof(key), "i%ld", (long int)i);
				Item item(1, ItemType::U32, 1, key);
				hashlist.insert(item, i);
			}
			debug_i("Added %u items, %u blocks", count, hashlist.getBlockCount());
			// Remove them in the same order
			for(size_t i = 0; i < count; ++i) {
				hashlist.erase(i, true);
			}
			CHECK(hashlist.getBlockCount() == 0);
		}

		TEST_CASE("can init PageManager in empty flash")
		{
			PartitionEmulationFixture f(0, 4);
			PageManager pm;
			CHECK(pm.load(f.part) == ESP_OK);
		}

		TEST_CASE("PageManager adds page in the correct order")
		{
			const size_t pageCount = 8;
			PartitionEmulationFixture f(0, pageCount);
			uint32_t pageNo[pageCount] = {-1U, 50, 11, -1U, 23, 22, 24, 49};

			for(uint32_t i = 0; i < pageCount; ++i) {
				Page p;
				p.load(f.part, i);
				if(pageNo[i] != -1U) {
					p.setSeqNumber(pageNo[i]);
					p.writeItem(1, "foo", 10U);
				}
			}

			PageManager pageManager;
			CHECK(pageManager.load(f.part) == ESP_OK);

			uint32_t lastSeqNo = 0;
			for(auto it = std::begin(pageManager); it != std::end(pageManager); ++it) {
				uint32_t seqNo;
				CHECK(it->getSeqNumber(seqNo) == ESP_OK);
				CHECK(seqNo > lastSeqNo);
			}
		}

		TEST_CASE("can init container in empty flash")
		{
			PartitionEmulationFixture f(0, 4);
			Container container(f.part.ptr());

			debug_i("before check");
			ElapseTimer timer;
			CHECK(container.init());
			debug_i("Time to init empty container (4 sectors): %s", timer.elapsedTime().toString().c_str());
		}

		TEST_CASE("container doesn't add duplicates within one page")
		{
			PartitionEmulationFixture f(0, 4);
			Container container(f.part.ptr());
			CHECK(container.init());

			int bar = 0;
			CHECK(container.writeItem(1, "bar", ++bar));
			CHECK(container.writeItem(1, "bar", ++bar));

			Page page;
			page.load(f.part, 0);
			CHECK(page.getUsedEntryCount() == 1);
			CHECK(page.getErasedEntryCount() == 1);
		}

		TEST_CASE("can write one item a thousand times")
		{
			PartitionEmulationFixture f(4, 4);
			Container container(f.part.ptr());
			CHECK(container.init());

			for(size_t i = 0; i < Page::ENTRY_COUNT * 4 * 2; ++i) {
				CHECK(container.writeItem(1, "i", int(i)));
			}

			f.emu.printTo(s_perf, _F("write one item a thousand times"));
		}

		TEST_CASE("container doesn't add duplicates within multiple pages")
		{
			PartitionEmulationFixture f(0, 4);
			Container container(f.part.ptr());
			CHECK(container.init());

			int bar = 0;
			CHECK(container.writeItem(1, "bar", ++bar));
			for(size_t i = 0; i < Page::ENTRY_COUNT; ++i) {
				CHECK(container.writeItem(1, "foo", int(++bar)));
			}
			CHECK(container.writeItem(1, "bar", ++bar));

			Page page;
			page.load(f.part, 0);
			CHECK(page.findItem(1, itemTypeOf<int>(), "bar") == ESP_ERR_NVS_NOT_FOUND);
			page.load(f.part, 1);
			CHECK(page.findItem(1, itemTypeOf<int>(), "bar") == ESP_OK);
		}

		TEST_CASE("container can find items on second page if first is not fully written and has cached search data")
		{
			PartitionEmulationFixture f(0, 3);
			Container container(f.part.ptr());
			CHECK(container.init());

			uint8_t bigdata[(Page::CHUNK_MAX_SIZE - Page::ENTRY_SIZE) / 2] = {0};
			// write one big chunk of data
			REQUIRE(container.writeItem(0, ItemType::BLOB, "1", bigdata, sizeof(bigdata)));
			// write another big chunk of data
			REQUIRE(container.writeItem(0, ItemType::BLOB, "2", bigdata, sizeof(bigdata)));

			// write third one; it will not fit into the first page
			REQUIRE(container.writeItem(0, ItemType::BLOB, "3", bigdata, sizeof(bigdata)));

			size_t size;
			REQUIRE(container.getItemDataSize(0, ItemType::BLOB, "1", size));
			CHECK(size == sizeof(bigdata));
			REQUIRE(container.getItemDataSize(0, ItemType::BLOB, "3", size));
			CHECK(size == sizeof(bigdata));
		}

		TEST_CASE("can write and read variable length data lots of times")
		{
			PartitionEmulationFixture f(0, 4);
			Container container(f.part.ptr());
			CHECK(container.init());

			const char str[] = "foobar1234foobar1234foobar1234foobar1234foobar1234foobar1234foobar1234foobar1234";
			char buf[sizeof(str) + 16];
			size_t len = strlen(str);
			for(size_t i = 0; i < Page::ENTRY_COUNT * 4 * 2; ++i) {
				//				CAPTURE(i);
				CHECK(container.writeItem(1, ItemType::SZ, "foobaar", str, len + 1));
				CHECK(container.writeItem(1, "foo", uint32_t(i)));

				uint32_t value;
				CHECK(container.readItem(1, "foo", value));
				CHECK(value == i);

				memset(buf, 0xff, sizeof(buf));
				CHECK(container.readItem(1, ItemType::SZ, "foobaar", buf, sizeof(buf)));
				CHECK(strcmp(buf, str) == 0);
			}
			f.emu.printTo(s_perf, _F("write one string and one integer a thousand times"));
		}

		TEST_CASE("can get length of variable length data")
		{
			PartitionEmulationFixture f(0, 4);
			f.emu.randomize();
			Container container(f.part.ptr());
			CHECK(container.init());

			PSTR_ARRAY(str, "foobar1234foobar1234foobar1234foobar1234foobar1234foobar1234foobar1234foobar1234");
			size_t len = strlen(str);
			CHECK(container.writeItem(1, ItemType::SZ, "foobaar", str, len + 1));
			size_t dataSize;
			CHECK(container.getItemDataSize(1, ItemType::SZ, "foobaar", dataSize));
			CHECK(dataSize == len + 1);

			CHECK(container.writeItem(2, ItemType::BLOB, "foobaar", str, len));
			CHECK(container.getItemDataSize(2, ItemType::BLOB, "foobaar", dataSize));
			CHECK(dataSize == len);
		}

		TEST_CASE("can create namespaces")
		{
			PartitionEmulationFixture f(0, 4);
			Container container(f.part.ptr());
			CHECK(container.init());

			uint8_t nsi;
			CHECK(!container.createOrOpenNamespace("wifi", false, nsi));
			TEST_ESP_ERR(nvs_errno, ESP_ERR_NVS_NOT_FOUND);

			CHECK(container.createOrOpenNamespace("wifi", true, nsi));
			Page page;
			page.load(f.part, 0);
			CHECK(page.findItem(Page::NS_INDEX, ItemType::U8, "wifi") == ESP_OK);
		}

		TEST_CASE("container may become full")
		{
			PartitionEmulationFixture f(0, 4);
			Container container(f.part.ptr());
			CHECK(container.init());

			for(size_t i = 0; i < Page::ENTRY_COUNT * 3; ++i) {
				char name[Item::MAX_KEY_LENGTH + 1];
				m_snprintf(name, sizeof(name), "key%05d", int(i));
				CHECK(container.writeItem(1, name, int(i)));
			}
			REQUIRE(!container.writeItem(1, "foo", 10));
			TEST_ESP_ERR(nvs_errno, ESP_ERR_NVS_NOT_ENOUGH_SPACE);
		}

		TEST_CASE("can modify an item on a page which will be erased")
		{
			PartitionEmulationFixture f(0, 2);
			Container container(f.part.ptr());
			CHECK(container.init());

			for(size_t i = 0; i < Page::ENTRY_COUNT * 3 + 1; ++i) {
				CHECK(container.writeItem(1, "foo", 42U));
			}
		}

		TEST_CASE("erase operations are distributed among sectors")
		{
			const size_t sectors = 6;
			PartitionEmulationFixture f(0, sectors);
			Container container(f.part.ptr());
			CHECK(container.init());

			/* Fill some part of container with static values */
			const size_t static_sectors = 2;
			for(size_t i = 0; i < static_sectors * Page::ENTRY_COUNT; ++i) {
				char name[Item::MAX_KEY_LENGTH];
				m_snprintf(name, sizeof(name), "static%d", (int)i);
				CHECK(container.writeItem(1, name, i));
			}

			/* Now perform many write operations */
			const size_t write_ops = 2000;
			for(size_t i = 0; i < write_ops; ++i) {
				CHECK(container.writeItem(1, "value", i));
			}

			/* Check that erase counts are distributed between the remaining sectors */
			const size_t max_erase_cnt = write_ops / Page::ENTRY_COUNT / (sectors - static_sectors) + 1;
			for(size_t i = 0; i < sectors; ++i) {
				auto erase_cnt = f.emu.getSectorEraseCount(i);
				debug_i("Sector %u erased %u times", i, erase_cnt);
				CHECK(erase_cnt <= max_erase_cnt);
			}
		}

		TEST_CASE("can erase items")
		{
			PartitionEmulationFixture f(0, 3);
			Container container(f.part.ptr());
			CHECK(container.init());

			for(size_t i = 0; i < Page::ENTRY_COUNT * 2 - 3; ++i) {
				char name[Item::MAX_KEY_LENGTH + 1];
				m_snprintf(name, sizeof(name), "key%05d", int(i));
				CHECK(container.writeItem(3, name, int(i)));
			}
			CHECK(container.writeItem(1, "foo", 32));
			CHECK(container.writeItem(2, "foo", 64));
			CHECK(container.eraseItem(2, "foo"));
			int val;
			CHECK(container.readItem(1, "foo", val));
			CHECK(val == 32);
			CHECK(container.eraseNamespace(3));
			CHECK(!container.readItem(2, "foo", val));
			TEST_ESP_ERR(nvs_errno, ESP_ERR_NVS_NOT_FOUND);
			CHECK(!container.readItem(3, "key00222", val));
			TEST_ESP_ERR(nvs_errno, ESP_ERR_NVS_NOT_FOUND);
		}

		TEST_CASE("namespace name is deep copy")
		{
			DEFINE_FSTR_LOCAL(ns_name, "const_name");

			const uint32_t NVS_FLASH_SECTOR = 6;
			const uint32_t NVS_FLASH_SECTOR_COUNT_MIN = 3;
			PartitionEmulationFixture f(NVS_FLASH_SECTOR, NVS_FLASH_SECTOR_COUNT_MIN);

			auto container = partitionManager.openContainer(f.part.ptr());
			CHECK(container != nullptr);

			auto handle_1 = container->openHandle(ns_name, NVS_READWRITE);

			auto handle_2 = container->openHandle("just_kidding", NVS_READONLY);
			CHECK(!handle_2);
			TEST_ESP_ERR(nvs_errno, ESP_ERR_NVS_NOT_FOUND);

			handle_1.reset();
			handle_2.reset();

			delete container;
		}

		TEST_CASE("readonly handle fails on writing")
		{
			PartitionEmulationFixture f(0, 10);
			const char* str = "value 0123456789abcdef0123456789abcdef";
			const uint8_t blob[8] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7};

			auto container = partitionManager.openContainer(f.part.ptr());
			REQUIRE(container != nullptr);

			// first, creating namespace...
			auto handle_1 = container->openHandle("ro_ns", NVS_READWRITE);
			handle_1.reset();

			handle_1 = container->openHandle("ro_ns", NVS_READONLY);
			CHECK(handle_1);
			REQUIRE(!handle_1->setItem(ItemType::I32, "key", 47) && nvs_errno == ESP_ERR_NVS_READ_ONLY);
			REQUIRE(!handle_1->setString("key", str) && nvs_errno == ESP_ERR_NVS_READ_ONLY);
			REQUIRE(!handle_1->setBlob("key", blob, 8) && nvs_errno == ESP_ERR_NVS_READ_ONLY);

			handle_1.reset();

			// without deinit it affects "nvs api tests"
			delete container;
		}

		TEST_CASE("nvs api tests")
		{
			PartitionEmulationFixture f(6, 3);
			f.emu.randomize();

			nvs_handle_t handle_1;
			TEST_ESP_ERR(nvs_open("namespace1", NVS_READONLY, &handle_1), ESP_ERR_NVS_NOT_INITIALIZED);

			REQUIRE(partitionManager.openContainer(f.part.ptr()));

			TEST_ESP_OK(nvs_open("namespace1", NVS_READWRITE, &handle_1));
			TEST_ESP_OK(nvs_set_i32(handle_1, "foo", 0x12345678));
			TEST_ESP_OK(nvs_set_i32(handle_1, "foo", 0x23456789));

			nvs_handle_t handle_2;
			TEST_ESP_OK(nvs_open("namespace2", NVS_READWRITE, &handle_2));
			TEST_ESP_OK(nvs_set_i32(handle_2, "foo", 0x3456789a));
			PSTR_ARRAY(str, "value 0123456789abcdef0123456789abcdef");
			TEST_ESP_OK(nvs_set_str(handle_2, "key", str));

			int32_t v1;
			TEST_ESP_OK(nvs_get_i32(handle_1, "foo", &v1));
			CHECK(0x23456789 == v1);

			int32_t v2;
			TEST_ESP_OK(nvs_get_i32(handle_2, "foo", &v2));
			CHECK(0x3456789a == v2);

			char buf[strlen(str) + 1];
			size_t buf_len = sizeof(buf);

			size_t buf_len_needed;
			TEST_ESP_OK(nvs_get_str(handle_2, "key", nullptr, &buf_len_needed));
			CHECK(buf_len_needed == buf_len);

			size_t buf_len_short = buf_len - 1;
			TEST_ESP_ERR(ESP_ERR_NVS_INVALID_LENGTH, nvs_get_str(handle_2, "key", buf, &buf_len_short));
			CHECK(buf_len_short == buf_len);

			size_t buf_len_long = buf_len + 1;
			TEST_ESP_OK(nvs_get_str(handle_2, "key", buf, &buf_len_long));
			CHECK(buf_len_long == buf_len);

			TEST_ESP_OK(nvs_get_str(handle_2, "key", buf, &buf_len));

			CHECK(0 == strcmp(buf, str));
			nvs_close(handle_1);
			nvs_close(handle_2);

			REQUIRE(partitionManager.closeContainer(NVS_DEFAULT_PART_NAME));
		}

		TEST_CASE("nvs iterators tests")
		{
			PartitionEmulationFixture f(0, 5);
			nvs::Container* container{};
			DEFINE_FSTR_LOCAL(name_1, "namespace1");
			DEFINE_FSTR_LOCAL(name_2, "namespace2");
			DEFINE_FSTR_LOCAL(name_3, "namespace3");
			nvs::HandlePtr handle_1, handle_2;

			auto begin = [&]() {
				f.emu.reset();
				container = partitionManager.openContainer(f.part.ptr());
				REQUIRE(container != nullptr);

				constexpr uint32_t blob{0x11223344};
				handle_1 = container->openHandle(name_1, NVS_READWRITE);
				REQUIRE(handle_1);
				handle_2 = container->openHandle(name_2, NVS_READWRITE);
				REQUIRE(handle_2);

				REQUIRE(handle_1->setItem(ItemType::I8, "value1", -11));
				REQUIRE(handle_1->setItem(ItemType::U8, "value2", 11));
				REQUIRE(handle_1->setItem(ItemType::I16, "value3", 1234));
				REQUIRE(handle_1->setItem(ItemType::U16, "value4", -1234));
				REQUIRE(handle_1->setItem(ItemType::I32, "value5", -222));
				REQUIRE(handle_1->setItem(ItemType::I32, "value6", -222));
				REQUIRE(handle_1->setItem(ItemType::I32, "value7", -222));
				REQUIRE(handle_1->setItem(ItemType::U32, "value8", 222));
				REQUIRE(handle_1->setItem(ItemType::U32, "value9", 222));
				REQUIRE(handle_1->setString("value10", "foo"));
				REQUIRE(handle_1->setBlob("value11", &blob, sizeof(blob)));
				REQUIRE(handle_2->setItem(ItemType::I32, "value1", -111));
				REQUIRE(handle_2->setItem(ItemType::I32, "value2", -111));
				REQUIRE(handle_2->setItem(ItemType::I64, "value3", -555));
				REQUIRE(handle_2->setItem(ItemType::U64, "value4", 555));
			};

			auto end = [&]() {
				handle_1.reset();
				handle_2.reset();
				delete container;
				container = nullptr;
			};

			auto entry_count = [&container](const String& key, ItemType type) {
				return std::count(container->find(key, type), container->end(), true);
			};

#define SECTION(x) TEST_CASE(x)

			SECTION("Number of entries found for specified namespace and type is correct")
			{
				begin();

				CHECK_EQ(entry_count(nullptr, ItemType::ANY), 15);
				CHECK_EQ(entry_count(name_1, ItemType::ANY), 11);
				CHECK_EQ(entry_count(name_1, ItemType::I32), 3);
				CHECK_EQ(entry_count(nullptr, ItemType::I32), 5);
				CHECK_EQ(entry_count(nullptr, ItemType::U64), 1);

				end();
			}

			SECTION("New entry is not created when existing key-value pair is set")
			{
				begin();

				CHECK_EQ(entry_count(name_2, ItemType::ANY), 4);
				REQUIRE(handle_2->setItem(ItemType::I32, "value1", -222));
				CHECK_EQ(entry_count(name_2, ItemType::ANY), 4);

				end();
			}

			SECTION("Number of entries found decrease when entry is erased")
			{
				begin();

				CHECK_EQ(entry_count(nullptr, ItemType::U64), 1);
				CHECK(handle_2->eraseItem("value4"));
				CHECK_EQ(entry_count(nullptr, ItemType::U64), 0);

				end();
			}

			SECTION("All fields of nvs_entry_info_t structure are correct")
			{
				begin();

				auto it = container->find(name_1, ItemType::I32);
				CHECK(it);
				char key[] = "value5";
				for(; it; key[5]++, it++) {
					CHECK_EQ(it->nsName(), name_1);
					CHECK_EQ(it->key(), key);
					CHECK_EQ(it->type(), ItemType::I32);
				}

				end();
			}

			SECTION("Entry info is not affected by subsequent erase")
			{
				begin();

				auto it = container->find(name_1, ItemType::ANY);
				auto info = *it;
				REQUIRE(handle_1->eraseItem("value1"));
				REQUIRE(*it == info);

				end();
			}

			SECTION("Entry info is not affected by subsequent set")
			{
				begin();

				auto it = container->find(name_1, ItemType::ANY);
				auto info = *it;
				REQUIRE(handle_1->setItem(ItemType::U8, it->key(), 44));
				CHECK(*it == info);

				end();
			}

			SECTION("Iterating over multiple pages works correctly")
			{
				begin();

				constexpr int entries_created{250};

				auto handle_3 = container->openHandle(name_3, NVS_READWRITE);
				REQUIRE(handle_3);
				for(size_t i = 0; i < entries_created; i++) {
					CHECK(handle_3->setItem(ItemType::U8, String(i), 123));
				}
				handle_3.reset();

				int entries_found{0};
				auto it = container->find(name_3, ItemType::ANY);
				while(it) {
					entries_found++;
					it++;
				}
				CHECK_EQ(entries_created, entries_found);

				end();
			}

			SECTION("Iterating over multi-page blob works correctly")
			{
				begin();

				constexpr uint8_t multipage_blob[4096 * 2]{0};
				constexpr int NUMBER_OF_ENTRIES_PER_PAGE{125};
				size_t occupied_entries;

				auto handle_3 = container->openHandle(name_3, NVS_READWRITE);
				REQUIRE(handle_3);
				REQUIRE(handle_3->setBlob("blob", multipage_blob, sizeof(multipage_blob)));
				REQUIRE(handle_3->getUsedEntryCount(occupied_entries));
				handle_3.reset();

				CHECK(occupied_entries > NUMBER_OF_ENTRIES_PER_PAGE * 2);
				CHECK_EQ(entry_count(name_3, ItemType::BLOB_DATA), 1);

				end();
			}
		}

		TEST_CASE("Iterator with not matching type iterates correctly")
		{
			PartitionEmulationFixture f(0, 5);

			auto container = partitionManager.openContainer(f.part.ptr());
			REQUIRE(container);

			DEFINE_FSTR_LOCAL(NAMESPACE, "test_ns_4");

			// writing string to namespace (a type which spans multiple entries)
			auto my_handle = container->openHandle(NAMESPACE, NVS_READWRITE);
			REQUIRE(my_handle);
			REQUIRE(my_handle->setString("test-string", "InitString0"));
			REQUIRE(my_handle->commit());
			my_handle.reset();

			auto it = container->find(NAMESPACE, ItemType::I32);
			CHECK(!it);

			// re-init to trigger cleaning up of broken items -> a corrupted string will be erased
			delete container;
			container = partitionManager.openContainer(f.part.ptr());
			REQUIRE(container);

			auto it2 = container->find(NAMESPACE, ItemType::STR);
			CHECK(it2);

			// without deinit it affects "nvs api tests"
			delete container;
		}

		TEST_CASE("wifi test")
		{
			PartitionEmulationFixture f(5, 3);
			f.emu.randomize();

			auto container = partitionManager.openContainer(f.part.ptr());
			REQUIRE(container);

			nvs_handle_t misc_handle;
			TEST_ESP_OK(nvs_open("nvs.net80211", NVS_READWRITE, &misc_handle));
			char log[33];
			size_t log_size = sizeof(log);
			TEST_ESP_ERR(nvs_get_str(misc_handle, "log", log, &log_size), ESP_ERR_NVS_NOT_FOUND);
			strcpy(log, "foobarbazfizzz");
			TEST_ESP_OK(nvs_set_str(misc_handle, "log", log));
			nvs_close(misc_handle);

			nvs_handle_t net80211_handle;
			TEST_ESP_OK(nvs_open("nvs.net80211", NVS_READWRITE, &net80211_handle));

			uint8_t opmode = 2;
			TEST_ESP_ERR(nvs_get_u8(net80211_handle, "wifi.opmode", &opmode), ESP_ERR_NVS_NOT_FOUND);

			TEST_ESP_OK(nvs_set_u8(net80211_handle, "wifi.opmode", opmode));

			uint8_t country = 0;
			TEST_ESP_ERR(nvs_get_u8(net80211_handle, "wifi.country", &country), ESP_ERR_NVS_NOT_FOUND);
			TEST_ESP_OK(nvs_set_u8(net80211_handle, "wifi.country", country));

			char ssid[36];
			size_t size = sizeof(ssid);
			TEST_ESP_ERR(nvs_get_blob(net80211_handle, "sta.ssid", ssid, &size), ESP_ERR_NVS_NOT_FOUND);
			strcpy(ssid, "my android AP");
			TEST_ESP_OK(nvs_set_blob(net80211_handle, "sta.ssid", ssid, size));

			char mac[6];
			size = sizeof(mac);
			TEST_ESP_ERR(nvs_get_blob(net80211_handle, "sta.mac", mac, &size), ESP_ERR_NVS_NOT_FOUND);
			memset(mac, 0xab, 6);
			TEST_ESP_OK(nvs_set_blob(net80211_handle, "sta.mac", mac, size));

			uint8_t authmode = 1;
			TEST_ESP_ERR(nvs_get_u8(net80211_handle, "sta.authmode", &authmode), ESP_ERR_NVS_NOT_FOUND);
			TEST_ESP_OK(nvs_set_u8(net80211_handle, "sta.authmode", authmode));

			char pswd[65];
			size = sizeof(pswd);
			TEST_ESP_ERR(nvs_get_blob(net80211_handle, "sta.pswd", pswd, &size), ESP_ERR_NVS_NOT_FOUND);
			strcpy(pswd, "`123456788990-=");
			TEST_ESP_OK(nvs_set_blob(net80211_handle, "sta.pswd", pswd, size));

			char pmk[32];
			size = sizeof(pmk);
			TEST_ESP_ERR(nvs_get_blob(net80211_handle, "sta.pmk", pmk, &size), ESP_ERR_NVS_NOT_FOUND);
			memset(pmk, 1, size);
			TEST_ESP_OK(nvs_set_blob(net80211_handle, "sta.pmk", pmk, size));

			uint8_t chan = 1;
			TEST_ESP_ERR(nvs_get_u8(net80211_handle, "sta.chan", &chan), ESP_ERR_NVS_NOT_FOUND);
			TEST_ESP_OK(nvs_set_u8(net80211_handle, "sta.chan", chan));

			uint8_t autoconn = 1;
			TEST_ESP_ERR(nvs_get_u8(net80211_handle, "auto.conn", &autoconn), ESP_ERR_NVS_NOT_FOUND);
			TEST_ESP_OK(nvs_set_u8(net80211_handle, "auto.conn", autoconn));

			uint8_t bssid_set = 1;
			TEST_ESP_ERR(nvs_get_u8(net80211_handle, "bssid.set", &bssid_set), ESP_ERR_NVS_NOT_FOUND);
			TEST_ESP_OK(nvs_set_u8(net80211_handle, "bssid.set", bssid_set));

			char bssid[6];
			size = sizeof(bssid);
			TEST_ESP_ERR(nvs_get_blob(net80211_handle, "sta.bssid", bssid, &size), ESP_ERR_NVS_NOT_FOUND);
			memset(mac, 0xcd, 6);
			TEST_ESP_OK(nvs_set_blob(net80211_handle, "sta.bssid", bssid, size));

			uint8_t phym = 3;
			TEST_ESP_ERR(nvs_get_u8(net80211_handle, "sta.phym", &phym), ESP_ERR_NVS_NOT_FOUND);
			TEST_ESP_OK(nvs_set_u8(net80211_handle, "sta.phym", phym));

			uint8_t phybw = 2;
			TEST_ESP_ERR(nvs_get_u8(net80211_handle, "sta.phybw", &phybw), ESP_ERR_NVS_NOT_FOUND);
			TEST_ESP_OK(nvs_set_u8(net80211_handle, "sta.phybw", phybw));

			char apsw[2];
			size = sizeof(apsw);
			TEST_ESP_ERR(nvs_get_blob(net80211_handle, "sta.apsw", apsw, &size), ESP_ERR_NVS_NOT_FOUND);
			memset(apsw, 0x2, size);
			TEST_ESP_OK(nvs_set_blob(net80211_handle, "sta.apsw", apsw, size));

			char apinfo[700];
			size = sizeof(apinfo);
			TEST_ESP_ERR(nvs_get_blob(net80211_handle, "sta.apinfo", apinfo, &size), ESP_ERR_NVS_NOT_FOUND);
			memset(apinfo, 0, size);
			TEST_ESP_OK(nvs_set_blob(net80211_handle, "sta.apinfo", apinfo, size));

			size = sizeof(ssid);
			TEST_ESP_ERR(nvs_get_blob(net80211_handle, "ap.ssid", ssid, &size), ESP_ERR_NVS_NOT_FOUND);
			strcpy(ssid, "ESP_A2F340");
			TEST_ESP_OK(nvs_set_blob(net80211_handle, "ap.ssid", ssid, size));

			size = sizeof(mac);
			TEST_ESP_ERR(nvs_get_blob(net80211_handle, "ap.mac", mac, &size), ESP_ERR_NVS_NOT_FOUND);
			memset(mac, 0xac, 6);
			TEST_ESP_OK(nvs_set_blob(net80211_handle, "ap.mac", mac, size));

			size = sizeof(pswd);
			TEST_ESP_ERR(nvs_get_blob(net80211_handle, "ap.passwd", pswd, &size), ESP_ERR_NVS_NOT_FOUND);
			strcpy(pswd, "");
			TEST_ESP_OK(nvs_set_blob(net80211_handle, "ap.passwd", pswd, size));

			size = sizeof(pmk);
			TEST_ESP_ERR(nvs_get_blob(net80211_handle, "ap.pmk", pmk, &size), ESP_ERR_NVS_NOT_FOUND);
			memset(pmk, 1, size);
			TEST_ESP_OK(nvs_set_blob(net80211_handle, "ap.pmk", pmk, size));

			chan = 6;
			TEST_ESP_ERR(nvs_get_u8(net80211_handle, "ap.chan", &chan), ESP_ERR_NVS_NOT_FOUND);
			TEST_ESP_OK(nvs_set_u8(net80211_handle, "ap.chan", chan));

			authmode = 0;
			TEST_ESP_ERR(nvs_get_u8(net80211_handle, "ap.authmode", &authmode), ESP_ERR_NVS_NOT_FOUND);
			TEST_ESP_OK(nvs_set_u8(net80211_handle, "ap.authmode", authmode));

			uint8_t hidden = 0;
			TEST_ESP_ERR(nvs_get_u8(net80211_handle, "ap.hidden", &hidden), ESP_ERR_NVS_NOT_FOUND);
			TEST_ESP_OK(nvs_set_u8(net80211_handle, "ap.hidden", hidden));

			uint8_t max_conn = 4;
			TEST_ESP_ERR(nvs_get_u8(net80211_handle, "ap.max.conn", &max_conn), ESP_ERR_NVS_NOT_FOUND);
			TEST_ESP_OK(nvs_set_u8(net80211_handle, "ap.max.conn", max_conn));

			uint8_t bcn_interval = 2;
			TEST_ESP_ERR(nvs_get_u8(net80211_handle, "bcn_interval", &bcn_interval), ESP_ERR_NVS_NOT_FOUND);
			TEST_ESP_OK(nvs_set_u8(net80211_handle, "bcn_interval", bcn_interval));

			nvs_close(net80211_handle);

			f.emu.printTo(s_perf, _F("simulate nvs init with wifi libs"));

			TEST_ESP_OK(nvs_flash_deinit_partition(NVS_DEFAULT_PART_NAME));
		}

		TEST_CASE("writing the identical content does not write or erase")
		{
			PartitionEmulationFixture f(5, 10);
			REQUIRE(partitionManager.openContainer(f.part.ptr()));
			auto& stat = f.emu.stat();

			nvs_handle_t misc_handle;
			TEST_ESP_OK(nvs_open("test", NVS_READWRITE, &misc_handle));

			// Test writing a u8 twice, then changing it
			nvs_set_u8(misc_handle, "test_u8", 8);
			f.emu.clearStats();
			nvs_set_u8(misc_handle, "test_u8", 8);
			CHECK(stat.writeOps == 0);
			CHECK(stat.eraseOps == 0);
			CHECK(stat.readOps != 0);
			f.emu.clearStats();
			nvs_set_u8(misc_handle, "test_u8", 9);
			CHECK(stat.writeOps != 0);
			CHECK(stat.readOps != 0);

			// Test writing a string twice, then changing it
			static const char* test[2] = {"Hello world.", "Hello world!"};
			nvs_set_str(misc_handle, "test_str", test[0]);
			f.emu.clearStats();
			nvs_set_str(misc_handle, "test_str", test[0]);
			CHECK(stat.writeOps == 0);
			CHECK(stat.eraseOps == 0);
			CHECK(stat.readOps != 0);
			f.emu.clearStats();
			nvs_set_str(misc_handle, "test_str", test[1]);
			CHECK(stat.writeOps != 0);
			CHECK(stat.readOps != 0);

			// Test writing a multi-page blob, then changing it
			uint8_t blob[Page::CHUNK_MAX_SIZE * 3] = {0};
			memset(blob, 1, sizeof(blob));
			nvs_set_blob(misc_handle, "test_blob", blob, sizeof(blob));
			f.emu.clearStats();
			nvs_set_blob(misc_handle, "test_blob", blob, sizeof(blob));
			CHECK(stat.writeOps == 0);
			CHECK(stat.eraseOps == 0);
			CHECK(stat.readOps != 0);
			blob[sizeof(blob) - 1]++;
			f.emu.clearStats();
			nvs_set_blob(misc_handle, "test_blob", blob, sizeof(blob));
			CHECK(stat.writeOps != 0);
			CHECK(stat.readOps != 0);

			nvs_close(misc_handle);

			TEST_ESP_OK(nvs_flash_deinit_partition(NVS_DEFAULT_PART_NAME));
		}

		TEST_CASE("can init container from flash with random contents")
		{
			PartitionEmulationFixture f(5, 3);
			f.emu.randomize();
			REQUIRE(partitionManager.openContainer(f.part.ptr()));

			nvs_handle_t handle;
			TEST_ESP_OK(nvs_open("nvs.net80211", NVS_READWRITE, &handle));

			uint8_t opmode = 2;
			if(nvs_get_u8(handle, "wifi.opmode", &opmode) != ESP_OK) {
				TEST_ESP_OK(nvs_set_u8(handle, "wifi.opmode", opmode));
			}

			nvs_close(handle);

			TEST_ESP_OK(nvs_flash_deinit_partition(NVS_DEFAULT_PART_NAME));
		}

		TEST_CASE("nvs api tests, starting with random data in flash")
		{
			const size_t testIters = 3000;
			int lastPercent = -1;
			for(size_t count = 0; count < testIters; ++count) {
				int percentDone = (int)(count * 100 / testIters);
				if(percentDone != lastPercent) {
					lastPercent = percentDone;
					m_printf("\r%d%%  ", percentDone);
				}
				PartitionEmulationFixture f(6, 3);
				f.emu.randomize();
				CHECK(partitionManager.openContainer(f.part.ptr()));

				nvs_handle_t handle_1;
				TEST_ESP_ERR(nvs_open("namespace1", NVS_READONLY, &handle_1), ESP_ERR_NVS_NOT_FOUND);

				TEST_ESP_OK(nvs_open("namespace1", NVS_READWRITE, &handle_1));
				TEST_ESP_OK(nvs_set_i32(handle_1, "foo", 0x12345678));
				for(size_t i = 0; i < 500; ++i) {
					nvs_handle_t handle_2;
					TEST_ESP_OK(nvs_open("namespace2", NVS_READWRITE, &handle_2));
					TEST_ESP_OK(nvs_set_i32(handle_1, "foo", 0x23456789 % (i + 1)));
					TEST_ESP_OK(nvs_set_i32(handle_2, "foo", static_cast<int32_t>(i)));
					char str_buf[128];
					m_snprintf(str_buf, sizeof(str_buf), _F("value 0123456789abcdef0123456789abcdef %09d"),
							   i + count * 1024);
					TEST_ESP_OK(nvs_set_str(handle_2, "key", str_buf));

					int32_t v1;
					TEST_ESP_OK(nvs_get_i32(handle_1, "foo", &v1));
					CHECK(int32_t(0x23456789 % (i + 1)) == v1);

					int32_t v2;
					TEST_ESP_OK(nvs_get_i32(handle_2, "foo", &v2));
					CHECK(int32_t(i) == v2);

					char buf[128];
					size_t buf_len = sizeof(buf);
					TEST_ESP_OK(nvs_get_str(handle_2, "key", buf, &buf_len));

					CHECK(0 == strcmp(buf, str_buf));
					nvs_close(handle_2);
				}
				nvs_close(handle_1);

				TEST_ESP_OK(nvs_flash_deinit_partition(NVS_DEFAULT_PART_NAME));
			}
		}

		TEST_CASE("monkey test")
		{
			PartitionEmulationFixture f(2, 8);
			f.emu.randomize();
			f.emu.clearStats();

			REQUIRE(partitionManager.openContainer(f.part.ptr()));

			nvs_handle_t handle;
			TEST_ESP_OK(nvs_open("namespace1", NVS_READWRITE, &handle));
			RandomTest test;
			size_t count = 1000;
			CHECK(test.doRandomThings(handle, count) == ESP_OK);
			nvs_close(handle);

			auto& stat = f.emu.stat();
			s_perf.print(_F("Monkey test: nErase="));
			s_perf.print(stat.eraseOps);
			s_perf.print(" nWrite=");
			s_perf.println(stat.writeOps);

			TEST_ESP_OK(nvs_flash_deinit_partition(NVS_DEFAULT_PART_NAME));
		}

		TEST_CASE("test recovery from sudden poweroff")
		{
			const size_t iter_count = 2000;
			PartitionEmulationFixture f(2, 8);

			size_t totalOps{0};
			int lastPercent{-1};
			for(uint32_t errDelay = 0;; ++errDelay) {
				m_printf("\r%u  ", errDelay);
				f.emu.randomize();
				f.emu.clearStats();
				f.emu.failAfter(errDelay);
				RandomTest test;

				if(totalOps != 0) {
					int percent = errDelay * 100 / totalOps;
					if(percent > lastPercent) {
						m_printf("\r%u/%u (%d%%)  ", errDelay, totalOps, percent);
						lastPercent = percent;
					}
				}

				nvs_handle_t handle;
				size_t count = iter_count;

				if(partitionManager.openContainer(f.part.ptr())) {
					if(nvs_open("namespace1", NVS_READWRITE, &handle) == ESP_OK) {
						auto err = test.doRandomThings(handle, count);
						nvs_close(handle);
						if(err != ESP_ERR_FLASH_OP_FAIL) {
							break;
						}
					}
					TEST_ESP_OK(nvs_flash_deinit_partition(NVS_DEFAULT_PART_NAME));
				}

				CHECK(partitionManager.openContainer(f.part.ptr()));

				TEST_ESP_OK(nvs_open("namespace1", NVS_READWRITE, &handle));
				auto res = test.doRandomThings(handle, count);
				if(res != ESP_OK) {
					nvs_dump(NVS_DEFAULT_PART_NAME);
					CHECK(false);
				}
				nvs_close(handle);
				auto& stat = f.emu.stat();
				totalOps = stat.eraseOps + stat.writeBytes / 4;

				TEST_ESP_OK(nvs_flash_deinit_partition(NVS_DEFAULT_PART_NAME));
			}
		}

		TEST_CASE("test for memory leaks in open/set")
		{
			PartitionEmulationFixture f(6, 3);
			REQUIRE(partitionManager.openContainer(f.part.ptr()));

			for(int i = 0; i < 100000; ++i) {
				nvs_handle_t light_handle = 0;
				char lightbulb[1024] = {12, 13, 14, 15, 16};
				TEST_ESP_OK(nvs_open("light", NVS_READWRITE, &light_handle));
				TEST_ESP_OK(nvs_set_blob(light_handle, "key", lightbulb, sizeof(lightbulb)));
				TEST_ESP_OK(nvs_commit(light_handle));
				nvs_close(light_handle);
			}

			TEST_ESP_OK(nvs_flash_deinit_partition(NVS_DEFAULT_PART_NAME));
		}

		TEST_CASE("duplicate items are removed")
		{
			PartitionEmulationFixture f(0, 3);
			{
				// create one item
				nvs::Page p;
				p.load(f.part, 0);
				p.writeItem<uint8_t>(1, "opmode", 3);
			}
			{
				// add another two without deleting the first one
				nvs::Item item(1, ItemType::U8, 1, "opmode");
				item.data[0] = 2;
				item.crc32 = item.calculateCrc32();
				f.emu.write(3 * 32, &item, sizeof(item));
				f.emu.write(4 * 32, &item, sizeof(item));
				uint32_t mask = 0xFFFFFFEA;
				f.emu.write(32, &mask, 4);
			}
			{
				// load page and check that second item persists
				nvs::Container c(f.part.ptr());
				CHECK(c.init());
				uint8_t val;
				CHECK(c.readItem(1, "opmode", val));
				CHECK_EQ(val, uint8_t(2));
			}
			{
				Page p;
				p.load(f.part, 0);
				CHECK_EQ(p.getErasedEntryCount(), 2U);
				CHECK_EQ(p.getUsedEntryCount(), 1U);
			}
		}

		TEST_CASE("recovery after failure to write data")
		{
			PartitionEmulationFixture f(0, 3);
			PSTR_ARRAY(str, "value 0123456789abcdef012345678value 0123456789abcdef012345678");

			// make flash write fail exactly in Page::writeEntryData
			f.emu.failAfter(17);
			{
				Container container(f.part.ptr());
				REQUIRE(container.init());

				REQUIRE(!container.writeItem(1, ItemType::SZ, "key", str, strlen(str)));
				TEST_ESP_ERR(nvs_errno, ESP_ERR_FLASH_OP_FAIL);

				// check that repeated operations cause an error
				REQUIRE(!container.writeItem(1, ItemType::SZ, "key", str, strlen(str)));
				TEST_ESP_ERR(nvs_errno, ESP_ERR_NVS_INVALID_STATE);

				uint8_t val;
				REQUIRE(!container.readItem(1, ItemType::U8, "key", &val, sizeof(val)));
				TEST_ESP_ERR(nvs_errno, ESP_ERR_NVS_NOT_FOUND);
			}
			{
				// load page and check that data was erased
				Page p;
				p.load(f.part, 0);
				CHECK_EQ(p.getErasedEntryCount(), 3U);
				CHECK_EQ(p.getUsedEntryCount(), 0U);

				// try to write again
				TEST_ESP_OK(p.writeItem(1, ItemType::SZ, "key", str, strlen(str)));
			}
		}

		TEST_CASE("crc errors in item header are handled")
		{
			PartitionEmulationFixture f(0, 3);
			Container container(f.part.ptr());
			// prepare some data
			REQUIRE(container.init());
			REQUIRE(container.writeItem(0, "ns1", uint8_t(1)));
			REQUIRE(container.writeItem(1, "value1", uint32_t(1)));
			REQUIRE(container.writeItem(1, "value2", uint32_t(2)));

			// corrupt item header
			uint32_t val{0};
			f.emu.write(32 * 3, &val, sizeof(val));

			// check that container can recover
			REQUIRE(container.init());
			REQUIRE(container.readItem(1, "value2", val));
			CHECK(val == 2);
			// check that the corrupted item is no longer present
			REQUIRE(!container.readItem(1, "value1", val));
			TEST_ESP_ERR(nvs_errno, ESP_ERR_NVS_NOT_FOUND);

			// add more items to make the page full
			for(size_t i = 0; i < Page::ENTRY_COUNT; ++i) {
				char item_name[Item::MAX_KEY_LENGTH + 1];
				m_snprintf(item_name, sizeof(item_name), "item_%ld", (long int)i);
				CHECK(container.writeItem(1, item_name, uint32_t(i)));
			}

			// corrupt another item on the full page
			val = 0;
			f.emu.write(32 * 4, &val, sizeof(val));

			// check that container can recover
			REQUIRE(container.init());
			// check that the corrupted item is no longer present
			REQUIRE(!container.readItem(1, "value2", val));
			TEST_ESP_ERR(nvs_errno, ESP_ERR_NVS_NOT_FOUND);
		}

		TEST_CASE("crc error in variable length item is handled")
		{
			PartitionEmulationFixture f(0, 3);
			constexpr uint64_t before_val{0xbef04e};
			constexpr uint64_t after_val{0xaf7e4};
			// write some data
			{
				Page p;
				p.load(f.part, 0);
				TEST_ESP_OK(p.writeItem<uint64_t>(0, "before", before_val));
				const char* str = "foobar";
				TEST_ESP_OK(p.writeItem(0, ItemType::SZ, "key", str, strlen(str)));
				TEST_ESP_OK(p.writeItem<uint64_t>(0, "after", after_val));
			}
			// corrupt some data
			uint32_t w;
			CHECK(f.emu.read(32 * 3 + 8, &w, sizeof(w)));
			w &= 0xf000000f;
			CHECK(f.emu.write(32 * 3 + 8, &w, sizeof(w)));
			// load and check
			{
				Page p;
				p.load(f.part, 0);
				CHECK_EQ(p.getUsedEntryCount(), 2U);
				CHECK_EQ(p.getErasedEntryCount(), 2U);

				uint64_t val;
				TEST_ESP_OK(p.readItem<uint64_t>(0, "before", val));
				CHECK(val == before_val);
				TEST_ESP_ERR(p.findItem(0, ItemType::SZ, "key"), ESP_ERR_NVS_NOT_FOUND);
				TEST_ESP_OK(p.readItem<uint64_t>(0, "after", val));
				CHECK_EQ(val, after_val);
			}
		}

		TEST_CASE("read/write failure (TW8406)")
		{
			PartitionEmulationFixture f(0, 3);
			REQUIRE(partitionManager.openContainer(f.part.ptr()));
			for(int attempts = 0; attempts < 3; ++attempts) {
				int i{0};
				nvs_handle_t light_handle{0};
				char key[15]{0};
				char data[76]{12, 13, 14, 15, 16};
				uint8_t number{20};
				size_t data_len{sizeof(data)};

				TEST_ESP_OK(nvs_open("LIGHT", NVS_READWRITE, &light_handle));
				TEST_ESP_OK(nvs_set_u8(light_handle, "RecordNum", number));
				for(i = 0; i < number; ++i) {
					m_snprintf(key, sizeof(key), "light%d", i);
					TEST_ESP_OK(nvs_set_blob(light_handle, key, data, sizeof(data)));
				}
				nvs_commit(light_handle);

				uint8_t get_number = 0;
				TEST_ESP_OK(nvs_get_u8(light_handle, "RecordNum", &get_number));
				REQUIRE_EQ(number, get_number);
				for(i = 0; i < number; ++i) {
					char data[76] = {0};
					m_snprintf(key, sizeof(key), "light%d", i);
					TEST_ESP_OK(nvs_get_blob(light_handle, key, data, &data_len));
				}
				nvs_close(light_handle);
			}

			TEST_ESP_OK(nvs_flash_deinit_partition(NVS_DEFAULT_PART_NAME));
		}

		TEST_CASE("nvs_flash_init checks for an empty page")
		{
			const size_t blob_size = Page::CHUNK_MAX_SIZE;
			uint8_t blob[blob_size] = {0};
			PartitionEmulationFixture f(0, 5);
			REQUIRE(partitionManager.openContainer(f.part.ptr()));
			nvs_handle_t handle;
			TEST_ESP_OK(nvs_open("test", NVS_READWRITE, &handle));
			// Fill first page
			TEST_ESP_OK(nvs_set_blob(handle, "1a", blob, blob_size));
			// Fill second page
			TEST_ESP_OK(nvs_set_blob(handle, "2a", blob, blob_size));
			// Fill third page
			TEST_ESP_OK(nvs_set_blob(handle, "3a", blob, blob_size));
			TEST_ESP_OK(nvs_commit(handle));
			nvs_close(handle);
			TEST_ESP_OK(nvs_flash_deinit_partition(NVS_DEFAULT_PART_NAME));
			// first two pages are now full, third one is writable, last two are empty
			// init should fail
			PartitionEmulator part(f.emu, 0, 3 * SPI_FLASH_SEC_SIZE);
			REQUIRE(!partitionManager.openContainer(part.ptr()));
			TEST_ESP_ERR(nvs_errno, ESP_ERR_NVS_NO_FREE_PAGES);

			// in case this test fails, to not affect other tests
			nvs_flash_deinit_partition(NVS_DEFAULT_PART_NAME);
		}

		TEST_CASE("multiple partitions access check")
		{
			FlashEmulator emu(10);
			PartitionEmulator p0(emu, 0 * SPI_FLASH_SEC_SIZE, 5 * SPI_FLASH_SEC_SIZE, "nvs1");
			PartitionEmulator p1(emu, 5 * SPI_FLASH_SEC_SIZE, 5 * SPI_FLASH_SEC_SIZE, "nvs2");
			REQUIRE(partitionManager.openContainer(p0.ptr()));
			REQUIRE(partitionManager.openContainer(p1.ptr()));

			nvs_handle_t handle1, handle2;
			TEST_ESP_OK(nvs_open_from_partition("nvs1", "test", NVS_READWRITE, &handle1));
			TEST_ESP_OK(nvs_open_from_partition("nvs2", "test", NVS_READWRITE, &handle2));
			TEST_ESP_OK(nvs_set_i32(handle1, "foo", 0xdeadbeef));
			TEST_ESP_OK(nvs_set_i32(handle2, "foo", 0xcafebabe));
			int32_t v1, v2;
			TEST_ESP_OK(nvs_get_i32(handle1, "foo", &v1));
			TEST_ESP_OK(nvs_get_i32(handle2, "foo", &v2));
			CHECK_EQ(v1, int32_t(0xdeadbeef));
			CHECK_EQ(v2, int32_t(0xcafebabe));

			nvs_close(handle1);
			nvs_close(handle2);

			TEST_ESP_OK(nvs_flash_deinit_partition(p0.name().c_str()));
			TEST_ESP_OK(nvs_flash_deinit_partition(p1.name().c_str()));
		}

		TEST_CASE("nvs page selection takes into account free entries also not just erased entries")
		{
			const size_t blob_size = Page::CHUNK_MAX_SIZE / 2;
			uint8_t blob[blob_size] = {0};
			PartitionEmulationFixture f(0, 3);
			REQUIRE(partitionManager.openContainer(f.part.ptr()));
			nvs_handle_t handle;
			TEST_ESP_OK(nvs_open("test", NVS_READWRITE, &handle));
			// Fill first page
			TEST_ESP_OK(nvs_set_blob(handle, "1a", blob, blob_size / 3));
			TEST_ESP_OK(nvs_set_blob(handle, "1b", blob, blob_size));
			// Fill second page
			TEST_ESP_OK(nvs_set_blob(handle, "2a", blob, blob_size));
			TEST_ESP_OK(nvs_set_blob(handle, "2b", blob, blob_size));

			// The item below should be able to fit the first page.
			TEST_ESP_OK(nvs_set_blob(handle, "3a", blob, 4));
			TEST_ESP_OK(nvs_commit(handle));
			nvs_close(handle);

			TEST_ESP_OK(nvs_flash_deinit_partition(f.part.name().c_str()));
		}

		TEST_CASE("calculate used and free space")
		{
			PartitionEmulationFixture f(0, 6);
			auto container = partitionManager.openContainer(f.part.ptr());
			REQUIRE(container);

			Page p;
			// after erase. empty partition
			nvs_stats_t stat1;
			REQUIRE(container->fillStats(stat1));
			CHECK_NEQ(stat1.freeEntries, 0U);
			CHECK_EQ(stat1.namespaceCount, 0U);
			CHECK_EQ(stat1.totalEntries, 6 * p.ENTRY_COUNT);
			CHECK_EQ(stat1.usedEntries, 0U);

			// create namespace test_k1
			nvs_handle_t handle_1;
			nvs_stats_t stat2;
			TEST_ESP_OK(nvs_open("test_k1", NVS_READWRITE, &handle_1));
			REQUIRE(container->fillStats(stat2));
			CHECK_EQ(stat2.freeEntries + 1, stat1.freeEntries);
			CHECK_EQ(stat2.namespaceCount, 1U);
			CHECK_EQ(stat2.totalEntries, stat1.totalEntries);
			CHECK_EQ(stat2.usedEntries, 1U);

			// create pair key-value com
			TEST_ESP_OK(nvs_set_i32(handle_1, "com", 0x12345678));
			REQUIRE(container->fillStats(stat1));
			CHECK_EQ(stat1.freeEntries + 1, stat2.freeEntries);
			CHECK_EQ(stat1.namespaceCount, 1U);
			CHECK_EQ(stat1.totalEntries, stat2.totalEntries);
			CHECK_EQ(stat1.usedEntries, 2U);

			// change value in com
			TEST_ESP_OK(nvs_set_i32(handle_1, "com", 0x01234567));
			REQUIRE(container->fillStats(stat2));
			CHECK_EQ(stat2.freeEntries, stat1.freeEntries);
			CHECK_EQ(stat2.namespaceCount, 1U);
			CHECK_NEQ(stat2.totalEntries, 0U);
			CHECK_EQ(stat2.usedEntries, 2U);

			// create pair key-value ru
			TEST_ESP_OK(nvs_set_i32(handle_1, "ru", 0x00FF00FF));
			REQUIRE(container->fillStats(stat1));
			CHECK_EQ(stat1.freeEntries + 1, stat2.freeEntries);
			CHECK_EQ(stat1.namespaceCount, 1U);
			CHECK_NEQ(stat1.totalEntries, 0U);
			CHECK_EQ(stat1.usedEntries, 3U);

			// amount valid pair in namespace 1
			//			size_t h1_count_entries;
			//			TEST_ESP_OK(nvs_get_used_entry_count(handle_1, &h1_count_entries));
			//			CHECK(h1_count_entries == 2);

			nvs_handle_t handle_2;
			// create namespace test_k2
			TEST_ESP_OK(nvs_open("test_k2", NVS_READWRITE, &handle_2));
			REQUIRE(container->fillStats(stat2));
			CHECK_EQ(stat2.freeEntries + 1, stat1.freeEntries);
			CHECK_EQ(stat2.namespaceCount, 2U);
			CHECK_EQ(stat2.totalEntries, stat1.totalEntries);
			CHECK_EQ(stat2.usedEntries, 4U);

			// create pair key-value
			TEST_ESP_OK(nvs_set_i32(handle_2, "su1", 0x00000001));
			TEST_ESP_OK(nvs_set_i32(handle_2, "su2", 0x00000002));
			TEST_ESP_OK(nvs_set_i32(handle_2, "sus", 0x00000003));
			REQUIRE(container->fillStats(stat1));
			CHECK_EQ(stat1.freeEntries + 3, stat2.freeEntries);
			CHECK_EQ(stat1.namespaceCount, 2U);
			CHECK_EQ(stat1.totalEntries, stat2.totalEntries);
			CHECK_EQ(stat1.usedEntries, 7U);

			CHECK_EQ(stat1.totalEntries, stat1.usedEntries + stat1.freeEntries);

			// amount valid pair in namespace 2
			//			size_t h2_count_entries;
			//			TEST_ESP_OK(nvs_get_used_entry_count(handle_2, &h2_count_entries));
			//			CHECK(h2_count_entries == 3);

			//			CHECK(stat1.usedEntries == (h1_count_entries + h2_count_entries + stat1.namespaceCount));

			nvs_close(handle_1);
			nvs_close(handle_2);

			nvs_handle_t handle_3;
			// create namespace test_k3
			TEST_ESP_OK(nvs_open("test_k3", NVS_READWRITE, &handle_3));
			REQUIRE(container->fillStats(stat2));
			CHECK_EQ(stat2.freeEntries + 1, stat1.freeEntries);
			CHECK_EQ(stat2.namespaceCount, 3U);
			CHECK_EQ(stat2.totalEntries, stat1.totalEntries);
			CHECK_EQ(stat2.usedEntries, 8U);

			// create pair blobs
			uint32_t blob[12];
			TEST_ESP_OK(nvs_set_blob(handle_3, "bl1", &blob, sizeof(blob)));
			REQUIRE(container->fillStats(stat1));
			CHECK_EQ(stat1.freeEntries + 4, stat2.freeEntries);
			CHECK_EQ(stat1.namespaceCount, 3U);
			CHECK_EQ(stat1.totalEntries, stat2.totalEntries);
			CHECK_EQ(stat1.usedEntries, 12U);

			// amount valid pair in namespace 2
			//			size_t h3_count_entries;
			//			TEST_ESP_OK(nvs_get_used_entry_count(handle_3, &h3_count_entries));
			//			CHECK(h3_count_entries == 4);

			//			CHECK(stat1.usedEntries == (h1_count_entries + h2_count_entries + h3_count_entries + stat1.namespaceCount));

			nvs_close(handle_3);

			TEST_ESP_OK(nvs_flash_deinit_partition(f.part.name().c_str()));
		}

		// TODO: leaks memory
		TEST_CASE("Recovery from power-off when the entry being erased is not on active page")
		{
			const size_t blob_size = Page::CHUNK_MAX_SIZE / 2;
			size_t read_size = blob_size;
			uint8_t blob[blob_size] = {0x11};
			PartitionEmulationFixture f(0, 3);
			REQUIRE(partitionManager.openContainer(f.part.ptr()));
			nvs_handle_t handle;
			TEST_ESP_OK(nvs_open("test", NVS_READWRITE, &handle));

			f.emu.clearStats();
			f.emu.failAfter(Page::CHUNK_MAX_SIZE / 4 + 75);
			TEST_ESP_OK(nvs_set_blob(handle, "1a", blob, blob_size));
			TEST_ESP_OK(nvs_set_blob(handle, "1b", blob, blob_size));

			TEST_ESP_ERR(nvs_erase_key(handle, "1a"), ESP_ERR_FLASH_OP_FAIL);

			REQUIRE(partitionManager.openContainer(f.part.ptr()));

			// Check 1a is erased fully
			TEST_ESP_ERR(nvs_get_blob(handle, "1a", blob, &read_size), ESP_ERR_NVS_NOT_FOUND);

			// Check 2b is still accessible
			TEST_ESP_OK(nvs_get_blob(handle, "1b", blob, &read_size));

			nvs_close(handle);

			TEST_ESP_OK(nvs_flash_deinit_partition(f.part.name().c_str()));
		}

		// TODO: leaks memory
		TEST_CASE("Recovery from power-off when page is being freed.")
		{
			const size_t blob_size = (Page::ENTRY_COUNT - 3) * Page::ENTRY_SIZE;
			size_t read_size = blob_size / 2;
			uint8_t blob[blob_size] = {0};
			PartitionEmulationFixture f(0, 3);
			REQUIRE(partitionManager.openContainer(f.part.ptr()));
			nvs_handle_t handle;
			TEST_ESP_OK(nvs_open("test", NVS_READWRITE, &handle));
			// Fill first page
			TEST_ESP_OK(nvs_set_blob(handle, "1a", blob, blob_size / 3));
			TEST_ESP_OK(nvs_set_blob(handle, "1b", blob, blob_size / 3));
			TEST_ESP_OK(nvs_set_blob(handle, "1c", blob, blob_size / 4));
			// Fill second page
			TEST_ESP_OK(nvs_set_blob(handle, "2a", blob, blob_size / 2));
			TEST_ESP_OK(nvs_set_blob(handle, "2b", blob, blob_size / 2));

			TEST_ESP_OK(nvs_erase_key(handle, "1c"));

			f.emu.clearStats();
			f.emu.failAfter(6 * Page::ENTRY_COUNT);
			TEST_ESP_ERR(nvs_set_blob(handle, "1d", blob, blob_size / 4), ESP_ERR_FLASH_OP_FAIL);

			REQUIRE(partitionManager.openContainer(f.part.ptr()));

			read_size = blob_size / 3;
			TEST_ESP_OK(nvs_get_blob(handle, "1a", blob, &read_size));
			TEST_ESP_OK(nvs_get_blob(handle, "1b", blob, &read_size));

			read_size = blob_size / 4;
			TEST_ESP_ERR(nvs_get_blob(handle, "1c", blob, &read_size), ESP_ERR_NVS_NOT_FOUND);
			TEST_ESP_ERR(nvs_get_blob(handle, "1d", blob, &read_size), ESP_ERR_NVS_NOT_FOUND);

			read_size = blob_size / 2;
			TEST_ESP_OK(nvs_get_blob(handle, "2a", blob, &read_size));
			TEST_ESP_OK(nvs_get_blob(handle, "2b", blob, &read_size));

			TEST_ESP_OK(nvs_commit(handle));
			nvs_close(handle);

			TEST_ESP_OK(nvs_flash_deinit_partition(f.part.name().c_str()));
		}

		TEST_CASE("Multi-page blobs are supported")
		{
			const size_t blob_size = Page::CHUNK_MAX_SIZE * 2;
			uint8_t blob[blob_size] = {0};
			PartitionEmulationFixture f(0, 5);
			REQUIRE(partitionManager.openContainer(f.part.ptr()));
			nvs_handle_t handle;
			TEST_ESP_OK(nvs_open("test", NVS_READWRITE, &handle));
			TEST_ESP_OK(nvs_set_blob(handle, "abc", blob, blob_size));
			TEST_ESP_OK(nvs_commit(handle));
			nvs_close(handle);

			TEST_ESP_OK(nvs_flash_deinit_partition(f.part.name().c_str()));
		}

		TEST_CASE("Failures are handled while storing multi-page blobs")
		{
			const size_t blob_size = Page::CHUNK_MAX_SIZE * 7;
			uint8_t blob[blob_size] = {0};
			PartitionEmulationFixture f(0, 5);
			REQUIRE(partitionManager.openContainer(f.part.ptr()));
			nvs_handle_t handle;
			TEST_ESP_OK(nvs_open("test", NVS_READWRITE, &handle));
			TEST_ESP_ERR(nvs_set_blob(handle, "abc", blob, blob_size), ESP_ERR_NVS_VALUE_TOO_LONG);
			TEST_ESP_OK(nvs_set_blob(handle, "abc", blob, Page::CHUNK_MAX_SIZE * 2));
			TEST_ESP_OK(nvs_commit(handle));
			nvs_close(handle);

			TEST_ESP_OK(nvs_flash_deinit_partition(f.part.name().c_str()));
		}

		TEST_CASE("Reading multi-page blobs")
		{
			const size_t blob_size = Page::CHUNK_MAX_SIZE * 3;
			uint8_t blob[blob_size];
			uint8_t blob_read[blob_size];
			size_t read_size = blob_size;
			PartitionEmulationFixture f(0, 5);
			REQUIRE(partitionManager.openContainer(f.part.ptr()));
			nvs_handle_t handle;
			memset(blob, 0x11, blob_size);
			memset(blob_read, 0xee, blob_size);
			TEST_ESP_OK(nvs_open("readTest", NVS_READWRITE, &handle));
			TEST_ESP_OK(nvs_set_blob(handle, "abc", blob, blob_size));
			TEST_ESP_OK(nvs_get_blob(handle, "abc", blob_read, &read_size));
			CHECK(memcmp(blob, blob_read, blob_size) == 0);
			TEST_ESP_OK(nvs_commit(handle));
			nvs_close(handle);

			TEST_ESP_OK(nvs_flash_deinit_partition(f.part.name().c_str()));
		}

		TEST_CASE("Modification of values for Multi-page blobs are supported")
		{
			const size_t blob_size = Page::CHUNK_MAX_SIZE * 2;
			uint8_t blob[blob_size] = {0};
			uint8_t blob_read[blob_size] = {0xfe};
			uint8_t blob2[blob_size] = {0x11};
			uint8_t blob3[blob_size] = {0x22};
			uint8_t blob4[blob_size] = {0x33};
			size_t read_size = blob_size;
			PartitionEmulationFixture f(0, 6);
			REQUIRE(partitionManager.openContainer(f.part.ptr()));
			nvs_handle_t handle;
			memset(blob, 0x11, blob_size);
			memset(blob2, 0x22, blob_size);
			memset(blob3, 0x33, blob_size);
			memset(blob4, 0x44, blob_size);
			memset(blob_read, 0xff, blob_size);
			TEST_ESP_OK(nvs_open("test", NVS_READWRITE, &handle));
			TEST_ESP_OK(nvs_set_blob(handle, "abc", blob, blob_size));
			TEST_ESP_OK(nvs_set_blob(handle, "abc", blob2, blob_size));
			TEST_ESP_OK(nvs_set_blob(handle, "abc", blob3, blob_size));
			TEST_ESP_OK(nvs_set_blob(handle, "abc", blob4, blob_size));
			TEST_ESP_OK(nvs_get_blob(handle, "abc", blob_read, &read_size));
			CHECK(memcmp(blob4, blob_read, blob_size) == 0);
			TEST_ESP_OK(nvs_commit(handle));
			nvs_close(handle);

			TEST_ESP_OK(nvs_flash_deinit_partition(f.part.name().c_str()));
		}

		TEST_CASE("Modification from single page blob to multi-page")
		{
			const size_t blob_size = Page::CHUNK_MAX_SIZE * 3;
			uint8_t blob[blob_size] = {0};
			uint8_t blob_read[blob_size] = {0xff};
			size_t read_size = blob_size;
			PartitionEmulationFixture f(0, 5);
			REQUIRE(partitionManager.openContainer(f.part.ptr()));
			nvs_handle_t handle;
			TEST_ESP_OK(nvs_open("Test", NVS_READWRITE, &handle));
			TEST_ESP_OK(nvs_set_blob(handle, "abc", blob, Page::CHUNK_MAX_SIZE / 2));
			TEST_ESP_OK(nvs_set_blob(handle, "abc", blob, blob_size));
			TEST_ESP_OK(nvs_get_blob(handle, "abc", blob_read, &read_size));
			CHECK(memcmp(blob, blob_read, blob_size) == 0);
			TEST_ESP_OK(nvs_commit(handle));
			nvs_close(handle);

			TEST_ESP_OK(nvs_flash_deinit_partition(f.part.name().c_str()));
		}

		TEST_CASE("Modification from  multi-page to single page")
		{
			const size_t blob_size = Page::CHUNK_MAX_SIZE * 3;
			uint8_t blob[blob_size] = {0};
			uint8_t blob_read[blob_size] = {0xff};
			size_t read_size = blob_size;
			PartitionEmulationFixture f(0, 5);
			REQUIRE(partitionManager.openContainer(f.part.ptr()));
			nvs_handle_t handle;
			TEST_ESP_OK(nvs_open("Test", NVS_READWRITE, &handle));
			TEST_ESP_OK(nvs_set_blob(handle, "abc", blob, blob_size));
			TEST_ESP_OK(nvs_set_blob(handle, "abc", blob, Page::CHUNK_MAX_SIZE / 2));
			TEST_ESP_OK(nvs_set_blob(handle, "abc2", blob, blob_size));
			TEST_ESP_OK(nvs_get_blob(handle, "abc", blob_read, &read_size));
			CHECK(memcmp(blob, blob_read, Page::CHUNK_MAX_SIZE) == 0);
			TEST_ESP_OK(nvs_commit(handle));
			nvs_close(handle);

			TEST_ESP_OK(nvs_flash_deinit_partition(f.part.name().c_str()));
		}

		TEST_CASE("Multi-page blob erased using nvs_erase_key should not be found when probed for just length")
		{
			const size_t blob_size = Page::CHUNK_MAX_SIZE * 3;
			uint8_t blob[blob_size] = {0};
			size_t read_size = blob_size;
			PartitionEmulationFixture f(0, 5);
			REQUIRE(partitionManager.openContainer(f.part.ptr()));
			nvs_handle_t handle;
			TEST_ESP_OK(nvs_open("Test", NVS_READWRITE, &handle));
			TEST_ESP_OK(nvs_set_blob(handle, "abc", blob, blob_size));
			TEST_ESP_OK(nvs_erase_key(handle, "abc"));
			TEST_ESP_ERR(nvs_get_blob(handle, "abc", nullptr, &read_size), ESP_ERR_NVS_NOT_FOUND);
			TEST_ESP_OK(nvs_commit(handle));
			nvs_close(handle);

			TEST_ESP_OK(nvs_flash_deinit_partition(f.part.name().c_str()));
		}

		TEST_CASE("Check that orphaned blobs are erased during init")
		{
			constexpr size_t blob_size{Page::CHUNK_MAX_SIZE * 3};
			uint8_t blob[blob_size]{0x11};
			PartitionEmulationFixture f(0, 5);
			Container container(f.part.ptr());

			REQUIRE(container.init());

			CHECK(container.writeItem(1, ItemType::BLOB, "key", blob, sizeof(blob)));

			CHECK(container.init());
			// Check that multi - page item is still available.
			CHECK(container.readItem(1, ItemType::BLOB, "key", blob, sizeof(blob)));

			CHECK(!container.writeItem(1, ItemType::BLOB, "key2", blob, sizeof(blob)));
			TEST_ESP_ERR(nvs_errno, ESP_ERR_NVS_NOT_ENOUGH_SPACE);

			Page p;
			p.load(f.part, 3); // This is where index will be placed.
			p.erase();

			CHECK(container.init());

			CHECK(!container.readItem(1, ItemType::BLOB, "key", blob, sizeof(blob)));
			TEST_ESP_ERR(nvs_errno, ESP_ERR_NVS_NOT_FOUND);
			CHECK(container.writeItem(1, ItemType::BLOB, "key3", blob, sizeof(blob)));
		}

		TEST_CASE("nvs blob fragmentation test")
		{
			PartitionEmulationFixture f(0, 4);
			REQUIRE(partitionManager.openContainer(f.part.ptr()));
			constexpr size_t BLOB_SIZE{3500};
			auto blob = new uint8_t[BLOB_SIZE];
			memset(blob, 0xEE, BLOB_SIZE);
			const uint32_t magic = 0xff33eaeb;

			nvs_handle_t h;
			TEST_ESP_OK(nvs_open("blob_tests", NVS_READWRITE, &h));
			for(int i = 0; i < 128; i++) {
				m_printf("\rIteration %u ...  ", i);
				TEST_ESP_OK(nvs_set_u32(h, "magic", magic));
				TEST_ESP_OK(nvs_set_blob(h, "blob", blob, BLOB_SIZE));
				char seq_buf[16];
				m_snprintf(seq_buf, sizeof(seq_buf), "seq%d", i);
				TEST_ESP_OK(nvs_set_u32(h, seq_buf, i));
			}
			delete[] blob;
			nvs_close(h);

			TEST_ESP_OK(nvs_flash_deinit_partition(f.part.name().c_str()));
		}

		TEST_CASE("nvs code handles errors properly when partition is near to full")
		{
			constexpr size_t blob_size{3 * Page::CHUNK_MAX_SIZE / 10};
			uint8_t blob[blob_size]{0x11};
			PartitionEmulationFixture f(0, 5);
			Container container(f.part.ptr());
			char nvs_key[16]{""};

			REQUIRE(container.init());

			// Four pages should fit roughly 12 blobs
			for(uint8_t count = 1; count <= 12; count++) {
				m_snprintf(nvs_key, sizeof(nvs_key), "key:%u", count);
				CHECK(container.writeItem(1, ItemType::BLOB, nvs_key, blob, sizeof(blob)));
			}

			for(uint8_t count = 13; count <= 20; count++) {
				m_snprintf(nvs_key, sizeof(nvs_key), "key:%u", count);
				CHECK(!container.writeItem(1, ItemType::BLOB, nvs_key, blob, sizeof(blob)));
				TEST_ESP_ERR(nvs_errno, ESP_ERR_NVS_NOT_ENOUGH_SPACE);
			}
		}

		TEST_CASE("Check for nvs version incompatibility")
		{
			PartitionEmulationFixture f(0, 3);

			int32_t val1 = 0x12345678;
			Page p;
			p.load(f.part, 0);
			TEST_ESP_OK(p.setVersion(Page::NVS_VERSION - 1));
			TEST_ESP_OK(p.writeItem(1, ItemType::I32, "foo", &val1, sizeof(val1)));

			REQUIRE(!partitionManager.openContainer(f.part.ptr()));
			TEST_ESP_ERR(nvs_errno, ESP_ERR_NVS_NEW_VERSION_FOUND);

			// if something went wrong, clean up
			nvs_flash_deinit_partition(f.part.name().c_str());
		}

		TEST_CASE("Check that NVS supports old blob format without blob index")
		{
			FlashEmulator emu(testData::part_old_blob_format);
			PartitionEmulator part(emu, 0, 2 * SPI_FLASH_SEC_SIZE, "test");
			nvs_handle_t handle;

			REQUIRE(partitionManager.openContainer(part.ptr()));
			TEST_ESP_OK(nvs_open_from_partition("test", "dummyNamespace", NVS_READWRITE, &handle));

			char buf[64] = {0};
			size_t buflen = 64;
			uint8_t hexdata[] = {0x01, 0x02, 0x03, 0xab, 0xcd, 0xef};
			TEST_ESP_OK(nvs_get_blob(handle, "dummyHex2BinKey", buf, &buflen));
			CHECK(memcmp(buf, hexdata, buflen) == 0);

			buflen = 64;
			uint8_t base64data[] = {'1', '2', '3', 'a', 'b', 'c'};
			TEST_ESP_OK(nvs_get_blob(handle, "dummyBase64Key", buf, &buflen));
			CHECK(memcmp(buf, base64data, buflen) == 0);

			Page p;
			p.load(part, 0);

			// Check that item is stored in old format without blob index
			TEST_ESP_OK(p.findItem(1, ItemType::BLOB, "dummyHex2BinKey"));

			// Modify the blob so that it is stored in the new format
			hexdata[0] = hexdata[1] = hexdata[2] = 0x99;
			TEST_ESP_OK(nvs_set_blob(handle, "dummyHex2BinKey", hexdata, sizeof(hexdata)));

			Page p2;
			p2.load(part, 0);

			// Check the type of the blob.Expect type mismatch since the blob is stored in new format
			TEST_ESP_ERR(p2.findItem(1, ItemType::BLOB, "dummyHex2BinKey"), ESP_ERR_NVS_TYPE_MISMATCH);

			// Check that index is present for the modified blob according to new format
			TEST_ESP_OK(p2.findItem(1, ItemType::BLOB_IDX, "dummyHex2BinKey"));

			// Read the blob in new format and check the contents buflen = 64;
			TEST_ESP_OK(nvs_get_blob(handle, "dummyBase64Key", buf, &buflen));
			CHECK(memcmp(buf, base64data, buflen) == 0);

			nvs_close(handle);

			TEST_ESP_OK(nvs_flash_deinit_partition(part.name().c_str()));
		}

		// TODO: leaks memory
		TEST_CASE("monkey test with old-format blob present")
		{
			const uint32_t NVS_FLASH_SECTOR = 2;
			const uint32_t NVS_FLASH_SECTOR_COUNT_MIN = 8;

			PartitionEmulationFixture f(NVS_FLASH_SECTOR, NVS_FLASH_SECTOR_COUNT_MIN);
			f.emu.randomize();
			f.emu.clearStats();

			const size_t smallBlobLen = Page::CHUNK_MAX_SIZE / 3;

			auto container = partitionManager.openContainer(f.part.ptr());
			REQUIRE(container);

			nvs_handle_t handle;
			TEST_ESP_OK(nvs_open("namespace1", NVS_READWRITE, &handle));
			RandomTest test;

			for(uint8_t it = 0; it < 10; it++) {
				size_t count{200};

				// Erase index and chunks for the blob with "singlepage" key
				for(uint8_t num = 0; num < NVS_FLASH_SECTOR_COUNT_MIN; num++) {
					Page p;
					p.load(f.part, num);
					p.eraseItem(1, ItemType::BLOB, "singlepage", Item::CHUNK_ANY, VerOffset::VER_ANY);
					p.eraseItem(1, ItemType::BLOB_IDX, "singlepage", Item::CHUNK_ANY, VerOffset::VER_ANY);
					p.eraseItem(1, ItemType::BLOB_DATA, "singlepage", Item::CHUNK_ANY, VerOffset::VER_ANY);
				}

				// Now write "singlepage" blob in old format
				for(uint8_t num = 0; num < NVS_FLASH_SECTOR_COUNT_MIN; num++) {
					Page p;
					TEST_ESP_OK(p.load(f.part, num));
					if(p.state() == Page::PageState::ACTIVE) {
						uint8_t buf[smallBlobLen];
						size_t blobLen = gen() % smallBlobLen;

						if(blobLen > p.getVarDataTailroom()) {
							blobLen = p.getVarDataTailroom();
						}

						os_get_random(buf, blobLen);

						TEST_ESP_OK(p.writeItem(1, ItemType::BLOB, "singlepage", buf, blobLen, Item::CHUNK_ANY));
						TEST_ESP_OK(p.findItem(1, ItemType::BLOB, "singlepage"));
						test.handleExternalWrite("singlepage", buf, blobLen);

						break;
					}
				}

				nvs_close(handle);

				TEST_ESP_OK(nvs_flash_deinit_partition(f.part.name().c_str()));
				// Initialize again
				REQUIRE(partitionManager.openContainer(f.part.ptr()));
				TEST_ESP_OK(nvs_open("namespace1", NVS_READWRITE, &handle));

				// Perform random things
				auto res = test.doRandomThings(handle, count);
				if(res != ESP_OK) {
					nvs_dump(NVS_DEFAULT_PART_NAME);
					CHECK(0);
				}

				// Check that only one version is present for "singlepage". Its possible that last iteration did not write
				// anything for "singlepage". So either old version or new version should be present.
				bool oldVerPresent{false};
				bool newVerPresent{false};

				for(uint8_t num = 0; num < NVS_FLASH_SECTOR_COUNT_MIN; num++) {
					Page p;
					TEST_ESP_OK(p.load(f.part, num));
					if(!oldVerPresent &&
					   p.findItem(1, ItemType::BLOB, "singlepage", Item::CHUNK_ANY, VerOffset::VER_ANY) == ESP_OK) {
						oldVerPresent = true;
					}

					if(!newVerPresent &&
					   p.findItem(1, ItemType::BLOB_IDX, "singlepage", Item::CHUNK_ANY, VerOffset::VER_ANY) == ESP_OK) {
						newVerPresent = true;
					}
				}
				CHECK_NEQ(oldVerPresent, newVerPresent);
			}

			nvs_close(handle);

			TEST_ESP_OK(nvs_flash_deinit_partition(f.part.name().c_str()));
			auto& stat = f.emu.stat();
			s_perf.print(_F("Monkey test: nErase="));
			s_perf.print(stat.eraseOps);
			s_perf.print(" nWrite=");
			s_perf.println(stat.writeOps);
		}

		TEST_CASE("Recovery from power-off during modification of blob present in old-format (same page)")
		{
			PartitionEmulationFixture f(0, 3);
			f.emu.clearStats();

			REQUIRE(partitionManager.openContainer(f.part.ptr()));

			nvs_handle_t handle;
			TEST_ESP_OK(nvs_open("namespace1", NVS_READWRITE, &handle));
			nvs_close(handle);

			uint8_t hexdata[] = {0x01, 0x02, 0x03, 0xab, 0xcd, 0xef};
			uint8_t hexdata_old[] = {0x11, 0x12, 0x13, 0xbb, 0xcc, 0xee};
			size_t buflen = sizeof(hexdata);
			uint8_t buf[Page::CHUNK_MAX_SIZE];

			// Power - off when blob was being written on the same page where its old version in old format* was present
			Page p;
			p.load(f.part, 0);
			// Write blob in old - format
			TEST_ESP_OK(p.writeItem(1, ItemType::BLOB, "singlepage", hexdata_old, sizeof(hexdata_old)));

			// Write blob in new format
			TEST_ESP_OK(p.writeItem(1, ItemType::BLOB_DATA, "singlepage", hexdata, sizeof(hexdata), 0));
			// All pages are stored.Now store the index
			Item item;
			item.blobIndex.dataSize = sizeof(hexdata);
			item.blobIndex.chunkCount = 1;
			item.blobIndex.chunkStart = VerOffset::VER_0_OFFSET;

			TEST_ESP_OK(p.writeItem(1, ItemType::BLOB_IDX, "singlepage", item.data, sizeof(item.data)));

			TEST_ESP_OK(p.findItem(1, ItemType::BLOB, "singlepage"));

			TEST_ESP_OK(nvs_flash_deinit_partition(NVS_DEFAULT_PART_NAME));
			// Initialize again
			REQUIRE(partitionManager.openContainer(f.part.ptr()));
			TEST_ESP_OK(nvs_open("namespace1", NVS_READWRITE, &handle));

			TEST_ESP_OK(nvs_get_blob(handle, "singlepage", buf, &buflen));
			CHECK(memcmp(buf, hexdata, buflen) == 0);
			nvs_close(handle);

			Page p2;
			p2.load(f.part, 0);
			TEST_ESP_ERR(p2.findItem(1, ItemType::BLOB, "singlepage"), ESP_ERR_NVS_TYPE_MISMATCH);

			TEST_ESP_OK(nvs_flash_deinit_partition(f.part.name().c_str()));
		}

		TEST_CASE("Recovery from power-off during modification of blob present in old-format (different page)")
		{
			PartitionEmulationFixture f(0, 3);
			f.emu.clearStats();

			REQUIRE(partitionManager.openContainer(f.part.ptr()));

			nvs_handle_t handle;
			TEST_ESP_OK(nvs_open("namespace1", NVS_READWRITE, &handle));
			nvs_close(handle);

			uint8_t hexdata[] = {0x01, 0x02, 0x03, 0xab, 0xcd, 0xef};
			uint8_t hexdata_old[] = {0x11, 0x12, 0x13, 0xbb, 0xcc, 0xee};
			size_t buflen = sizeof(hexdata);
			uint8_t buf[Page::CHUNK_MAX_SIZE];

			// Power - off when blob was being written on the different page where its old version in old format* was present
			Page p;
			p.load(f.part, 0);
			// Write blob in old-format
			TEST_ESP_OK(p.writeItem(1, ItemType::BLOB, "singlepage", hexdata_old, sizeof(hexdata_old)));

			// Write blob in new format
			TEST_ESP_OK(p.writeItem(1, ItemType::BLOB_DATA, "singlepage", hexdata, sizeof(hexdata), 0));
			// All pages are stored.Now store the index
			Item item{};
			item.blobIndex.dataSize = sizeof(hexdata);
			item.blobIndex.chunkCount = 1;
			item.blobIndex.chunkStart = VerOffset::VER_0_OFFSET;
			p.markFull();
			Page p2;
			p2.load(f.part, 1);
			p2.setSeqNumber(1);

			TEST_ESP_OK(p2.writeItem(1, ItemType::BLOB_IDX, "singlepage", item.data, sizeof(item.data)));

			TEST_ESP_OK(p.findItem(1, ItemType::BLOB, "singlepage"));

			TEST_ESP_OK(nvs_flash_deinit_partition(f.part.name().c_str()));
			// Initialize again
			REQUIRE(partitionManager.openContainer(f.part.ptr()));
			TEST_ESP_OK(nvs_open("namespace1", NVS_READWRITE, &handle));

			TEST_ESP_OK(nvs_get_blob(handle, "singlepage", buf, &buflen));
			CHECK(memcmp(buf, hexdata, buflen) == 0);
			nvs_close(handle);

			Page p3;
			p3.load(f.part, 0);
			TEST_ESP_ERR(p3.findItem(1, ItemType::BLOB, "singlepage"), ESP_ERR_NVS_NOT_FOUND);

			TEST_ESP_OK(nvs_flash_deinit_partition(f.part.name().c_str()));
		}

		TEST_CASE("check and read data from partition generated via partition generation utility with multipage blob "
				  "support disabled")
		{
			/*
				./nvs_partition_gen.py generate sample_singlepage_blob.csv partition_single_page.bin 0x3000 --version 1
			*/
			FlashEmulator emu(testData::singlepage_blob_partition);

			check_nvs_part_gen_args(emu, "test", 3, testData::singlepage_blob, false, nullptr);
		}

		TEST_CASE("check and read data from partition generated via partition generation utility with multipage blob "
				  "support enabled")
		{
			/*
				./nvs_partition_gen.py generate sample_multipage_blob.csv partition_multipage_blob.bin 0x4000 --version 2
			*/
			FlashEmulator emu(testData::multipage_blob_partition);

			check_nvs_part_gen_args(emu, "test", 4, testData::multipage_blob, false, nullptr);
		}

		TEST_CASE("check and read data from partition generated via manufacturing utility with multipage blob support "
				  "disabled")
		{
			/*
				rm -rf ../../../tools/mass_mfg/host_test
				cp -rf ../../../tools/mass_mfg/testdata mfg_testdata
				cp -rf ../nvs_partition_generator/testdata .
				mkdir -p ../../../tools/mass_mfg/host_test
				../../../tools/mass_mfg/mfg_gen.py generate ../../../tools/mass_mfg/samples/sample_config.csv ../../../tools/mass_mfg/samples/sample_values_singlepage_blob.csv Test 0x3000 --version 1
				nvs_partition_gen.py generate csv/Test-1.csv Test-1-partition.bin 0x3000 --version 1
			 */

			FlashEmulator emu1(testData::test_1_v1);
			check_nvs_part_gen_args(emu1, "test", 3, testData::singlepage_blob, false, nullptr);

			FlashEmulator emu2(testData::test_1_v1_partition);
			check_nvs_part_gen_args(emu2, "test", 3, testData::singlepage_blob, false, nullptr);
		}

		TEST_CASE("check and read data from partition generated via manufacturing utility with multipage blob support "
				  "enabled")
		{
			/*
				../../../tools/mass_mfg/mfg_gen.py generate ../../../tools/mass_mfg/samples/sample_config.csv ../../../tools/mass_mfg/samples/sample_values_multipage_blob.csv Test 0x4000 --version 2
				./nvs_partition_gen.py generate csv/Test-1.csv ../nvs_partition_generator/Test-1-partition.bin 0x4000 --version 2
			*/

			FlashEmulator emu1(testData::test_1_v2);
			check_nvs_part_gen_args(emu1, "test", 4, testData::multipage_blob, false, nullptr);

			FlashEmulator emu2(testData::test_1_v2_partition);
			check_nvs_part_gen_args(emu2, "test", 4, testData::multipage_blob, false, nullptr);
		}

		// Add new tests above
		// This test has to be the final one

		TEST_CASE("dump all performance data")
		{
			Serial.println(_F("===================="));
			Serial.println(_F("Dumping benchmarks"));
			Serial.copyFrom(&s_perf);
			Serial.println(_F("===================="));
		}
	}

	void check_nvs_part_gen_args(FlashEmulator& flashEmulator, char const* part_name, int size,
								 const FlashString& part_data, bool is_encr, const EncryptionKey* xts_cfg)
	{
		nvs_handle_t handle;

		PartitionPtr part;
		if(is_encr) {
#ifdef ENABLE_NVS_ENCRYPTION
			EncryptedPartitionEmulator enc_p(flashEmulator, 0, size * SPI_FLASH_SEC_SIZE);
			TEST_ESP_OK(enc_p.init(*xts_cfg));
			part = enc_p.ptr();
#else
			assert(false);
#endif
		} else {
			part = PartitionEmulator(flashEmulator, 0, size * SPI_FLASH_SEC_SIZE).ptr();
		}
		REQUIRE(partitionManager.openContainer(part));

		TEST_ESP_OK(nvs_open_from_partition(part_name, "dummyNamespace", NVS_READONLY, &handle));
		uint8_t u8v;
		TEST_ESP_OK(nvs_get_u8(handle, "dummyU8Key", &u8v));
		CHECK(u8v == 127);
		int8_t i8v;
		TEST_ESP_OK(nvs_get_i8(handle, "dummyI8Key", &i8v));
		CHECK(i8v == -128);
		uint16_t u16v;
		TEST_ESP_OK(nvs_get_u16(handle, "dummyU16Key", &u16v));
		CHECK(u16v == 32768);
		uint32_t u32v;
		TEST_ESP_OK(nvs_get_u32(handle, "dummyU32Key", &u32v));
		CHECK(u32v == 4294967295);
		int32_t i32v;
		TEST_ESP_OK(nvs_get_i32(handle, "dummyI32Key", &i32v));
		CHECK(i32v == -2147483648);

		char buf[64] = {0};
		size_t buflen = 64;
		TEST_ESP_OK(nvs_get_str(handle, "dummyStringKey", buf, &buflen));
		CHECK(strncmp(buf, "0A:0B:0C:0D:0E:0F", buflen) == 0);

		uint8_t hexdata[] = {0x01, 0x02, 0x03, 0xab, 0xcd, 0xef};
		buflen = 64;
		TEST_ESP_OK(nvs_get_blob(handle, "dummyHex2BinKey", buf, &buflen));
		CHECK(memcmp(buf, hexdata, buflen) == 0);

		uint8_t base64data[] = {'1', '2', '3', 'a', 'b', 'c'};
		TEST_ESP_OK(nvs_get_blob(handle, "dummyBase64Key", buf, &buflen));
		CHECK(memcmp(buf, base64data, buflen) == 0);

		buflen = 64;
		uint8_t hexfiledata[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
		TEST_ESP_OK(nvs_get_blob(handle, "hexFileKey", buf, &buflen));
		CHECK(memcmp(buf, hexfiledata, buflen) == 0);

		buflen = 64;
		uint8_t strfiledata[64] = "abcdefghijklmnopqrstuvwxyz\0";
		TEST_ESP_OK(nvs_get_str(handle, "stringFileKey", buf, &buflen));
		CHECK(memcmp(buf, strfiledata, buflen) == 0);

		char bin_data[5200];
		size_t bin_len = sizeof(bin_data);
		char binfiledata[5200];
		part_data.readFlash(0, binfiledata, bin_len);
		TEST_ESP_OK(nvs_get_blob(handle, "binFileKey", bin_data, &bin_len));
		CHECK(memcmp(bin_data, binfiledata, bin_len) == 0);

		nvs_close(handle);

		TEST_ESP_OK(nvs_flash_deinit_partition(part_name));
	}

	static void nvs_dump(const char* partName)
	{
		auto container = partitionManager.lookupContainer(partName);
		if(container) {
			container->debugDump();
		}
	}

private:
	MemoryDataStream s_perf;
};

void REGISTER_TEST(nvs)
{
	registerGroup<NvsTest>();
}
