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
#include "EncryptedPartition.hpp"
#include "include/Nvs/Item.hpp"

#define ESP_ENCRYPT_BLOCK_SIZE 16

namespace nvs
{
esp_err_t EncryptedPartition::init(nvs_sec_cfg_t* cfg)
{
#ifdef ENABLE_MBEDTLS
	uint8_t* eky = reinterpret_cast<uint8_t*>(cfg);

	mbedtls_aes_xts_init(&mEctxt);
	mbedtls_aes_xts_init(&mDctxt);

	if(mbedtls_aes_xts_setkey_enc(&mEctxt, eky, 2 * NVS_KEY_SIZE * 8) != 0) {
		return ESP_ERR_NVS_XTS_CFG_FAILED;
	}

	if(mbedtls_aes_xts_setkey_dec(&mDctxt, eky, 2 * NVS_KEY_SIZE * 8) != 0) {
		return ESP_ERR_NVS_XTS_CFG_FAILED;
	}

	return ESP_OK;
#else
	return ESP_ERR_NVS_ENCR_NOT_SUPPORTED;
#endif
}

esp_err_t EncryptedPartition::read(size_t src_offset, void* dst, size_t size)
{
#ifdef ENABLE_MBEDTLS
	/** Currently upper layer of NVS reads entries one by one even for variable size
    * multi-entry data types. So length should always be equal to size of an entry.*/
	if(size != sizeof(Item))
		return ESP_ERR_INVALID_SIZE;

	// read data
	esp_err_t read_result = Partition::read(src_offset, dst, size);
	if(read_result != ESP_OK) {
		return read_result;
	}

	// decrypt data
	//sector num required as an arr by mbedtls. Should have been just uint64/32.
	uint8_t data_unit[16];

	uint32_t relAddr = src_offset;

	memset(data_unit, 0, sizeof(data_unit));

	memcpy(data_unit, &relAddr, sizeof(relAddr));

	uint8_t* destination = reinterpret_cast<uint8_t*>(dst);

	if(mbedtls_aes_crypt_xts(&mDctxt, MBEDTLS_AES_DECRYPT, size, data_unit, destination, destination) != 0) {
		return ESP_ERR_NVS_XTS_DECR_FAILED;
	}

	return ESP_OK;
#else
	return ESP_ERR_NVS_ENCR_NOT_SUPPORTED;
#endif
}

esp_err_t EncryptedPartition::write(size_t addr, const void* src, size_t size)
{
#ifdef ENABLE_MBEDTLS
	if(size % ESP_ENCRYPT_BLOCK_SIZE != 0)
		return false;

	// copy data to buffer for encryption
	uint8_t* buf = new(std::nothrow) uint8_t[size];

	if(!buf)
		return false;

	memcpy(buf, src, size);

	// encrypt data
	uint8_t entrySize = sizeof(Item);

	//sector num required as an arr by mbedtls. Should have been just uint64/32.
	uint8_t data_unit[16];

	/* Use relative address instead of absolute address (relocatable), so that host-generated
     * encrypted nvs images can be used*/
	uint32_t relAddr = offset;

	memset(data_unit, 0, sizeof(data_unit));

	for(uint8_t entry = 0; entry < (size / entrySize); entry++) {
		uint32_t off = entry * entrySize;
		uint32_t* addr_loc = (uint32_t*)&data_unit[0];

		*addr_loc = relAddr + off;
		if(mbedtls_aes_crypt_xts(&mEctxt, MBEDTLS_AES_ENCRYPT, entrySize, data_unit, buf + off, buf + off) != 0) {
			delete buf;
			return false;
			return ESP_ERR_NVS_XTS_ENCR_FAILED;
		}
	}

	// write data
	esp_err_t result = Partition::write(addr, buf, size);

	delete buf;

	return result;

#else
	return ESP_ERR_NVS_ENCR_NOT_SUPPORTED;
#endif
}

} // namespace nvs
