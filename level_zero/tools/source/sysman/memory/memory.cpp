/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/memory/memory.h"

#include "level_zero/tools/source/sysman/memory/memory_imp.h"

namespace L0 {

MemoryHandleContext::~MemoryHandleContext() {
    for (Memory *pMemory : handleList) {
        delete pMemory;
    }
}

ze_result_t MemoryHandleContext::init() {
    Memory *pMemory = new MemoryImp(pOsSysman, hCoreDevice);
    handleList.push_back(pMemory);
    return ZE_RESULT_SUCCESS;
}

ze_result_t MemoryHandleContext::memoryGet(uint32_t *pCount, zet_sysman_mem_handle_t *phMemory) {
    *pCount = 0;
    return ZE_RESULT_SUCCESS;
}

} // namespace L0
