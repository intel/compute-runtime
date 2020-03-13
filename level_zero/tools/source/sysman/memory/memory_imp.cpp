/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/memory/memory_imp.h"

#include "level_zero/core/source/device/device.h"

namespace L0 {

ze_result_t MemoryImp::memoryGetBandwidth(zet_mem_bandwidth_t *pBandwidth) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t MemoryImp::memoryGetState(zet_mem_state_t *pState) {

    ze_result_t result;

    result = pOsMemory->getAllocSize(pState->allocatedSize);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    result = pOsMemory->getMaxSize(pState->maxSize);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    result = pOsMemory->getMemHealth(pState->health);

    return result;
}

ze_result_t MemoryImp::memoryGetProperties(zet_mem_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
MemoryImp::MemoryImp(OsSysman *pOsSysman, ze_device_handle_t hDevice) {
    pOsMemory = OsMemory::create(pOsSysman);
    hCoreDevice = hDevice;
}

MemoryImp::~MemoryImp() {
    if (nullptr != pOsMemory) {
        delete pOsMemory;
    }
}

} // namespace L0
