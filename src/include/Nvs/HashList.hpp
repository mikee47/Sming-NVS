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

#pragma once

#include "nvs.h"
#include "Item.hpp"
#include "intrusive_list.h"

namespace nvs
{
class HashList
{
public:
	HashList()
	{
	}

	HashList(const HashList& other) = delete;

	~HashList()
	{
		clear();
	}

	const HashList& operator=(const HashList& rhs) = delete;

	esp_err_t insert(const Item& item, size_t index);
	void erase(const size_t index, bool itemShouldExist = true);
	size_t find(size_t start, const Item& item);
	void clear();

	size_t getBlockCount()
	{
		return mBlockList.size();
	}

private:
	struct HashListNode {
		uint32_t mIndex : 8;
		uint32_t mHash : 24;

		bool isValid() const
		{
			return mIndex != 0xff;
		}

		void invalidate()
		{
			mIndex = 0xff;
		}

		bool matches(uint8_t startIndex, uint32_t hash) const
		{
			return mHash == hash && isValid() && mIndex >= startIndex;
		}
	};

	struct HashListBlock : public intrusive_list_node<HashList::HashListBlock> {
		HashListBlock();

		static const size_t BYTE_SIZE = 128;
		static const size_t ENTRY_COUNT =
			(BYTE_SIZE - sizeof(intrusive_list_node<HashListBlock>) - sizeof(size_t)) / sizeof(HashListNode);

		bool add(uint32_t index, uint32_t hash)
		{
			if(mCount >= ENTRY_COUNT) {
				return false;
			}
			mNodes[mCount++] = HashListNode{index, hash};
			return true;
		}

		size_t mCount{0};
		HashListNode mNodes[ENTRY_COUNT];
	};

	intrusive_list<HashListBlock> mBlockList;
}; // class HashList

} // namespace nvs
