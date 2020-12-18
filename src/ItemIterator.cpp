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

#include "include/Nvs/ItemIterator.hpp"
#include "include/Nvs/Container.hpp"

namespace nvs
{
ItemIterator::ItemIterator(Container& container, const String& nsName, ItemType itemType)
	: ItemIterator(container, itemType)
{
	done = !container.createOrOpenNamespace(nsName, false, nsIndex);
}

void ItemIterator::reset()
{
	entryIndex = 0;
	page = container.mPageManager.begin();
	done = false;
}

bool ItemIterator::next()
{
	if(!bool(*this)) {
		return false;
	}

	auto isIterableItem = [this]() {
		return (nsIndex != 0 && datatype != ItemType::BLOB && datatype != ItemType::BLOB_IDX);
	};

	auto isMultipageBlob = [this]() {
		return datatype == ItemType::BLOB_DATA && chunkIndex != uint8_t(VerOffset::VER_0_OFFSET) &&
			   chunkIndex != uint8_t(VerOffset::VER_1_OFFSET);
	};

	for(; page != container.mPageManager.end(); ++page) {
		esp_err_t err;
		do {
			err = page->findItem(nsIndex, itemType, nullptr, entryIndex, *this);
			entryIndex += span;
			if(err == ESP_OK && isIterableItem() && !isMultipageBlob()) {
				return true;
			}
		} while(err != ESP_ERR_NVS_NOT_FOUND);

		entryIndex = 0;
	}

	done = true;
	return false;
}

String ItemIterator::nsName() const
{
	auto it = std::find(container.mNamespaces.begin(), container.mNamespaces.end(), nsIndex);
	return it ? it->mName : nullptr;
}

} // namespace nvs
