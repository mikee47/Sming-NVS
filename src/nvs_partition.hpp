// Copyright 2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ESP_PARTITION_HPP_
#define ESP_PARTITION_HPP_

#include <Partition.h>
#include "intrusive_list.h"

namespace nvs {

/**
 * Implementation of Partition for NVS.
 *
 * It is implemented as an intrusive_list_node to easily store instances of it. NVSStorage and NVSPage take pointer
 * references of this class to abstract their partition operations.
 */
using NVSPartition = intrusive_list_node<Partition>;

} // nvs

#endif // ESP_PARTITION_HPP_
