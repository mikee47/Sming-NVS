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
#include "ItemIterator.hpp"
#include "Page.hpp"
#include "PageManager.hpp"

namespace nvs
{
class Handle;
using HandlePtr = std::unique_ptr<Handle>;

/**
 * @brief Manages data storage within a single NVS partition
 */
class Container : public intrusive_list_node<Container>
{
	enum class State {
		INVALID,
		ACTIVE,
	};

	struct NamespaceEntry : public intrusive_list_node<NamespaceEntry> {
		char mName[Item::MAX_KEY_LENGTH + 1];
		uint8_t mIndex;

		bool operator==(uint8_t index) const
		{
			return index == mIndex;
		}

		bool operator==(const String& name) const
		{
			return name.equalsIgnoreCase(mName);
		}
	};

	struct UsedPageNode : public intrusive_list_node<UsedPageNode> {
		Page* mPage;
	};

	struct BlobIndexNode : public intrusive_list_node<BlobIndexNode> {
		char key[Item::MAX_KEY_LENGTH + 1];
		uint8_t nsIndex;
		uint8_t chunkCount;
		VerOffset chunkStart;
	};

	using TBlobIndexList = intrusive_list<BlobIndexNode>;

public:
	Container(PartitionPtr& partition) : mPartition(std::move(partition))
	{
	}

	~Container()
	{
		assert(checkNoHandlesInUse());
		mNamespaces.clearAndFreeNodes();
	}

	bool init();

	bool operator==(const String& part_name) const
	{
		return *mPartition == part_name;
	}

	bool isValid() const
	{
		return mState == State::ACTIVE;
	}

	bool createOrOpenNamespace(const String& nsName, bool canCreate, uint8_t& nsIndex);

	HandlePtr openHandle(const String& nsName, OpenMode openMode);

	bool writeItem(uint8_t nsIndex, ItemType datatype, const String& key, const void* data, size_t dataSize);

	bool readItem(uint8_t nsIndex, ItemType datatype, const String& key, void* data, size_t dataSize);

	String readItem(uint8_t nsIndex, ItemType datatype, const String& key);

	bool getItemDataSize(uint8_t nsIndex, ItemType datatype, const String& key, size_t& dataSize);

	bool eraseItem(uint8_t nsIndex, ItemType datatype, const String& key);

	template <typename T> bool writeItem(uint8_t nsIndex, const String& key, const T& value)
	{
		return writeItem(nsIndex, itemTypeOf(value), key, &value, sizeof(value));
	}

	template <typename T> bool readItem(uint8_t nsIndex, const String& key, T& value)
	{
		return readItem(nsIndex, itemTypeOf(value), key, &value, sizeof(value));
	}

	bool eraseItem(uint8_t nsIndex, const String& key)
	{
		return eraseItem(nsIndex, ItemType::ANY, key);
	}

	bool eraseNamespace(uint8_t nsIndex);

	const Storage::Partition& partition() const
	{
		return *mPartition;
	}

	bool writeMultiPageBlob(uint8_t nsIndex, const String& key, const void* data, size_t dataSize,
							VerOffset chunkStart);

	bool readMultiPageBlob(uint8_t nsIndex, const String& key, void* data, size_t dataSize);

	bool cmpMultiPageBlob(uint8_t nsIndex, const String& key, const void* data, size_t dataSize);

	bool eraseMultiPageBlob(uint8_t nsIndex, const String& key, VerOffset chunkStart = VerOffset::VER_ANY);

	void debugDump();

	void debugCheck();

	bool fillStats(nvs_stats_t& nvsStats);

	bool calcEntriesInNamespace(uint8_t nsIndex, size_t& usedEntries);

	ItemIterator findEntry(const String& namespace_name = nullptr, ItemType itemType = ItemType::ANY)
	{
		return ItemIterator(*this, namespace_name, itemType);
	}

	esp_err_t lastError() const
	{
		return mLastError;
	}

	uint16_t handleCount() const
	{
		return mHandleCount;
	}

	size_t pageCount() const
	{
		return mPageManager.getPageCount();
	}

	bool checkNoHandlesInUse();

private:
	friend Handle;
	friend ItemIterator;

	void handle_destroyed()
	{
		assert(mHandleCount > 0);
		--mHandleCount;
	}

	Page& getCurrentPage()
	{
		return mPageManager.back();
	}

	bool populateBlobIndices(TBlobIndexList& blobIdxList);

	void eraseOrphanDataBlobs(TBlobIndexList& blobIdxList);

	bool findItem(uint8_t nsIndex, ItemType datatype, const String& key, Page*& page, Item& item,
				  uint8_t chunkIdx = Page::CHUNK_ANY, VerOffset chunkStart = VerOffset::VER_ANY);

	PartitionPtr mPartition;
	uint16_t mHandleCount{0};
	uint16_t mPageCount{0};
	PageManager mPageManager;
	intrusive_list<NamespaceEntry> mNamespaces;
	CompressedEnumTable<bool, 1, 256> mNamespaceUsage;
	State mState{State::INVALID};
	esp_err_t mLastError{ESP_OK};
};

} // namespace nvs

static constexpr nvs::OpenMode NVS_READONLY{nvs::OpenMode::ReadOnly};
static constexpr nvs::OpenMode NVS_READWRITE{nvs::OpenMode::ReadWrite};
