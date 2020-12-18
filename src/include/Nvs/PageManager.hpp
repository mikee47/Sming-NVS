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

#include <memory>
#include "Item.hpp"
#include "Page.hpp"
#include "intrusive_list.h"

namespace nvs
{
using TPageList = intrusive_list<Page>;
using TPageListIterator = TPageList::iterator;

class PageManager
{
public:
	PageManager()
	{
	}

	esp_err_t load(Partition& partition);

	TPageListIterator begin()
	{
		return mPageList.begin();
	}

	TPageListIterator end()
	{
		return mPageList.end();
	}

	Page& back()
	{
		return mPageList.back();
	}

	uint32_t getPageCount()
	{
		return mPageCount;
	}

	esp_err_t requestNewPage();

	esp_err_t fillStats(nvs_stats_t& nvsStats);

protected:
	friend class Iterator;

	esp_err_t activatePage();

	TPageList mPageList;
	TPageList mFreePageList;
	std::unique_ptr<Page[]> mPages;
	uint32_t mPageCount;
	uint32_t mSeqNumber;
};

} // namespace nvs
