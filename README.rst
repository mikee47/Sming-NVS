Non-volatile storage library
============================

Introduction
------------

The Non-volatile storage (NVS) library is designed to store key-value pairs in flash memory.
This is a port of the Espressif Esp32 IDF `nvs_flash` library for Sming. It differs from the original in several ways:

-  Reworked to provide a consistent C++ interface
-  Methods return ``bool``, and error code for last operation can be obtained via ``nvs_errno``
-  IDF `nvs::Storage` has been renamed as `Container`; this handles data for a single :cpp:class:`Storage::Partition`.
-  Containers may not be closed or erased if there are open handles - calls will fail.
   (IDF allows this, but invalidates all open handles in the process.
   It consumes RAM by keeping a list of all open handles for this purpose.)
-  The C API is retained as it's required by ``esp_wifi``.
   'C' handles are created by casting the :cpp:class:`nvs::Handle` pointer to a `uint32_t`.
   (IDF consumes RAM using a separate list for this.)


Underlying storage
^^^^^^^^^^^^^^^^^^

The library performs all low-level reading, writing and erasing via the partition API:
it does not access flash memory directly.

Partitions must have a type/subtype of ``data`` / ``nvs``.

Storage block size is fixed at 4096 bytes, compatible with the ESP flash API.

Other types of storage device may be used. See the :component:`Storage` library for details.

.. note::

   If an NVS partition is truncated (by a change to the partition table), its contents should be erased.

.. note::

   NVS works best for storing many small values, rather than a few large values of the type 'string' and 'blob'.
   If you need to store large blobs or strings, consider using a regular file system.

   The library also uses RAM for caching entries. This can be minimised by keeping the partition size small.


Keys and values
^^^^^^^^^^^^^^^

NVS operates on key-value pairs. Keys are ASCII strings; the maximum key length is currently 15 characters. Values can have one of the following types:

-  integer types: ``uint8_t``, ``int8_t``, ``uint16_t``, ``int16_t``, ``uint32_t``, ``int32_t``, ``uint64_t``, ``int64_t``
-  zero-terminated string
-  variable length binary data (blob)

.. note::

    String values are currently limited to 4000 bytes. This includes the null terminator.
    Blob values are limited to 508000 bytes or (97.6% of the partition size - 4000) bytes, whichever is lower.

Keys are required to be unique. Assigning a new value to an existing key works as follows:

-  if the new value is of the same type as the old one, value is updated
-  if the new value has a different data type, an error is returned

Data type check is also performed when reading a value. Read operations will fail if the requested data type does not match that of the stored value.


Containers, Handles and Namespaces
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

A :cpp:class:`nvs::Container` is used to manage storage within a single partition.

A container stores key-value pairs from different sources, each identified by a unique ``namespace``.

Namespace names follow the same rules as key names, with a maximum length of 15 characters.

Containers are normally accessed using a :cpp:class:`nvs::Handle`, which provides read-only
or read/write access to a single namespace.

Handles are created using a call to :cpp:func:`nvs::Container::openHandle`.


Security, tampering, and robustness
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

NVS is not directly compatible with the ESP32 flash encryption system. However, data can still be stored in encrypted form if NVS encryption is used together with ESP32 flash encryption.
See :ref:`nvs_encryption` for details.

If NVS encryption is not used, it is possible for anyone with physical access to the flash chip to alter, erase, or add key-value pairs. With NVS encryption enabled, it is not possible to alter or add a key-value pair and get recognized as a valid pair without knowing corresponding NVS encryption keys. However, there is no tamper-resistance against the erase operation.

The library does try to recover from conditions when flash memory is in an inconsistent state. In particular, one should be able to power off the device at any point and time and then power it back on. This should not result in loss of data, except for the new key-value pair if it was being written at the moment of powering off. The library should also be able to initialize properly with any random data present in flash memory.


Internals
---------

See :doc:`internals`.


API
---

.. doxygennamespace:: nvs
   :members:
