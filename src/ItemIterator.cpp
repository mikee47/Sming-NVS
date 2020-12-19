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
String ItemInfo::nsName() const
{
	auto it = std::find(mContainer.namespaces().begin(), mContainer.namespaces().end(), mItem.nsIndex);
	return it ? it->mName : nullptr;
}

ItemIterator::ItemIterator(Container& container, const String& nsName, ItemType itemType)
	: mItemInfo(container), mItemType(itemType)
{
	if(nsName) {
		mDone = !container.createOrOpenNamespace(nsName, false, mNsIndex);
	}
	if(!mDone) {
		if(itemType == ItemType::UNK) {
			mDone = true;
		} else {
			mPage = container.mPageManager.begin();
			next();
		}
	}
}

bool ItemIterator::next()
{
	if(!bool(*this)) {
		return false;
	}

	for(; mPage != mItemInfo.mContainer.mPageManager.end(); ++mPage) {
		esp_err_t err;
		do {
			err = mPage->findItem(mNsIndex, mItemType, nullptr, mEntryIndex, mItemInfo.mItem);
			mEntryIndex += mItemInfo.mItem.span;
			if(err == ESP_OK && mItemInfo.isIterableItem() && !mItemInfo.isMultipageBlob()) {
				return true;
			}
		} while(err != ESP_ERR_NVS_NOT_FOUND);

		mEntryIndex = 0;
	}

	mDone = true;
	return false;
}

} // namespace nvs
