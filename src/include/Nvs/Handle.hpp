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

#include <string>
#include <memory>
#include <type_traits>

#include "nvs.h"
#include "Item.hpp"
#include "intrusive_list.h"

class ItemIterator;

namespace nvs
{
class Storage;

/**
 * @brief A handle allowing nvs-entry related operations on the NVS.
 *
 * @note The scope of this handle is the namespace of a particular partition. Outside that scope, nvs entries can't be accessed/altered.
 */
class Handle : public intrusive_list_node<Handle>
{
public:
	Handle(bool readOnly, uint8_t nsIndex, Storage& storage)
		: mStorage(storage), mNsIndex(nsIndex), mReadOnly(readOnly), valid(true)
	{
	}

	~Handle();

	/**
     * @brief      set value for given key
     *
     * Sets value for key. Note that physical storage will not be updated until nvs_commit function is called.
     *
     * @param[in]  key     Key name. Maximal length is (NVS_KEY_NAME_MAX_SIZE-1) characters. Shouldn't be empty.
     * @param[in]  value   The value to set. Allowed types are the ones declared in ItemType as well as enums.
     *                     For strings, the maximum length (including null character) is
     *                     4000 bytes.
     *                     Note that enums loose their type information when stored in NVS. Ensure that the correct
     *                     enum type is used during retrieval with \ref get_item!
     *
     * @return
     *             - ESP_OK if value was set successfully
     *             - ESP_ERR_NVS_READ_ONLY if storage handle was opened as read only
     *             - ESP_ERR_NVS_INVALID_NAME if key name doesn't satisfy constraints
     *             - ESP_ERR_NVS_NOT_ENOUGH_SPACE if there is not enough space in the
     *               underlying storage to save the value
     *             - ESP_ERR_NVS_REMOVE_FAILED if the value wasn't updated because flash
     *               write operation has failed. The value was written however, and
     *               update will be finished after re-initialization of nvs, provided that
     *               flash operation doesn't fail again.
     *             - ESP_ERR_NVS_VALUE_TOO_LONG if the string value is too long
     */
	template <typename T> esp_err_t set_item(const char* key, T value);
	esp_err_t set_string(const char* key, const char* value);

	/**
     * @brief      get value for given key
     *
     * These functions retrieve value for the key, given its name. If key does not
     * exist, or the requested variable type doesn't match the type which was used
     * when setting a value, an error is returned.
     *
     * In case of any error, out_value is not modified.
     *
     * @param[in]     key        Key name. Maximal length is (NVS_KEY_NAME_MAX_SIZE-1) characters. Shouldn't be empty.
     * @param         value      The output value. All integral types which are declared in ItemType as well as enums
     *                           are allowed. Note however that enums lost their type information when stored in NVS.
     *                           Ensure that the correct enum type is used during retrieval with \ref get_item!
     *
     * @return
     *             - ESP_OK if the value was retrieved successfully
     *             - ESP_ERR_NVS_NOT_FOUND if the requested key doesn't exist
     *             - ESP_ERR_NVS_INVALID_NAME if key name doesn't satisfy constraints
     *             - ESP_ERR_NVS_INVALID_LENGTH if length is not sufficient to store data
     */
	template <typename T> esp_err_t get_item(const char* key, T& value);

	/**
     * @brief       set variable length binary value for given key
     *
     * This family of functions set value for the key, given its name. Note that
     * actual storage will not be updated until nvs_commit function is called.
     *
     * @param[in]  key     Key name. Maximal length is (NVS_KEY_NAME_MAX_SIZE-1) characters. Shouldn't be empty.
     * @param[in]  blob    The blob value to set.
     * @param[in]  len     length of binary value to set, in bytes; Maximum length is
     *                     508000 bytes or (97.6% of the partition size - 4000) bytes
     *                     whichever is lower.
     *
     * @return
     *             - ESP_OK if value was set successfully
     *             - ESP_ERR_NVS_READ_ONLY if storage handle was opened as read only
     *             - ESP_ERR_NVS_INVALID_NAME if key name doesn't satisfy constraints
     *             - ESP_ERR_NVS_NOT_ENOUGH_SPACE if there is not enough space in the
     *               underlying storage to save the value
     *             - ESP_ERR_NVS_REMOVE_FAILED if the value wasn't updated because flash
     *               write operation has failed. The value was written however, and
     *               update will be finished after re-initialization of nvs, provided that
     *               flash operation doesn't fail again.
     *             - ESP_ERR_NVS_VALUE_TOO_LONG if the value is too long
     *
     * @note compare to \ref nvs_set_blob in nvs.h
     */
	esp_err_t set_blob(const char* key, const void* blob, size_t len);

