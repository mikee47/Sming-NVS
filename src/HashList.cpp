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

#include "include/Nvs/HashList.hpp"

namespace nvs
{
void HashList::clear()
{
	for(auto it = mBlockList.begin(); it != mBlockList.end();) {
		auto tmp = it;
		++it;
		mBlockList.erase(tmp);
		delete static_cast<HashListBlock*>(tmp);
	}
}

HashList::HashListBlock::HashListBlock()
{
	static_assert(sizeof(HashListBlock) == HashListBlock::BYTE_SIZE, "cache block size calculation incorrect");
}

esp_err_t HashList::insert(const Item& item, size_t index)
{
	const uint32_t hash_24 = item.calculateCrc32WithoutValue() & 0xffffff;
	// add entry to the end of last block if possible
	if(!mBlockList.empty()) {
		auto& block = mBlockList.back();
		if(block.add(index, hash_24)) {
			return ESP_OK;
		}
	}

	// if the above failed, create a new block and add entry to it
	auto newBlock = new(std::nothrow) HashListBlock;
	if(newBlock == nullptr) {
		return ESP_ERR_NO_MEM;
	}

	mBlockList.push_back(newBlock);
	newBlock->add(index, hash_24);
	return ESP_OK;
}

void HashList::erase(size_t index, bool itemShouldExist)
{
	for(auto it = mBlockList.begin(); it != mBlockList.end();) {
		bool haveEntries = false;
		bool foundIndex = false;
		for(size_t i = 0; i < it->mCount; ++i) {
			auto& e = it->mNodes[i];
			if(e.mIndex == index) {
				e.invalidate();
				foundIndex = true;
				/* found the item and removed it */
			}
			if(e.isValid()) {
				haveEntries = true;
			}
			if(haveEntries && foundIndex) {
				/* item was found, and HashListBlock still has some items */
				return;
			}
		}
		/* no items left in HashListBlock, can remove */
		if(!haveEntries) {
			auto tmp = it;
			++it;
			mBlockList.erase(tmp);
			delete static_cast<HashListBlock*>(tmp);
		} else {
			++it;
		}
		if(foundIndex) {
			/* item was found and empty HashListBlock was removed */
			return;
		}
	}
	if(itemShouldExist) {
		assert(false && "item should have been present in cache");
	}
}

size_t HashList::find(size_t start, const Item& item)
{
	const uint32_t hash_24 = item.calculateCrc32WithoutValue() & 0xffffff;
	for(auto it = mBlockList.begin(); it != mBlockList.end(); ++it) {
		for(size_t index = 0; index < it->mCount; ++index) {
			auto& e = it->mNodes[index];
			if(e.matches(start, hash_24)) {
				return e.mIndex;
			}
		}
	}
	return SIZE_MAX;
}

} // namespace nvs
