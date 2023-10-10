/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/memory/sysman_memory_imp.h"

namespace L0 {
namespace Sysman {

ze_result_t MemoryImp::memoryGetBandwidth(zes_mem_bandwidth_t *pBandwidth) {
    return pOsMemory->getBandwidth(pBandwidth);
}

ze_result_t MemoryImp::memoryGetState(zes_mem_state_t *pState) {
    return pOsMemory->getState(pState);
}

ze_result_t MemoryImp::memoryGetProperties(zes_mem_properties_t *pProperties) {
    *pProperties = memoryProperties;
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
