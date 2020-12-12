#include "nvs_partition_lookup.hpp"
#include <Storage/PartitionTable.h>
#include "nvs.h"

#ifdef CONFIG_NVS_ENCRYPTION
#include "nvs_encrypted_partition.hpp"
#endif // CONFIG_NVS_ENCRYPTION

namespace nvs {

namespace partition_lookup {

esp_err_t lookup_nvs_partition(const char* label, NVSPartition *&p)
{
	p = nullptr;

    auto part = ::Storage::PartitionTable().find(label);
    if(!part) {
    	return ESP_ERR_NOT_FOUND;
    }

    if(!part.verify(::Storage::Partition::SubType::Data::nvs)) {
    	return ESP_ERR_NOT_SUPPORTED;
    }

    if (part.isEncrypted()) {
        return ESP_ERR_NVS_WRONG_ENCRYPTION;
    }

    NVSPartition *partition = new (std::nothrow) NVSPartition(part);
    if (partition == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    p = partition;

    return ESP_OK;
}

#ifdef CONFIG_NVS_ENCRYPTION
esp_err_t lookup_nvs_encrypted_partition(const char* label, nvs_sec_cfg_t* cfg, NVSPartition *&p)
{
	p = nullptr;

	auto part = ::Storage::PartitionTable().find(label);
    if(!part) {
    	return ESP_ERR_NOT_FOUND;
    }

    if(!part.verify(::Storage::Partition::SubType::Data::nvs)) {
    	return ESP_ERR_NOT_SUPPORTED;
    }

    if (part.isEncrypted()) {
        return ESP_ERR_NVS_WRONG_ENCRYPTION;
    }

    NVSEncryptedPartition *enc_p = new (std::nothrow) NVSEncryptedPartition(part);
    if (enc_p == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t result = enc_p->init(cfg);
    if (result != ESP_OK) {
        delete enc_p;
        return result;
    }

    p = enc_p;

    return ESP_OK;
}

#endif // CONFIG_NVS_ENCRYPTION

} // partition_lookup

} // nvs
