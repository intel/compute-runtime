/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/memory/sysman_memory_imp.h"

#include "shared/source/helpers/string.h"

#include <map>
#include <string>

namespace L0 {
namespace Sysman {

static const std::map<uint32_t, std::string> memoryVendorIdToNameMap = {
    {0xFFu, "Micron"},
};

ze_result_t MemoryImp::memoryGetBandwidth(zes_mem_bandwidth_t *pBandwidth) {
    return pOsMemory->getBandwidth(pBandwidth);
}

ze_result_t MemoryImp::memoryGetState(zes_mem_state_t *pState) {
    return pOsMemory->getState(pState);
}

ze_result_t MemoryImp::memoryGetProperties(zes_mem_properties_t *pProperties) {
    void *pNext = pProperties->pNext;
    *pProperties = memoryProperties;
    pProperties->pNext = pNext;

    while (pNext) {
        auto pExtProps = reinterpret_cast<zes_base_properties_t *>(pNext);
        if (pExtProps->stype == ZES_STRUCTURE_TYPE_MEMORY_VENDOR_INFO_EXT_PROPERTIES) {
            auto pVendorIdProps = reinterpret_cast<zes_memory_vendor_info_ext_properties_t *>(pExtProps);
            // A vendor ID of 0 indicates that the memory vendor ID could not be determined
            if (pOsMemory->getVendorId(&pVendorIdProps->vendorId) != ZE_RESULT_SUCCESS) {
                pVendorIdProps->vendorId = 0;
            }
            // A length of 0 indicates that the memory vendor name could not be determined.
            pVendorIdProps->length = 0;
            pVendorIdProps->vendorName[0] = '\0';
            auto vendorNameIterator = memoryVendorIdToNameMap.find(pVendorIdProps->vendorId);
            if (vendorNameIterator != memoryVendorIdToNameMap.end()) {
                const std::string &vendorName = vendorNameIterator->second;
                strncpy_s(pVendorIdProps->vendorName, ZES_MEMORY_VENDOR_NAME_EXT_SIZE, vendorName.c_str(), vendorName.size());
                pVendorIdProps->length = static_cast<uint16_t>(vendorName.length());
            }
        }
        pNext = pExtProps->pNext;
    }

    return ZE_RESULT_SUCCESS;
}

void MemoryImp::init() {
    this->initSuccess = pOsMemory->isMemoryModuleSupported();
    if (this->initSuccess == true) {
        pOsMemory->getProperties(&memoryProperties);
    }
}

MemoryImp::MemoryImp(OsSysman *pOsSysman, bool onSubdevice, uint32_t subDeviceId) {
    pOsMemory = OsMemory::create(pOsSysman, onSubdevice, subDeviceId);
    init();
}

MemoryImp::~MemoryImp() = default;

} // namespace Sysman
} // namespace L0
