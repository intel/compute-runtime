/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/memory/memory_imp.h"

namespace L0 {

ze_result_t MemoryImp::memoryGetBandwidth(zes_mem_bandwidth_t *pBandwidth) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t MemoryImp::memoryGetState(zes_mem_state_t *pState) {
    ze_result_t result = pOsMemory->getMemorySize(pState->size, pState->size);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    return pOsMemory->getMemHealth(pState->health);
}

ze_result_t MemoryImp::memoryGetProperties(zes_mem_properties_t *pProperties) {
    *pProperties = memoryProperties;
    return ZE_RESULT_SUCCESS;
}

void MemoryImp::init() {
    memoryProperties.type = ZES_MEM_TYPE_DDR;
    memoryProperties.onSubdevice = false;
    memoryProperties.subdeviceId = 0;
    memoryProperties.physicalSize = 0;
}

MemoryImp::MemoryImp(OsSysman *pOsSysman, ze_device_handle_t hDevice) {
    pOsMemory = OsMemory::create(pOsSysman);
    hCoreDevice = hDevice;

    init();
}

MemoryImp::~MemoryImp() {
    if (nullptr != pOsMemory) {
        delete pOsMemory;
    }
}

} // namespace L0
