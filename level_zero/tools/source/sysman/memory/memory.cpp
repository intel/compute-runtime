/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/sysman/memory/memory_imp.h"
#include "level_zero/tools/source/sysman/os_sysman.h"

namespace L0 {

MemoryHandleContext::~MemoryHandleContext() {
    releaseMemoryHandles();
}

void MemoryHandleContext::releaseMemoryHandles() {
    handleList.clear();
}

void MemoryHandleContext::createHandle(ze_device_handle_t deviceHandle) {
    std::unique_ptr<Memory> pMemory = std::make_unique<MemoryImp>(pOsSysman, deviceHandle);
    if (pMemory->initSuccess == true) {
        handleList.push_back(std::move(pMemory));
    }
}

ze_result_t MemoryHandleContext::init(std::vector<ze_device_handle_t> &deviceHandles) {
    for (const auto &deviceHandle : deviceHandles) {
        createHandle(deviceHandle);
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t MemoryHandleContext::memoryGet(uint32_t *pCount, zes_mem_handle_t *phMemory) {
    std::call_once(initMemoryOnce, [this]() {
        this->init(pOsSysman->getDeviceHandles());
        this->memoryInitDone = true;
    });
    uint32_t handleListSize = static_cast<uint32_t>(handleList.size());
    uint32_t numToCopy = std::min(*pCount, handleListSize);
    if (0 == *pCount || *pCount > handleListSize) {
        *pCount = handleListSize;
    }
    if (nullptr != phMemory) {
        for (uint32_t i = 0; i < numToCopy; i++) {
            phMemory[i] = handleList[i]->toHandle();
        }
    }
    return ZE_RESULT_SUCCESS;
}

} // namespace L0
