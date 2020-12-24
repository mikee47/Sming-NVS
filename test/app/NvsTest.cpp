#include <NvsTest.h>
#include <Nvs/PartitionManager.hpp>

unsigned listContainer(nvs::Container& container)
{
	unsigned count{0};
	for(auto item : container) {
		debug_i("{ namespace: \"%s\", key: \"%s\", dataType: %s, dataSize: %u }", item.nsName().c_str(),
				item.key().c_str(), toString(item.type()).c_str(), item.dataSize());
		++count;
	}
	debug_i("%u items found", count);
	return count;
}

bool listContainer(const String& name)
{
	auto container = nvs::partitionManager.lookupContainer(name);
	if(container == nullptr) {
		return false;
	}

	listContainer(*container);
	return true;
}
