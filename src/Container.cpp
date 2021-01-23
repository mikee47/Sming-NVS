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

#include "include/Nvs/Container.hpp"
#include "include/Nvs/Handle.hpp"
#include "include/Nvs/PartitionManager.hpp"
#include <WHashMap.h>

namespace nvs
{
Container::~Container()
{
	assert(checkNoHandlesInUse());
	mNamespaces.clearAndFreeNodes();
	partitionManager.invalidateContainer(this);
}

bool Container::populateBlobIndices(TBlobIndexList& blobIdxList)
{
	for(auto& page : mPageManager) {
		size_t itemIndex = 0;
		Item item;

		/* If the power went off just after writing a blob index, the duplicate detection
         * logic in pagemanager will remove the earlier index. So we should never find a
         * duplicate index at this point */

		while(page.findItem(Page::NS_ANY, ItemType::BLOB_IDX, nullptr, itemIndex, item) == ESP_OK) {
			auto entry = new(std::nothrow) BlobIndexNode;
			if(entry == nullptr) {
				nvs_errno = ESP_ERR_NO_MEM;
				return false;
			}

			item.getKey(entry->key, sizeof(entry->key));
			entry->nsIndex = item.nsIndex;
			entry->chunkStart = item.blobIndex.chunkStart;
			entry->chunkCount = item.blobIndex.chunkCount;

			blobIdxList.push_back(entry);
			itemIndex += item.span;
		}
	}

	nvs_errno = ESP_OK;
	return true;
}

void Container::eraseOrphanDataBlobs(TBlobIndexList& blobIdxList)
{
	for(auto& page : mPageManager) {
		size_t itemIndex{0};
		Item item;
		/* Chunks with same <ns,key> and with chunkIndex in the following ranges
         * belong to same family.
         * 1) VER_0_OFFSET <= chunkIndex < VER_1_OFFSET-1 => Version0 chunks
         * 2) VER_1_OFFSET <= chunkIndex < VER_ANY => Version1 chunks
         */
		while(page.findItem(Page::NS_ANY, ItemType::BLOB_DATA, nullptr, itemIndex, item) == ESP_OK) {
			auto iter = std::find_if(blobIdxList.begin(), blobIdxList.end(), [=](const BlobIndexNode& e) -> bool {
				return (item == e.key) && (item.nsIndex == e.nsIndex) && (item.chunkIndex >= uint8_t(e.chunkStart)) &&
					   (item.chunkIndex < uint8_t(e.chunkStart) + e.chunkCount);
			});
			if(!iter) {
				page.eraseItem(item.nsIndex, item.datatype, item.key, item.chunkIndex);
			}
			itemIndex += item.span;
		}
	}
}

bool Container::checkNoHandlesInUse()
{
	if(mHandleCount == 0) {
		nvs_errno = ESP_OK;
		return true;
	}

	debug_e("Handles in use, cannot init");
	nvs_errno = ESP_ERR_NVS_INVALID_STATE;
	return false;
}

bool Container::init()
{
	if(!checkNoHandlesInUse()) {
		assert(false);
		return false;
	}

	nvs_errno = mPageManager.load(*mPartition);
	if(nvs_errno != ESP_OK) {
		mState = State::INVALID;
		return false;
	}

	// load namespaces list
	mNamespaces.clearAndFreeNodes();
	mNamespaceUsage.clear();
	for(auto& page : mPageManager) {
		size_t itemIndex = 0;
		Item item;
		while(page.findItem(Page::NS_INDEX, ItemType::U8, nullptr, itemIndex, item) == ESP_OK) {
			auto* entry = new(std::nothrow) NamespaceEntry;

			if(!entry) {
				mState = State::INVALID;
				nvs_errno = ESP_ERR_NO_MEM;
				return false;
			}

			item.getKey(entry->mName, sizeof(entry->mName));
			item.getValue(entry->mIndex);
			mNamespaces.push_back(entry);
			mNamespaceUsage.set(entry->mIndex, true);
			itemIndex += item.span;
		}
	}
	mNamespaceUsage.set(0, true);
	mNamespaceUsage.set(255, true);
	mState = State::ACTIVE;

	// Populate list of multi-page index entries.
	TBlobIndexList blobIdxList;
	if(!populateBlobIndices(blobIdxList)) {
		mState = State::INVALID;
		return false;
	}

	// Remove the entries for which there is no parent multi-page index.
	eraseOrphanDataBlobs(blobIdxList);

	// Purge the blob index list
	blobIdxList.clearAndFreeNodes();

#ifdef ENABLE_NVS_DEBUGCHECK
	debugCheck();
#endif

	nvs_errno = ESP_OK;
	return true;
}

bool Container::findItem(uint8_t nsIndex, ItemType datatype, const String& key, Page*& page, Item& item,
						 uint8_t chunkIdx, VerOffset chunkStart)
{
	for(auto it = std::begin(mPageManager); it != std::end(mPageManager); ++it) {
		size_t itemIndex = 0;
		nvs_errno = it->findItem(nsIndex, datatype, key, itemIndex, item, chunkIdx, chunkStart);
		if(nvs_errno == ESP_OK) {
			page = it;
			return true;
		}
	}

	nvs_errno = ESP_ERR_NVS_NOT_FOUND;
	return false;
}

bool Container::writeMultiPageBlob(uint8_t nsIndex, const String& key, const void* data, size_t dataSize,
								   VerOffset chunkStart)
{
	/* Check how much maximum data can be accommodated**/
	uint32_t max_pages = std::min(uint32_t(mPageManager.getPageCount() - 1), uint32_t(Page::CHUNK_ANY - 1) / 2U);

	if(dataSize > max_pages * Page::CHUNK_MAX_SIZE) {
		nvs_errno = ESP_ERR_NVS_VALUE_TOO_LONG;
		return false;
	}

	uint8_t chunkCount{0};
	intrusive_list<UsedPageNode> usedPages;
	size_t remainingSize = dataSize;
	size_t offset{0};

	do {
		Page& page = getCurrentPage();
		size_t tailroom = page.getVarDataTailroom();
		size_t chunkSize = 0;
		if(chunkCount == 0 && (tailroom == 0 || tailroom < dataSize) && tailroom < (Page::CHUNK_MAX_SIZE / 10)) {
			/** This is the first chunk and tailroom is too small ***/
			if(page.state() != Page::PageState::FULL) {
				nvs_errno = page.markFull();
				if(nvs_errno != ESP_OK) {
					return false;
				}
			}
			nvs_errno = mPageManager.requestNewPage();
			if(nvs_errno != ESP_OK) {
				return false;
			}
			if(getCurrentPage().getVarDataTailroom() == tailroom) {
				/* We got the same page or we are not improving.*/
				nvs_errno = ESP_ERR_NVS_NOT_ENOUGH_SPACE;
				return false;
			}
			continue;
		}
		if(tailroom == 0) {
			nvs_errno = ESP_ERR_NVS_NOT_ENOUGH_SPACE;
			break;
		}

		/* Split the blob into two and store the chunk of available size onto the current page */
		assert(tailroom != 0);
		chunkSize = (remainingSize > tailroom) ? tailroom : remainingSize;
		remainingSize -= chunkSize;

		nvs_errno = page.writeItem(nsIndex, ItemType::BLOB_DATA, key, static_cast<const uint8_t*>(data) + offset,
								   chunkSize, uint8_t(chunkStart) + chunkCount);
		chunkCount++;
		assert(nvs_errno != ESP_ERR_NVS_PAGE_FULL);
		if(nvs_errno != ESP_OK) {
			break;
		}

		auto node = new(std::nothrow) UsedPageNode;
		if(node == nullptr) {
			nvs_errno = ESP_ERR_NO_MEM;
			break;
		}
		node->mPage = &page;
		usedPages.push_back(node);
		if(remainingSize != 0 || (tailroom - chunkSize) < Page::ENTRY_SIZE) {
			if(page.state() != Page::PageState::FULL) {
				nvs_errno = page.markFull();
				if(nvs_errno != ESP_OK) {
					break;
				}
			}
			nvs_errno = mPageManager.requestNewPage();
			if(nvs_errno != ESP_OK) {
				break;
			}
		}

		offset += chunkSize;
		if(remainingSize == 0) {
			/* All pages are stored. Now store the index.*/
			Item item;
			item.blobIndex.dataSize = dataSize;
			item.blobIndex.chunkCount = chunkCount;
			item.blobIndex.chunkStart = chunkStart;

			nvs_errno = getCurrentPage().writeItem(nsIndex, ItemType::BLOB_IDX, key, item.data, sizeof(item.data));
			assert(nvs_errno != ESP_ERR_NVS_PAGE_FULL);
			break;
		}
	} while(true);

	if(nvs_errno != ESP_OK) {
		/* Anything failed, then we should erase all the written chunks*/
		int ii = 0;
		for(auto it = std::begin(usedPages); it != std::end(usedPages); it++) {
			it->mPage->eraseItem(nsIndex, ItemType::BLOB_DATA, key, ii++);
		}
	}
	usedPages.clearAndFreeNodes();

	return nvs_errno == ESP_OK;
}

bool Container::writeItem(uint8_t nsIndex, ItemType datatype, const String& key, const void* data, size_t dataSize)
{
	if(mState != State::ACTIVE) {
		nvs_errno = ESP_ERR_NVS_NOT_INITIALIZED;
		return false;
	}

	Page* findPage{nullptr};
	Item item;

	if(!findItem(nsIndex, (datatype == ItemType::BLOB) ? ItemType::BLOB_IDX : datatype, key, findPage, item)) {
		if(nvs_errno != ESP_ERR_NVS_NOT_FOUND) {
			return false;
		}
	}

	if(datatype == ItemType::BLOB) {
		VerOffset prevStart, nextStart;
		prevStart = nextStart = VerOffset::VER_0_OFFSET;
		if(findPage) {
			// Do a sanity check that the item in question is actually being modified.
			// If it isn't, it is cheaper to purposefully not write out new data.
			// since it may invoke an erasure of flash.
			if(cmpMultiPageBlob(nsIndex, key, data, dataSize)) {
				return true;
			}

			if(findPage->state() == Page::PageState::UNINITIALIZED || findPage->state() == Page::PageState::INVALID) {
				assert(findItem(nsIndex, datatype, key, findPage, item));
			}
			/* Get the version of the previous index with same <ns,key> */
			prevStart = item.blobIndex.chunkStart;
			assert(prevStart == VerOffset::VER_0_OFFSET || prevStart == VerOffset::VER_1_OFFSET);

			/* Toggle the version by changing the offset */
			nextStart = (prevStart == VerOffset::VER_1_OFFSET) ? VerOffset::VER_0_OFFSET : VerOffset::VER_1_OFFSET;
		}

		/* Write the blob with new version*/
		if(!writeMultiPageBlob(nsIndex, key, data, dataSize, nextStart)) {
			if(nvs_errno == ESP_ERR_NVS_PAGE_FULL) {
				nvs_errno = ESP_ERR_NVS_NOT_ENOUGH_SPACE;
			}
			return false;
		}

		if(findPage) {
			/* Erase the blob with earlier version*/
			if(!eraseMultiPageBlob(nsIndex, key, prevStart)) {
				if(nvs_errno == ESP_ERR_FLASH_OP_FAIL) {
					nvs_errno = ESP_ERR_NVS_REMOVE_FAILED;
				}
				return false;
			}

			findPage = nullptr;
		} else {
			/* Support for earlier versions where BLOBS were stored without index */
			if(!findItem(nsIndex, datatype, key, findPage, item)) {
				if(nvs_errno != ESP_ERR_NVS_NOT_FOUND) {
					return false;
				}
			}
		}
	} else {
		// Do a sanity check that the item in question is actually being modified.
		// If it isn't, it is cheaper to purposefully not write out new data.
		// since it may invoke an erasure of flash.
		if(findPage != nullptr && findPage->cmpItem(nsIndex, datatype, key, data, dataSize) == ESP_OK) {
			nvs_errno = ESP_OK;
			return true;
		}

		Page& page = getCurrentPage();
		nvs_errno = page.writeItem(nsIndex, datatype, key, data, dataSize);
		if(nvs_errno == ESP_ERR_NVS_PAGE_FULL) {
			if(page.state() != Page::PageState::FULL) {
				nvs_errno = page.markFull();
				if(nvs_errno != ESP_OK) {
					return false;
				}
			}
			nvs_errno = mPageManager.requestNewPage();
			if(nvs_errno != ESP_OK) {
				return false;
			}

			nvs_errno = getCurrentPage().writeItem(nsIndex, datatype, key, data, dataSize);
			if(nvs_errno == ESP_ERR_NVS_PAGE_FULL) {
				nvs_errno = ESP_ERR_NVS_NOT_ENOUGH_SPACE;
				return false;
			}
			if(nvs_errno != ESP_OK) {
				return false;
			}
		} else if(nvs_errno != ESP_OK) {
			return false;
		}
	}

	if(findPage) {
		if(findPage->state() == Page::PageState::UNINITIALIZED || findPage->state() == Page::PageState::INVALID) {
			assert(findItem(nsIndex, datatype, key, findPage, item));
		}
		nvs_errno = findPage->eraseItem(nsIndex, datatype, key);
		if(nvs_errno != ESP_OK) {
			if(nvs_errno == ESP_ERR_FLASH_OP_FAIL) {
				nvs_errno = ESP_ERR_NVS_REMOVE_FAILED;
			}
			return false;
		}
	}

#ifdef ENABLE_NVS_DEBUGCHECK
	debugCheck();
#endif

	nvs_errno = ESP_OK;
	return true;
}

bool Container::createOrOpenNamespace(const String& nsName, bool canCreate, uint8_t& nsIndex)
{
	if(mState != State::ACTIVE) {
		nvs_errno = ESP_ERR_NVS_NOT_INITIALIZED;
		return false;
	}

	auto it = std::find(mNamespaces.begin(), mNamespaces.end(), nsName);
	if(it == std::end(mNamespaces)) {
		if(!canCreate) {
			nvs_errno = ESP_ERR_NVS_NOT_FOUND;
			return false;
		}

		uint8_t ns;
		for(ns = 1; ns < 255; ++ns) {
			if(mNamespaceUsage.get(ns) == false) {
				break;
			}
		}

		if(ns == 255) {
			nvs_errno = ESP_ERR_NVS_NOT_ENOUGH_SPACE;
			return false;
		}

		NamespaceEntry* entry = new(std::nothrow) NamespaceEntry;
		if(entry == nullptr) {
			nvs_errno = ESP_ERR_NO_MEM;
			return false;
		}

		if(!writeItem(Page::NS_INDEX, ItemType::U8, nsName, &ns, sizeof(ns))) {
			return false;
		}

		mNamespaceUsage.set(ns, true);
		nsIndex = ns;

		entry->mIndex = ns;
		strncpy(entry->mName, nsName.c_str(), sizeof(entry->mName) - 1);
		entry->mName[sizeof(entry->mName) - 1] = '\0';
		mNamespaces.push_back(entry);

	} else {
		nsIndex = it->mIndex;
	}

	nvs_errno = ESP_OK;
	return true;
}

bool Container::readMultiPageBlob(uint8_t nsIndex, const String& key, void* data, size_t dataSize)
{
	Item item;
	Page* findPage{nullptr};

	/* First read the blob index */
	if(!findItem(nsIndex, ItemType::BLOB_IDX, key, findPage, item)) {
		return false;
	}

	uint8_t chunkCount = item.blobIndex.chunkCount;
	VerOffset chunkStart = item.blobIndex.chunkStart;
	size_t offset = 0;

	assert(dataSize == item.blobIndex.dataSize);

	/* Now read corresponding chunks */
	for(uint8_t chunkNum = 0; chunkNum < chunkCount; chunkNum++) {
		if(!findItem(nsIndex, ItemType::BLOB_DATA, key, findPage, item, uint8_t(chunkStart) + chunkNum)) {
			if(nvs_errno == ESP_ERR_NVS_NOT_FOUND) {
				break;
			}
			return false;
		}
		nvs_errno = findPage->readItem(nsIndex, ItemType::BLOB_DATA, key, static_cast<uint8_t*>(data) + offset,
									   item.varLength.dataSize, uint8_t(chunkStart) + chunkNum);
		if(nvs_errno != ESP_OK) {
			return false;
		}
		assert(uint8_t(chunkStart) + chunkNum == item.chunkIndex);
		offset += item.varLength.dataSize;
	}

	if(nvs_errno == ESP_OK) {
		assert(offset == dataSize);
		return true;
	}

	if(nvs_errno == ESP_ERR_NVS_NOT_FOUND) {
		eraseMultiPageBlob(nsIndex, key); // cleanup if a chunk is not found
	}

	nvs_errno = ESP_ERR_NVS_NOT_FOUND;
	return false;
}

bool Container::cmpMultiPageBlob(uint8_t nsIndex, const String& key, const void* data, size_t dataSize)
{
	Item item;
	Page* findPage = nullptr;

	/* First read the blob index */
	if(!findItem(nsIndex, ItemType::BLOB_IDX, key, findPage, item)) {
		return false;
	}

	uint8_t chunkCount = item.blobIndex.chunkCount;
	VerOffset chunkStart = item.blobIndex.chunkStart;
	size_t readSize = item.blobIndex.dataSize;
	size_t offset = 0;

	if(dataSize != readSize) {
		nvs_errno = ESP_ERR_NVS_CONTENT_DIFFERS;
		return false;
	}

	/* Now read corresponding chunks */
	for(uint8_t chunkNum = 0; chunkNum < chunkCount; chunkNum++) {
		if(!findItem(nsIndex, ItemType::BLOB_DATA, key, findPage, item, uint8_t(chunkStart) + chunkNum)) {
			if(nvs_errno == ESP_ERR_NVS_NOT_FOUND) {
				break;
			}
			return false;
		}
		nvs_errno = findPage->cmpItem(nsIndex, ItemType::BLOB_DATA, key, static_cast<const uint8_t*>(data) + offset,
									  item.varLength.dataSize, uint8_t(chunkStart) + chunkNum);
		if(nvs_errno != ESP_OK) {
			return false;
		}
		assert(uint8_t(chunkStart) + chunkNum == item.chunkIndex);
		offset += item.varLength.dataSize;
	}

	if(nvs_errno == ESP_OK) {
		assert(offset == dataSize);
		return true;
	}

	return false;
}

bool Container::readItem(uint8_t nsIndex, ItemType datatype, const String& key, void* data, size_t dataSize)
{
	if(mState != State::ACTIVE) {
		nvs_errno = ESP_ERR_NVS_NOT_INITIALIZED;
		return false;
	}

	Item item;
	Page* findPage = nullptr;
	if(datatype == ItemType::BLOB) {
		if(readMultiPageBlob(nsIndex, key, data, dataSize)) {
			return true;
		}
		if(nvs_errno != ESP_ERR_NVS_NOT_FOUND) {
			return false;
		} // else check if the blob is stored with earlier version format without index
	}

	if(!findItem(nsIndex, datatype, key, findPage, item)) {
		return false;
	}

	nvs_errno = findPage->readItem(nsIndex, datatype, key, data, dataSize);
	return nvs_errno == ESP_OK;
}

String Container::readItem(uint8_t nsIndex, ItemType datatype, const String& key)
{
	String s;

	size_t dataSize{0};
	if(getItemDataSize(nsIndex, datatype, key, dataSize)) {
		auto len = dataSize;
		if(datatype == ItemType::SZ && dataSize > 0) {
			--len;
		}
		if(!s.setLength(len)) {
			nvs_errno = ESP_ERR_NO_MEM;
		} else if(!readItem(nsIndex, datatype, key, s.begin(), dataSize)) {
			s = nullptr;
		}
	}

	return s;
}

bool Container::eraseMultiPageBlob(uint8_t nsIndex, const String& key, VerOffset chunkStart)
{
	if(mState != State::ACTIVE) {
		nvs_errno = ESP_ERR_NVS_NOT_INITIALIZED;
		return false;
	}

	Item item;
	Page* findPage = nullptr;

	if(!findItem(nsIndex, ItemType::BLOB_IDX, key, findPage, item, Page::CHUNK_ANY, chunkStart)) {
		return false;
	}

	/* Erase the index first and make children blobs orphan*/
	nvs_errno = findPage->eraseItem(nsIndex, ItemType::BLOB_IDX, key, Page::CHUNK_ANY, chunkStart);
	if(nvs_errno != ESP_OK) {
		return false;
	}

	uint8_t chunkCount = item.blobIndex.chunkCount;

	if(chunkStart == VerOffset::VER_ANY) {
		chunkStart = item.blobIndex.chunkStart;
	} else {
		assert(chunkStart == item.blobIndex.chunkStart);
	}

	/* Now erase corresponding chunks*/
	for(uint8_t chunkNum = 0; chunkNum < chunkCount; chunkNum++) {
		if(!findItem(nsIndex, ItemType::BLOB_DATA, key, findPage, item, uint8_t(chunkStart) + chunkNum)) {
			if(nvs_errno != ESP_ERR_NVS_NOT_FOUND) {
				return false;
			}
			continue; // Keep erasing other chunks
		}

		nvs_errno = findPage->eraseItem(nsIndex, ItemType::BLOB_DATA, key, uint8_t(chunkStart) + chunkNum);
		if(nvs_errno != ESP_OK) {
			return false;
		}
	}

	nvs_errno = ESP_OK;
	return true;
}

bool Container::eraseItem(uint8_t nsIndex, ItemType datatype, const String& key)
{
	if(mState != State::ACTIVE) {
		nvs_errno = ESP_ERR_NVS_NOT_INITIALIZED;
		return false;
	}

	if(datatype == ItemType::BLOB) {
		return eraseMultiPageBlob(nsIndex, key);
	}

	Item item;
	Page* findPage = nullptr;
	if(!findItem(nsIndex, datatype, key, findPage, item)) {
		return false;
	}

	if(item.datatype == ItemType::BLOB_DATA || item.datatype == ItemType::BLOB_IDX) {
		return eraseMultiPageBlob(nsIndex, key);
	}

	nvs_errno = findPage->eraseItem(nsIndex, datatype, key);
	return nvs_errno == ESP_OK;
}

bool Container::eraseNamespace(uint8_t nsIndex)
{
	if(mState != State::ACTIVE) {
		nvs_errno = ESP_ERR_NVS_NOT_INITIALIZED;
		return false;
	}

	for(auto it = std::begin(mPageManager); it != std::end(mPageManager); ++it) {
		while(true) {
			auto err = it->eraseItem(nsIndex, ItemType::ANY, nullptr);
			if(err == ESP_ERR_NVS_NOT_FOUND) {
				break;
			}

			if(err != ESP_OK) {
				nvs_errno = err;
				return false;
			}
		}
	}

	nvs_errno = ESP_OK;
	return true;
}

bool Container::getItemDataSize(uint8_t nsIndex, ItemType datatype, const String& key, size_t& dataSize)
{
	if(datatype < ItemType::VARIABLE) {
		dataSize = size_t(datatype) & NVS_TYPE_SIZE;
		return true;
	}

	dataSize = 0;

	if(mState != State::ACTIVE) {
		nvs_errno = ESP_ERR_NVS_NOT_INITIALIZED;
		return false;
	}

	Item item;
	Page* findPage = nullptr;
	if(findItem(nsIndex, datatype, key, findPage, item)) {
		dataSize = item.varLength.dataSize;
		return true;
	}

	if(datatype != ItemType::BLOB) {
		return false;
	}

	if(findItem(nsIndex, ItemType::BLOB_IDX, key, findPage, item)) {
		dataSize = item.blobIndex.dataSize;
		return true;
	}

	return false;
}

void Container::debugDump()
{
	for(auto& page : mPageManager) {
		page.debugDump();
	}
}

void Container::debugCheck()
{
	HashMap<String, Page*> keys;

	for(auto& page : mPageManager) {
		size_t itemIndex = 0;
		size_t usedCount = 0;
		Item item;
		while(page.findItem(Page::NS_ANY, ItemType::ANY, nullptr, itemIndex, item) == ESP_OK) {
			String k;
			k += item.nsIndex;
			k += '_';
			k += unsigned(item.datatype);
			k += '_';
			k += item.key;
			k += '_';
			k += item.chunkIndex;
			if(keys.contains(k)) {
				debug_e("Duplicate key: %s", k.c_str());
				debugDump();
				assert(false);
			}
			keys[k] = &page;
			itemIndex += item.span;
			usedCount += item.span;
		}
		assert(usedCount == page.getUsedEntryCount());
	}
}

bool Container::fillStats(nvs_stats_t& nvsStats)
{
	nvsStats.namespaceCount = mNamespaces.size();
	nvs_errno = mPageManager.fillStats(nvsStats);
	return nvs_errno == ESP_OK;
}

bool Container::calcEntriesInNamespace(uint8_t nsIndex, size_t& usedEntries)
{
	usedEntries = 0;

	if(mState != State::ACTIVE) {
		nvs_errno = ESP_ERR_NVS_NOT_INITIALIZED;
		return false;
	}

	for(auto it = std::begin(mPageManager); it != std::end(mPageManager); ++it) {
		size_t itemIndex = 0;
		Item item;
		while(true) {
			auto err = it->findItem(nsIndex, ItemType::ANY, nullptr, itemIndex, item);
			if(err == ESP_ERR_NVS_NOT_FOUND) {
				break;
			}
			if(err != ESP_OK) {
				nvs_errno = err;
				return false;
			}

			usedEntries += item.span;
			itemIndex += item.span;
			if(itemIndex >= it->ENTRY_COUNT) {
				break;
			}
		}
	}

	nvs_errno = ESP_OK;
	return true;
}

HandlePtr Container::openHandle(const String& nsName, OpenMode openMode)
{
	if(nsName.length() == 0 || nsName.length() > NVS_KEY_NAME_MAX_SIZE) {
		nvs_errno = ESP_ERR_INVALID_ARG;
		return nullptr;
	}

	uint8_t nsIndex;
	if(!createOrOpenNamespace(nsName, openMode == OpenMode::ReadWrite, nsIndex)) {
		return nullptr;
	}

	auto handle = new(std::nothrow) Handle(*this, nsIndex, openMode == OpenMode::ReadOnly);

	if(handle == nullptr) {
		nvs_errno = ESP_ERR_NO_MEM;
	} else {
		++mHandleCount;
		nvs_errno = ESP_OK;
	}

	return HandlePtr(handle);
}

} // namespace nvs
