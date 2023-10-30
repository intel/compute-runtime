/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "level_zero/sysman/source/api/memory/sysman_memory_imp.h"
#include "level_zero/sysman/source/device/os_sysman.h"

namespace L0 {
namespace Sysman {

MemoryHandleContext::~MemoryHandleContext() {
    releaseMemoryHandles();
}

void MemoryHandleContext::releaseMemoryHandles() {
    handleList.clear();
}

void MemoryHandleContext::createHandle(bool onSubdevice, uint32_t subDeviceId) {
    std::unique_ptr<Memory> pMemory = std::make_unique<MemoryImp>(pOsSysman, onSubdevice, subDeviceId);
    if (pMemory->initSuccess == true) {
        handleList.push_back(std::move(pMemory));
    }
}

ze_result_t MemoryHandleContext::init(uint32_t subDeviceCount) {

    if (subDeviceCount > 0) {
        for (uint32_t subDeviceId = 0; subDeviceId < subDeviceCount; subDeviceId++) {
            createHandle(true, subDeviceId);
        }
    } else {
        createHandle(false, 0);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t MemoryHandleContext::memoryGet(uint32_t *pCount, zes_mem_handle_t *phMemory) {
    std::call_once(initMemoryOnce, [this]() {
        this->init(pOsSysman->getSubDeviceCount());
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

} // namespace Sysman
} // namespace L0
