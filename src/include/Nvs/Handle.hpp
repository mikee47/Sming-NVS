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
#include "nvs.h"
#include "Item.hpp"
#include "Container.hpp"

class ItemIterator;

namespace nvs
{
class Container;

#define CHECK_WRITE()                                                                                                  \
	if(mReadOnly) {                                                                                                    \
		mLastError = ESP_ERR_NVS_READ_ONLY;                                                                            \
		return false;                                                                                                  \
	}

/**
 * @brief A handle allowing nvs-entry related operations on the NVS.
 *
 * @note The scope of this handle is the namespace of a particular partition. Outside that scope, nvs entries can't be accessed/altered.
 */
class Handle
{
public:
	Handle(Container& container, uint8_t nsIndex, bool readOnly)
		: mContainer(container), mNsIndex(nsIndex), mReadOnly(readOnly)
	{
	}

	~Handle()
	{
		mContainer.handle_destroyed();
	}

	/**
     * @name Set value for simple integral or enum type key
     * @{
     *
     * @brief Method template to determine storage type based on provided value
     *
     * @param[in]  key     Key name. Maximal length is (NVS_KEY_NAME_MAX_SIZE-1) characters. Shouldn't be empty.
     * @param[in]  value   The value to set. Allowed types are the ones declared in ItemType as well as enums.
     *                     Note that enums lose their type information when stored in NVS. Ensure that the correct
     *                     enum type is used during retrieval with \ref getItem!
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
	template <typename T>
	typename std::enable_if<std::is_integral<T>::value || std::is_enum<T>::value, bool>::type setItem(const String& key,
																									  T value)
	{
		return setItem(itemTypeOf(value), key, value);
	}

	template <typename T>
	typename std::enable_if<std::is_integral<T>::value || std::is_enum<T>::value, bool>::type
	setItem(ItemType datatype, const String& key, T value)
	{
		return setItem(datatype, key, &value, sizeof(value));
	}

	/**
	 * @brief Sets value given specific storage type
	 */
	bool setItem(ItemType datatype, const String& key, const void* data, size_t dataSize)
	{
		CHECK_WRITE()
		return check(mContainer.writeItem(mNsIndex, datatype, key, data, dataSize));
	}
	/** @} */

	/**
	 * @name Set value for a String key type
	 * @{
	 *
	 * Fails if key exists and is of a different type.
     * Maximum string length (including null character) is 4000 bytes.
	 */
	bool setString(const String& key, const char* value)
	{
		CHECK_WRITE()
		return check(mContainer.writeItem(mNsIndex, nvs::ItemType::SZ, key, value, value ? strlen(value) + 1 : 0));
	}

	bool setString(const String& key, const String& value)
	{
		CHECK_WRITE()
		return check(mContainer.writeItem(mNsIndex, nvs::ItemType::SZ, key, value.c_str(), value.length() + 1));
	}
	/** @} */

	/**
     * @name brief      get value for given key
     *
     * Fails if key does not exist or the requested variable type doesn't match the stored type.
     * On error, `value` remains unchanged.
     *
     * @param[in]     key        Key name. Maximal length is (NVS_KEY_NAME_MAX_SIZE-1) characters. Shouldn't be empty.
     * @param         value      The output value. All integral types which are declared in ItemType as well as enums
     *                           are allowed. Note however that enums lost their type information when stored in NVS.
     *                           Ensure that the correct enum type is used during retrieval with \ref getItem!
     *
     * @return
     *             - ESP_OK if the value was retrieved successfully
     *             - ESP_ERR_NVS_NOT_FOUND if the requested key doesn't exist
     *             - ESP_ERR_NVS_INVALID_NAME if key name doesn't satisfy constraints
     *             - ESP_ERR_NVS_INVALID_LENGTH if length is not sufficient to store data
     */
	template <typename T>
	typename std::enable_if<std::is_integral<T>::value || std::is_enum<T>::value, bool>::type getItem(const String& key,
																									  T& value)
	{
		return getItem(itemTypeOf(value), key, &value, sizeof(value));
	}

	bool getItem(ItemType datatype, const String& key, void* data, size_t dataSize)
	{
		return check(mContainer.readItem(mNsIndex, datatype, key, data, dataSize));
	}
	/** @} */

	/**
     * @name Set variable-length binary values (Binary Large OBject)
     * @{
     *
     * @brief       set variable length binary value for given key
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
	bool setBlob(const String& key, const void* blob, size_t len)
	{
		CHECK_WRITE()
		return check(mContainer.writeItem(mNsIndex, nvs::ItemType::BLOB, key, blob, len));
	}

	bool setBlob(const String& key, const String& blob)
	{
		return setBlob(key, blob.c_str(), blob.length());
	}
	/** @} */

