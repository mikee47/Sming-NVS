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

#include <NvsTestBase.h>
#include <Nvs/Page.hpp>
#include <Nvs/nvs.h>

using namespace nvs;

#define THING_MAP(XX)                                                                                                  \
	XX("foo", I32, v1)                                                                                                 \
	XX("bar", I32, v2)                                                                                                 \
	XX("longkey_0123456", U64, v3)                                                                                     \
	XX("another key", U64, v4)                                                                                         \
	XX("param1", SZ, v5)                                                                                               \
	XX("param2", SZ, v6)                                                                                               \
	XX("param3", SZ, v7)                                                                                               \
	XX("param4", SZ, v8)                                                                                               \
	XX("param5", SZ, v9)                                                                                               \
	XX("singlepage", BLOB, v10)                                                                                        \
	XX("multipage", BLOB, v11)

inline uint32_t gen()
{
	return os_random();
}

template <typename T> void gen(T& value)
{
	os_get_random(reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

void gen(void* buffer, size_t len)
{
	os_get_random(static_cast<uint8_t*>(buffer), len);
}

class RandomTest : public NvsTestBase
{
public:
	static const size_t nKeys = 11;
	static constexpr size_t strBufLen{1024};
	static constexpr size_t smallBlobLen{Page::CHUNK_MAX_SIZE / 3};
	static constexpr size_t largeBlobLen{Page::CHUNK_MAX_SIZE * 3};

	union Value {
		int32_t i32;
		uint32_t u32;
		uint64_t u64;
		char str[strBufLen];
		uint8_t blob[largeBlobLen];
	};

	struct Thing {
		const char* key;
		ItemType type;
		Value* value;
		size_t varsize;
		size_t len;
		bool written;

		bool operator==(const char* key) const
		{
			return key != nullptr && strcmp(this->key, key) == 0;
		}

		void update(const void* buf, size_t len)
		{
			if(type == ItemType::STR) {
				++len;
			}
			assert(len <= varsize);
			memcpy(value, buf, len);
			this->len = len;
			written = true;
		}

		template <typename T> void update(const T& value)
		{
			assert(sizeof(value) == varsize);
			update(&value, sizeof(value));
		}
	};

#define XX(name, type, var) {name, ItemType::type, reinterpret_cast<Value*>(&var), sizeof(var)},
	RandomTest() : things{THING_MAP(XX)}
	{
	}
#undef XX

	esp_err_t doRandomThings(nvs_handle_t handle, size_t& count)
	{
		for(; count != 0; --count) {
			size_t index = gen() % nKeys;
			auto& thing = things[index];
			if(gen() % 3 == 0) {
				// read, 1/3
				if(randomRead(handle, thing) == ESP_ERR_FLASH_OP_FAIL) {
					return ESP_ERR_FLASH_OP_FAIL;
				}
			} else if(randomWrite(handle, thing) == ESP_ERR_FLASH_OP_FAIL) {
				return ESP_ERR_FLASH_OP_FAIL;
			}
		}
		return ESP_OK;
	}

	esp_err_t randomRead(nvs_handle_t handle, Thing& thing)
	{
		switch(thing.type) {
		case ItemType::I32: {
			int32_t val;
			auto err = nvs_get_i32(handle, thing.key, &val);
			if(err == ESP_ERR_FLASH_OP_FAIL) {
				return err;
			}
			if(!thing.written) {
				CHECK_EQ(err, ESP_ERR_NVS_NOT_FOUND);
			} else {
				CHECK_EQ(err, ESP_OK);
				CHECK_EQ(val, thing.value->i32);
			}
			break;
		}

		case ItemType::U64: {
			uint64_t val;
			auto err = nvs_get_u64(handle, thing.key, &val);
			if(err == ESP_ERR_FLASH_OP_FAIL) {
				return err;
			}
			if(!thing.written) {
				CHECK_EQ(err, ESP_ERR_NVS_NOT_FOUND);
			} else {
				CHECK_EQ(err, ESP_OK);
				CHECK_EQ(val, thing.value->u64);
			}
			break;
		}

		case ItemType::SZ: {
			char buf[strBufLen];
			size_t len = strBufLen;
			auto err = nvs_get_str(handle, thing.key, buf, &len);
			if(err == ESP_ERR_FLASH_OP_FAIL) {
				return err;
			}
			if(!thing.written) {
				CHECK_EQ(err, ESP_ERR_NVS_NOT_FOUND);
			} else {
				CHECK_EQ(err, ESP_OK);
				CHECK_EQ(len, thing.len);
				CHECK(memcmp(buf, thing.value->str, len) == 0);
			}
			break;
		}

		case ItemType::BLOB: {
			auto blobBufLen = thing.varsize;
			uint8_t buf[blobBufLen];
			memset(buf, 0, blobBufLen);

			size_t len = blobBufLen;
			auto err = nvs_get_blob(handle, thing.key, buf, &len);
			if(err == ESP_ERR_FLASH_OP_FAIL) {
				return err;
			}
			if(!thing.written) {
				CHECK_EQ(err, ESP_ERR_NVS_NOT_FOUND);
			} else {
				CHECK_EQ(err, ESP_OK);
				CHECK_EQ(len, thing.len);
				CHECK(memcmp(buf, thing.value->blob, len) == 0);
			}
			break;
		}

		default:
			assert(0);
		}
		return ESP_OK;
	}

	esp_err_t randomWrite(nvs_handle_t handle, Thing& thing)
	{
		switch(thing.type) {
		case ItemType::I32: {
			auto val = int32_t(gen());

			auto err = nvs_set_i32(handle, thing.key, val);
			if(err == ESP_ERR_FLASH_OP_FAIL) {
				return err;
			}
			if(err == ESP_ERR_NVS_REMOVE_FAILED) {
				thing.update(val);
				return ESP_ERR_FLASH_OP_FAIL;
			}
			CHECK_EQ(err, ESP_OK);
			thing.update(val);
			break;
		}

		case ItemType::U64: {
			uint64_t val;
			gen(val);

			auto err = nvs_set_u64(handle, thing.key, val);
			if(err == ESP_ERR_FLASH_OP_FAIL) {
				return err;
			}
			if(err == ESP_ERR_NVS_REMOVE_FAILED) {
				thing.update(val);
				return ESP_ERR_FLASH_OP_FAIL;
			}
			CHECK_EQ(err, ESP_OK);
			thing.update(val);
			break;
		}

		case ItemType::SZ: {
			char buf[strBufLen];
			size_t strLen = gen() % (strBufLen - 1);
			std::generate_n(buf, strLen, [&]() -> char {
				auto c = gen() % 127;
				return (c < 32) ? 32 : c;
			});
			buf[strLen] = '\0';

			auto err = nvs_set_str(handle, thing.key, buf);
			if(err == ESP_ERR_FLASH_OP_FAIL) {
				return err;
			}
			if(err == ESP_ERR_NVS_REMOVE_FAILED) {
				thing.update(buf, strLen);
				return ESP_ERR_FLASH_OP_FAIL;
			}
			CHECK_EQ(err, ESP_OK);
			thing.update(buf, strLen);
			break;
		}

		case ItemType::BLOB: {
			auto blobBufLen = thing.varsize;
			uint8_t buf[blobBufLen];
			memset(buf, 0, blobBufLen);
			size_t blobLen = gen() % blobBufLen;
			gen(buf, blobLen);

			auto err = nvs_set_blob(handle, thing.key, buf, blobLen);
			if(err == ESP_ERR_FLASH_OP_FAIL) {
				return err;
			}
			if(err == ESP_ERR_NVS_REMOVE_FAILED) {
				thing.update(buf, blobLen);
				return ESP_ERR_FLASH_OP_FAIL;
			}
			CHECK_EQ(err, ESP_OK);
			thing.update(buf, blobLen);
			break;
		}

		default:
			assert(false);
		}
		return ESP_OK;
	}

	esp_err_t handleExternalWrite(const char* key, const void* value, size_t len)
	{
		auto it = std::find(std::begin(things), std::end(things), key);
		if(!it) {
			return ESP_FAIL;
		}

		it->update(value, len);
		return ESP_OK;
	}

private:
	Thing things[nKeys];
	int32_t v1{0};
	int32_t v2{0};
	uint64_t v3{0};
	uint64_t v4{0};
	char v5[strBufLen];
	char v6[strBufLen];
	char v7[strBufLen];
	char v8[strBufLen];
	char v9[strBufLen];
	uint8_t v10[smallBlobLen];
	uint8_t v11[largeBlobLen];
	bool written[nKeys];
};
