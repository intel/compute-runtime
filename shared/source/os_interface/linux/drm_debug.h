/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"

#include <string>
#include <utility>

namespace NEO {

/*

UUIDs: Deterministic generation

$ python -q # Version 3.x
>>> import uuid
>>>
>>> I915_UUID_NAMESPACE = uuid.UUID(bytes = b'i915' * 4);
>>> I915_UUID_NAMESPACE
UUID('69393135-6939-3135-6939-313569393135')
>>>
>>>
>>> I915_UUID = lambda x: uuid.uuid5(I915_UUID_NAMESPACE, x)
>>> I915_UUID('I915_UUID_CLASS_CUSTOM')
UUID('74644f12-6a2c-59e6-ac92-ea7f2ef530eb')
>>>
>>>
>>> L0_UUID_NAMESPACE = uuid.UUID(bytes = b'L0' * 8);
>>> L0_UUID_NAMESPACE
UUID('4c304c30-4c30-4c30-4c30-4c304c304c30')
>>>
>>>
>>> L0_UUID = lambda x: uuid.uuid5(L0_UUID_NAMESPACE, x)
>>> L0_UUID('L0_ZEBIN_MODULE')
UUID('88d347c1-c79b-530a-b68f-e0db7d575e04')
>>>
>>>
>>> L0_UUID('L0_COMMAND_QUEUE')
UUID('285208b2-c5e0-5fcb-90bb-7576ed7a9697')

*/

using ClassNamesArray = std::array<std::pair<const char *, const std::string>, size_t(Drm::ResourceClass::MaxSize)>;
const ClassNamesArray classNamesToUuid = {std::make_pair("I915_UUID_CLASS_ELF_BINARY", "31203221-8069-5a0a-9d43-94a4d3395ee1"),
                                          std::make_pair("I915_UUID_CLASS_ISA_BYTECODE", "53baed0a-12c3-5d19-aa69-ab9c51aa1039"),
                                          std::make_pair("I915_UUID_L0_MODULE_AREA", "a411e82e-16c9-58b7-bfb5-b209b8601d5f"),
                                          std::make_pair("I915_UUID_L0_SIP_AREA", "21fd6baf-f918-53cc-ba74-f09aaaea2dc0"),
                                          std::make_pair("I915_UUID_L0_SBA_AREA", "ec45189d-97d3-58e2-80d1-ab52c72fdcc1"),
                                          std::make_pair("L0_ZEBIN_MODULE", "88d347c1-c79b-530a-b68f-e0db7d575e04")};

constexpr auto uuidL0CommandQueueName = "L0_COMMAND_QUEUE";
constexpr auto uuidL0CommandQueueHash = "285208b2-c5e0-5fcb-90bb-7576ed7a9697"; // L0_UUID('L0_COMMAND_QUEUE')

struct DrmUuid {
    static bool getClassUuidIndex(std::string uuid, uint32_t &index) {
        for (uint32_t i = 0; i < uint32_t(Drm::ResourceClass::MaxSize); i++) {
            if (uuid == classNamesToUuid[i].second) {
                index = i;
                return true;
            }
        }
        return false;
    }
};
} // namespace NEO