	/**
     * @name Read variable-length value
     *
     * Fails if key does not exist or the requested variable type doesn't match the stored type.
     *
     * Use getString() for reading NUL-terminated strings, and getBlob for arbitrary data structures.
     *
     * @{
     *
     * @brief Read value into user-provided buffer
     *
     * @param key        Key name. Maximal length is (NVS_KEY_NAME_MAX_SIZE-1) characters. Shouldn't be empty.
     * @param outValue   Buffer to store value. Must have sufficient room for NUL terminator.
     *                   On error, remains unchanged.
     * @param length     Size of buffer.
     *
     * @return
     *             - ESP_OK if the value was retrieved successfully
     *             - ESP_ERR_NVS_NOT_FOUND if the requested key doesn't exist
     *             - ESP_ERR_NVS_INVALID_NAME if key name doesn't satisfy constraints
     *             - ESP_ERR_NVS_INVALID_LENGTH if length is not sufficient to store data
     */
	bool getString(const String& key, char* outValue, size_t len)
	{
		return check(mContainer.readItem(mNsIndex, nvs::ItemType::SZ, key, outValue, len));
	}

	/**
	 * @brief Read value into String object
	 * @param key
	 * @retval String On error, will be invalid, i.e. bool(String) evaluates to false.
	 */
	String getString(const String& key)
	{
		String s = mContainer.readItem(mNsIndex, nvs::ItemType::SZ, key);
		mLastError = mContainer.lastError();
		return s;
	}

	/**
	 * @brief Read BLOB value into user-provided buffer
	 * @param key
	 * @param outValue Buffer to store value
	 * @param len Size of buffer
	 * @retval bool
	 */
	bool getBlob(const String& key, void* outValue, size_t len)
	{
		return check(mContainer.readItem(mNsIndex, nvs::ItemType::BLOB, key, outValue, len));
	}

	/**
	 * @brief Read BLOB value into String object
	 * @param key
	 * @retval String
	 */
	String getBlob(const String& key)
	{
		String s = mContainer.readItem(mNsIndex, nvs::ItemType::BLOB, key);
		mLastError = mContainer.lastError();
		return s;
	}
	/** @} */

	/**
     * @brief Look up the size of an entry's data
     * @param datatype Must match the type of stored data
     * @param key
     * @param dataSize For strings, includes the NUL terminator
     * @retval bool
     */
	bool getItemDataSize(ItemType datatype, const String& key, size_t& dataSize)
	{
		return check(mContainer.getItemDataSize(mNsIndex, datatype, key, dataSize));
	}

	/**
     * @brief Erases an entry
     */
	bool eraseItem(const String& key)
	{
		CHECK_WRITE()
		return check(mContainer.eraseItem(mNsIndex, key));
	}

	/**
     * @brief Erases all entries in the current namespace
     */
	bool eraseAll()
	{
		CHECK_WRITE()
		return check(mContainer.eraseNamespace(mNsIndex));
	}

	/**
     * @brief Commits all changes done through this handle so far
     */
	bool commit()
	{
		mLastError = ESP_OK;
		return true;
	}

	/**
     * @brief      Determine number of used entries in the current namespace
     * @param      usedEntries
     * @return     - ESP_OK if the changes have been written successfully.
     *               Return param used_entries will be filled valid value.
     *             - ESP_ERR_NVS_NOT_INITIALIZED if the storage driver is not initialized.
     *               Return param used_entries will be filled 0.
     *             - ESP_ERR_INVALID_ARG if nvs_stats equal to NULL.
     *             - Other error codes from the underlying storage driver.
     *               Return param used_entries will be filled 0.
     */
	bool getUsedEntryCount(size_t& usedEntries)
	{
		usedEntries = 0;
		return check(mContainer.calcEntriesInNamespace(mNsIndex, usedEntries));
	}

	/**
	 * @brief Get reference to storage container
	 *
	 * Can be used to perform operations on the entire storage container, not just
	 * in the current namespace.
	 */
	Container& container()
	{
		return mContainer;
	}

	bool operator==(const Handle& other) const
	{
		return this == &other;
	}

	esp_err_t lastError() const
	{
		return mLastError;
	}

private:
	friend class Container;

	bool check(bool res)
	{
		mLastError = res ? ESP_OK : mContainer.lastError();
		return res;
	}

	Container& mContainer;
	uint8_t mNsIndex; ///< Numeric representation of the namespace as it is saved in flash
	bool mReadOnly;   ///< Whether this handle is marked as read-only or read-write

	esp_err_t mLastError{ESP_OK};
};

using HandlePtr = std::unique_ptr<Handle>;

} // namespace nvs
