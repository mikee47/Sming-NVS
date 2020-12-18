// Copyright 2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstring>
#include <memory>
#include "../include/Nvs/EncryptedPartition.hpp"
#include "../include/Nvs/Item.hpp"

#define ESP_ENCRYPT_BLOCK_SIZE 16

namespace nvs
{
esp_err_t EncryptedPartition::init(const EncryptionKey& cfg)
{
	mbedtls_aes_xts_init(&mEctxt);
	mbedtls_aes_xts_init(&mDctxt);

	if(mbedtls_aes_xts_setkey_enc(&mEctxt, cfg.eky, sizeof(cfg)) != 0) {
		return ESP_ERR_NVS_XTS_CFG_FAILED;
	}

	if(mbedtls_aes_xts_setkey_dec(&mDctxt, cfg.eky, sizeof(cfg)) != 0) {
		return ESP_ERR_NVS_XTS_CFG_FAILED;
	}

	return ESP_OK;
}

esp_err_t EncryptedPartition::read(size_t src_offset, void* dst, size_t size)
{
	/*
	 * Currently upper layer of NVS reads entries one by one even for variable size
	 * multi-entry data types. So length should always be equal to size of an entry.
	 */
	if(size != sizeof(Item)) {
		return ESP_ERR_INVALID_SIZE;
	}

	// read data
	esp_err_t err = Partition::read(src_offset, dst, size);
	if(err != ESP_OK) {
		return err;
	}

	// decrypt data
	uint32_t data_unit[4]{0};

	data_unit[0] = src_offset;

	auto destination = static_cast<uint8_t*>(dst);

	if(mbedtls_aes_crypt_xts(&mDctxt, MBEDTLS_AES_DECRYPT, size, reinterpret_cast<uint8_t*>(data_unit), destination,
							 destination) != 0) {
		err = ESP_ERR_NVS_XTS_DECR_FAILED;
	}

	return err;
}

esp_err_t EncryptedPartition::write(size_t addr, const void* src, size_t size)
{
	if(size % ESP_ENCRYPT_BLOCK_SIZE != 0) {
		return ESP_ERR_INVALID_SIZE;
	}

	// copy data to buffer for encryption
	auto buf = std::unique_ptr<uint8_t[]>(new(std::nothrow) uint8_t[size]);
	if(!buf) {
		return ESP_ERR_NO_MEM;
	}

	memcpy(buf.get(), src, size);

	// encrypt data
	constexpr uint8_t entrySize{sizeof(Item)};

	//sector num required as an arr by mbedtls
	uint32_t data_unit[4]{0};

	for(uint32_t off = 0; off < size; off += entrySize) {
		data_unit[0] = addr + off;
		if(mbedtls_aes_crypt_xts(&mEctxt, MBEDTLS_AES_ENCRYPT, entrySize, reinterpret_cast<unsigned char*>(data_unit),
								 buf.get() + off, buf.get() + off) != 0) {
			return ESP_ERR_NVS_XTS_ENCR_FAILED;
		}
	}

	return Partition::write(addr, buf.get(), size);
}

} // namespace nvs
