/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/performance/sysman_performance.h"

#include "level_zero/sysman/source/api/performance/sysman_performance_imp.h"
#include "level_zero/sysman/source/device/os_sysman.h"

#include <algorithm>

namespace L0 {
namespace Sysman {

PerformanceHandleContext::~PerformanceHandleContext() {
    for (auto &pPerformance : handleList) {
        if (pPerformance) {
            delete pPerformance;
            pPerformance = nullptr;
        }
    }
}

void PerformanceHandleContext::createHandle(bool onSubdevice, uint32_t subDeviceId, zes_engine_type_flag_t domain) {
    Performance *pPerformance = new PerformanceImp(pOsSysman, onSubdevice, subDeviceId, domain);
    if (pPerformance->isPerformanceEnabled == true) {
        handleList.push_back(pPerformance);
    } else {
        delete pPerformance;
    }
}

ze_result_t PerformanceHandleContext::init(uint32_t subDeviceCount) {

    if (subDeviceCount > 0) {
        for (uint32_t subDeviceId = 0; subDeviceId < subDeviceCount; subDeviceId++) {
            createHandle(true, subDeviceId, ZES_ENGINE_TYPE_FLAG_MEDIA);
            createHandle(true, subDeviceId, ZES_ENGINE_TYPE_FLAG_COMPUTE);
        }

    } else {
        createHandle(false, 0, ZES_ENGINE_TYPE_FLAG_MEDIA);
        createHandle(false, 0, ZES_ENGINE_TYPE_FLAG_COMPUTE);
    }

    createHandle(false, 0, ZES_ENGINE_TYPE_FLAG_OTHER);
    return ZE_RESULT_SUCCESS;
}

ze_result_t PerformanceHandleContext::performanceGet(uint32_t *pCount, zes_perf_handle_t *phPerformance) {
    std::call_once(initPerformanceOnce, [this]() {
        this->init(pOsSysman->getSubDeviceCount());
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

} // namespace Sysman
} // namespace L0
