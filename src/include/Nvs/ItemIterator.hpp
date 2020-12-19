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

#pragma once

#include <memory>
#include "Item.hpp"
#include "PageManager.hpp"

namespace nvs
{
class Container;
class ItemIterator;

class ItemInfo
{
public:
	String nsName() const;

	String key() const
	{
		return mItem.key;
	}

	ItemType type() const
	{
		return mItem.datatype;
	}

	size_t dataSize() const
	{
		return mItem.dataSize();
	}

	explicit operator bool() const
	{
		return mItem.key[0] != '\0' && mItem.datatype != ItemType::UNK;
	}

	bool operator==(bool b) const
	{
		return bool(*this) == b;
	}

	bool operator==(const ItemInfo& other) const
	{
		return memcmp(&mItem, &other.mItem, sizeof(mItem)) == 0;
	}

private:
	friend ItemIterator;

	ItemInfo(Container& container) : mContainer(container)
	{
	}

	bool isIterableItem()
	{
		return (mItem.nsIndex != 0 && mItem.datatype != ItemType::BLOB && mItem.datatype != ItemType::BLOB_IDX);
	}

	bool isMultipageBlob()
	{
		return mItem.datatype == ItemType::BLOB_DATA && mItem.chunkIndex != uint8_t(VerOffset::VER_0_OFFSET) &&
			   mItem.chunkIndex != uint8_t(VerOffset::VER_1_OFFSET);
	}

	Container& mContainer;
	Item mItem;
};

class ItemIterator : public std::iterator<std::forward_iterator_tag, ItemInfo>
{
public:
	ItemIterator(Container& container, const String& nsName, ItemType itemType = ItemType::ANY);

	ItemIterator(Container& container, ItemType itemType) : ItemIterator(container, nullptr, itemType)
	{
	}

	explicit operator bool() const
	{
		return mPage && !mDone;
	}

	ItemIterator operator++(int)
	{
		auto result = *this;
		next();
		return result;
	}

	ItemIterator& operator++()
	{
		next();
		return *this;
	}

	bool operator==(const ItemIterator& other) const
	{
		if(mDone) {
			return other.mDone;
		}
		if(other.mDone) {
			return mDone;
		}
		return mPage == other.mPage && mEntryIndex == other.mEntryIndex;
	}

	bool operator!=(const ItemIterator& other) const
	{
		return !operator==(other);
	}

	const ItemInfo& operator*() const
	{
		return mItemInfo;
	}

	const ItemInfo* operator->() const
	{
		return mDone ? nullptr : &mItemInfo;
	}

private:
	void reset();
	bool next();

	ItemInfo mItemInfo;
	TPageListIterator mPage;
	size_t mEntryIndex{0};
	ItemType mItemType;
	uint8_t mNsIndex{Page::NS_ANY};
	bool mDone{false};
};

} // namespace nvs
