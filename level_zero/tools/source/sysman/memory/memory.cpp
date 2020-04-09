/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_manager.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/sysman/memory/memory_imp.h"

namespace L0 {

MemoryHandleContext::~MemoryHandleContext() {
    for (Memory *pMemory : handleList) {
        delete pMemory;
    }
}

ze_result_t MemoryHandleContext::init() {
    Device *device = L0::Device::fromHandle(hCoreDevice);

    isLmemSupported = device->getDriverHandle()->getMemoryManager()->isLocalMemorySupported(device->getRootDeviceIndex());

    if (isLmemSupported) {
        Memory *pMemory = new MemoryImp(pOsSysman, hCoreDevice);
        handleList.push_back(pMemory);
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t MemoryHandleContext::memoryGet(uint32_t *pCount, zet_sysman_mem_handle_t *phMemory) {
    if (nullptr == phMemory) {
        *pCount = static_cast<uint32_t>(handleList.size());
        return ZE_RESULT_SUCCESS;
    }
    uint32_t i = 0;
    for (Memory *mem : handleList) {
        if (i >= *pCount) {
            break;
        }
        phMemory[i++] = mem->toHandle();
    }
    *pCount = i;
    return ZE_RESULT_SUCCESS;
}

} // namespace L0
