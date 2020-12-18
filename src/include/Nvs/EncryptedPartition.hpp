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

#pragma once

#include <mbedtls/aes.h>
#include "../include/Nvs/Partition.hpp"

namespace nvs
{
#define NVS_KEY_SIZE 32 // AES-256

/**
 * @brief Key for encryption and decryption
 */
struct EncryptionKey {
	uint8_t eky[NVS_KEY_SIZE]; /*!<  XTS encryption and decryption key*/
	uint8_t tky[NVS_KEY_SIZE]; /*!<  XTS tweak key */
};

class EncryptedPartition : public Partition
{
public:
	using Partition::Partition;

	esp_err_t init(const EncryptionKey& cfg);
	esp_err_t read(size_t src_offset, void* dst, size_t size) override;
	esp_err_t write(size_t dst_offset, const void* src, size_t size) override;

protected:
	mbedtls_aes_xts_context mEctxt;
	mbedtls_aes_xts_context mDctxt;
};

} // namespace nvs
