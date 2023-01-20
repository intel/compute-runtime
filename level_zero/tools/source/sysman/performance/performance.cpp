/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "performance.h"

#include "level_zero/tools/source/sysman/os_sysman.h"

#include "performance_imp.h"

namespace L0 {

PerformanceHandleContext::~PerformanceHandleContext() {
    for (auto &pPerformance : handleList) {
        if (pPerformance) {
            delete pPerformance;
            pPerformance = nullptr;
        }
        handleList.pop_back();
    }
}

void PerformanceHandleContext::createHandle(ze_device_handle_t deviceHandle, zes_engine_type_flag_t domain) {
    Performance *pPerformance = new PerformanceImp(pOsSysman, deviceHandle, domain);
    if (pPerformance->isPerformanceEnabled == true) {
        handleList.push_back(pPerformance);
    } else {
        delete pPerformance;
    }
}

ze_result_t PerformanceHandleContext::init(std::vector<ze_device_handle_t> &deviceHandles, ze_device_handle_t coreDevice) {
    for (const auto &deviceHandle : deviceHandles) {
        createHandle(deviceHandle, ZES_ENGINE_TYPE_FLAG_MEDIA);
        createHandle(deviceHandle, ZES_ENGINE_TYPE_FLAG_COMPUTE);
    }
    createHandle(coreDevice, ZES_ENGINE_TYPE_FLAG_OTHER);
    return ZE_RESULT_SUCCESS;
}

ze_result_t PerformanceHandleContext::performanceGet(uint32_t *pCount, zes_perf_handle_t *phPerformance) {
    std::call_once(initPerformanceOnce, [this]() {
        this->init(pOsSysman->getDeviceHandles(), pOsSysman->getCoreDeviceHandle());
    });
    uint32_t handleListSize = static_cast<uint32_t>(handleList.size());
    uint32_t numToCopy = std::min(*pCount, handleListSize);
    if (0 == *pCount || *pCount > handleListSize) {
        *pCount = handleListSize;
    }
    if (nullptr != phPerformance) {
        for (uint32_t i = 0; i < numToCopy; i++) {
            phPerformance[i] = handleList[i]->toPerformanceHandle();
        }
    }
    return ZE_RESULT_SUCCESS;
}
} // namespace L0
