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

class ItemIterator : public Item
{
public:
	ItemIterator(Container& container, ItemType itemType)
: container(container), itemType(itemType)
{
reset();
}

	ItemIterator(Container& container, const String& nsName, ItemType itemType);

	void reset();

	bool next();

	explicit operator bool() const
	{
		return page && !done;
	}

	String nsName() const;

private:
	Container& container;
	ItemType itemType;
	uint8_t nsIndex{Page::NS_ANY};
	size_t entryIndex{0};
	TPageListIterator page;
	bool done{false};
};

} // namespace nvs
