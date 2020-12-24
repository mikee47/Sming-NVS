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

	using NamespaceList = intrusive_list<NamespaceEntry>;

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
	Container(PartitionPtr&& partition) : mPartition(std::move(partition))
	{
	}

	Container(PartitionPtr& partition) : mPartition(std::move(partition))
	{
	}

	~Container();

	/**
	 * @brief Initialise container by reading and caching partition contents
	 *
	 * Fails if there are any open handles.
	 */
	bool init();

	bool operator==(const String& part_name) const
	{
		return *mPartition == part_name;
	}

	/**
	 * @brief Determine if this container has been initialised successfully
	 */
	bool isValid() const
	{
		return mState == State::ACTIVE;
	}

	/**
	 * @brief Open an existing namespace or create a new one
	 * @param nsName Namespace to open/create
	 * @param canCreate true if a new namespace entry may be created
	 * @param nsIndex On return, contains index of namespace
	 * @retval bool
	 */
	bool createOrOpenNamespace(const String& nsName, bool canCreate, uint8_t& nsIndex);

	/**
	 * @brief Open a new handle on this storage container
	 * @param nsName Namespace to qualify all entries
	 * @param openMode If read-only, all write operations will fail
	 * @retval HandlePtr Created handle
	 */
	HandlePtr openHandle(const String& nsName, OpenMode openMode);

	/**
	 * @name Write an entry
	 * @{
	 */
	template <typename T> bool writeItem(uint8_t nsIndex, const String& key, const T& value)
	{
		return writeItem(nsIndex, itemTypeOf(value), key, &value, sizeof(value));
	}

	bool writeItem(uint8_t nsIndex, ItemType datatype, const String& key, const void* data, size_t dataSize);
	/** @} */

	/**
	 * @name Read an entry
	 * @{
	 *
	 * @brief Method template to read an entry and deduce the data type
	 */
	template <typename T> bool readItem(uint8_t nsIndex, const String& key, T& value)
	{
		return readItem(nsIndex, itemTypeOf(value), key, &value, sizeof(value));
	}

	/**
	 * @brief Read an item into a user-supplied buffer, specify data type explicitly
	 */
	bool readItem(uint8_t nsIndex, ItemType datatype, const String& key, void* data, size_t dataSize);

	/**
	 * @brief Read an item into a String object, specify data type explicitly
	 */
	String readItem(uint8_t nsIndex, ItemType datatype, const String& key);
	/** @} */

	/**
	 * @brief Get size of stored value in bytes
	 */
	bool getItemDataSize(uint8_t nsIndex, ItemType datatype, const String& key, size_t& dataSize);

	/**
	 * @name Erase single value from container
	 * @{
	 *
	 * @brief Erase item matching data type and key
	 * @param datatype May be ItemType::ANY to match any entry
	 * @param key May be nullptr to match any key
	 */
	bool eraseItem(uint8_t nsIndex, ItemType datatype, const String& key);

	/**
	 * @brief Erase item matching name only
	 * @param key May be nullptr to match any key
	 */
	bool eraseItem(uint8_t nsIndex, const String& key)
	{
		return eraseItem(nsIndex, ItemType::ANY, key);
	}
	/** @} */

	/**
	 * @brief Erase all entries matching a single namespace
	 */
	bool eraseNamespace(uint8_t nsIndex);

	/**
	 * @brief Get reference to underlying storage partition
	 */
	Partition& partition() const
	{
		assert(mPartition != nullptr);
		return *mPartition;
	}

	/**
	 * @brief Print contents of container for debugging
	 */
	void debugDump();

	/**
	 * @brief Run extended check on container contents
	 */
	void debugCheck();

	/**
	 * @brief Fetch statistics for this container
	 */
	bool fillStats(nvs_stats_t& nvsStats);

	/**
	 * @brief Determine number of used entries for a given namespace
	 */
	bool calcEntriesInNamespace(uint8_t nsIndex, size_t& usedEntries);

	/**
	 * @name STL iterator support
	 * @{
	 */
	ItemIterator begin()
	{
		return ItemIterator(*this, ItemType::ANY);
	}

	ItemIterator end()
	{
		return ItemIterator(*this, ItemType::UNK);
	}

	ItemIterator find(const String& nsName = nullptr, ItemType itemType = ItemType::ANY)
	{
		return ItemIterator(*this, nsName, itemType);
	}
	/** @} */

	/**
	 * @brief Get number of open handles
	 *
	 * Certain container operations are prohibited whilst there are open handles.
	 */
	size_t handleCount() const
	{
		return mHandleCount;
	}

	/**
	 * @brief Check and set error if any handles are in use
	 *
	 * Called before performing certain operations to ensure last error is set appropriately.
	 * A debug message is also logged.
	 */
	bool checkNoHandlesInUse();

	/**
	 * @brief Determine number of pages in use
	 */
	size_t pageCount() const
	{
		return mPageManager.getPageCount();
	}

	/**
	 * @brief Get reference to list of registered namespaces
	 */
	const NamespaceList& namespaces() const
	{
		return mNamespaces;
	}

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

	bool writeMultiPageBlob(uint8_t nsIndex, const String& key, const void* data, size_t dataSize,
							VerOffset chunkStart);

	bool readMultiPageBlob(uint8_t nsIndex, const String& key, void* data, size_t dataSize);

	bool cmpMultiPageBlob(uint8_t nsIndex, const String& key, const void* data, size_t dataSize);

	bool eraseMultiPageBlob(uint8_t nsIndex, const String& key, VerOffset chunkStart = VerOffset::VER_ANY);

	PartitionPtr mPartition;
	uint16_t mHandleCount{0};
	uint16_t mPageCount{0};
	PageManager mPageManager;
	NamespaceList mNamespaces;
	CompressedEnumTable<bool, 1, 256> mNamespaceUsage;
	State mState{State::INVALID};
};

} // namespace nvs

static constexpr nvs::OpenMode NVS_READONLY{nvs::OpenMode::ReadOnly};
static constexpr nvs::OpenMode NVS_READWRITE{nvs::OpenMode::ReadWrite};
