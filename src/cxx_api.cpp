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

#include "include/Nvs/PartitionManager.hpp"
#include "include/Nvs/Handle.hpp"

namespace nvs
{
std::unique_ptr<Handle> open_nvs_handle_from_partition(const char* partition_name, const char* ns_name,
													   nvs_open_mode_t open_mode, esp_err_t* err)
{
	if(partition_name == nullptr || ns_name == nullptr) {
		if(err) {
			*err = ESP_ERR_INVALID_ARG;
		}
		return nullptr;
	}

	Handle* handle;
	esp_err_t result = partitionManager.open_handle(partition_name, ns_name, open_mode, handle);

	if(err) {
		*err = result;
	}

	return std::unique_ptr<Handle>(handle);
}

std::unique_ptr<Handle> open_nvs_handle(const char* ns_name, nvs_open_mode_t open_mode, esp_err_t* err)
{
	return open_nvs_handle_from_partition(NVS_DEFAULT_PART_NAME, ns_name, open_mode, err);
}

} // namespace nvs