	/**
     * @brief      get value for given key
     *
     * These functions retrieve the data of an entry, given its key. If key does not
     * exist, or the requested variable type doesn't match the type which was used
     * when setting a value, an error is returned.
     *
     * In case of any error, out_value is not modified.
     *
     * Both functions expect out_value to be a pointer to an already allocated variable
     * of the given type.
     *
     * It is suggested that nvs_get/set_str is used for zero-terminated C strings, and
     * nvs_get/set_blob used for arbitrary data structures.
     *
     * @param[in]     key        Key name. Maximal length is (NVS_KEY_NAME_MAX_SIZE-1) characters. Shouldn't be empty.
     * @param         out_str/   Pointer to the output value.
     *                out_blob
     * @param[inout]  length     A non-zero pointer to the variable holding the length of out_value.
     *                           It will be set to the actual length of the value
     *                           written. For nvs_get_str this includes the zero terminator.
     *
     * @return
     *             - ESP_OK if the value was retrieved successfully
     *             - ESP_ERR_NVS_NOT_FOUND if the requested key doesn't exist
     *             - ESP_ERR_NVS_INVALID_NAME if key name doesn't satisfy constraints
     *             - ESP_ERR_NVS_INVALID_LENGTH if length is not sufficient to store data
     */
	esp_err_t get_string(const char* key, char* out_str, size_t len);
	esp_err_t get_blob(const char* key, void* out_blob, size_t len);

	/**
     * @brief Looks up the size of an entry's data.
     *
     * For strings, this size includes the zero terminator.
     */
	esp_err_t get_item_size(ItemType datatype, const char* key, size_t& size);

	/**
     * @brief Erases an entry.
     */
	esp_err_t erase_item(const char* key);

	/**
     * Erases all entries in the scope of this handle. The scope may vary, depending on the implementation.
     *
     * @not If you want to erase the whole nvs flash (partition), refer to \ref
     */
	esp_err_t erase_all();

	/**
     * Commits all changes done through this handle so far.
     */
	esp_err_t commit();

	/**
     * @brief      Calculate all entries in the scope of the handle.
     *
     * @param[out]  used_entries Returns amount of used entries from a namespace on success.
     *
     *
     * @return
     *             - ESP_OK if the changes have been written successfully.
     *               Return param used_entries will be filled valid value.
     *             - ESP_ERR_NVS_NOT_INITIALIZED if the storage driver is not initialized.
     *               Return param used_entries will be filled 0.
     *             - ESP_ERR_INVALID_ARG if nvs_stats equal to NULL.
     *             - Other error codes from the underlying storage driver.
     *               Return param used_entries will be filled 0.
     */
	esp_err_t get_used_entry_count(size_t& usedEntries);

	esp_err_t getItemDataSize(ItemType datatype, const char* key, size_t& dataSize);

	void debugDump();

	esp_err_t fillStats(nvs_stats_t& nvsStats);

	esp_err_t calcEntriesInNamespace(size_t& usedEntries);

	Storage& storage()
	{
		return mStorage;
	}

	bool operator==(const Handle& other) const
	{
		return this == &other;
	}

	esp_err_t set_typed_item(ItemType datatype, const char* key, const void* data, size_t dataSize);

	esp_err_t get_typed_item(ItemType datatype, const char* key, void* data, size_t dataSize);

private:
	friend class Storage;

	/**
     * The underlying storage's object.
     */
	Storage& mStorage;

	/**
     * Numeric representation of the namespace as it is saved in flash (see README.rst for further details).
     */
	uint8_t mNsIndex;

	/**
     * Whether this handle is marked as read-only or read-write.
     */
	bool mReadOnly;

	/**
     * Indicates the validity of this handle.
     * Upon opening, a handle is valid. It becomes invalid if the underlying storage is de-initialized.
     */
	bool valid;
};

using HandlePtr = std::unique_ptr<Handle>;

// Template Implementations
template <typename T> esp_err_t Handle::set_item(const char* key, T value)
{
	return set_typed_item(itemTypeOf(value), key, &value, sizeof(value));
}

template <typename T> esp_err_t Handle::get_item(const char* key, T& value)
{
	return get_typed_item(itemTypeOf(value), key, &value, sizeof(value));
}

} // namespace nvs
