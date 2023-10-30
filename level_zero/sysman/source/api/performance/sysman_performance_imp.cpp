/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/performance/sysman_performance_imp.h"

#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/sysman/source/device/sysman_device_imp.h"

namespace L0 {
namespace Sysman {

ze_result_t PerformanceImp::performanceGetProperties(zes_perf_properties_t *pProperties) {
    *pProperties = performanceProperties;
    return ZE_RESULT_SUCCESS;
}

ze_result_t PerformanceImp::performanceGetConfig(double *pFactor) {
    return pOsPerformance->osPerformanceGetConfig(pFactor);
}

ze_result_t PerformanceImp::performanceSetConfig(double pFactor) {
    return pOsPerformance->osPerformanceSetConfig(pFactor);
}

void PerformanceImp::init() {
    this->isPerformanceEnabled = pOsPerformance->isPerformanceSupported();
    if (this->isPerformanceEnabled) {
        pOsPerformance->osPerformanceGetProperties(performanceProperties);
    }
}

PerformanceImp::PerformanceImp(OsSysman *pOsSysman, bool onSubdevice, uint32_t subDeviceId, zes_engine_type_flag_t domain) {
    pOsPerformance = OsPerformance::create(pOsSysman, onSubdevice,
                                           subDeviceId, domain);
    UNRECOVERABLE_IF(nullptr == pOsPerformance);
    init();
}

PerformanceImp::~PerformanceImp() {
    if (pOsPerformance != nullptr) {
        delete pOsPerformance;
        pOsPerformance = nullptr;
    }
}

} // namespace Sysman
} // namespace L0
