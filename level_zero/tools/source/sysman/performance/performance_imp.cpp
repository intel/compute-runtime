/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "performance_imp.h"

#include "shared/source/helpers/debug_helpers.h"

namespace L0 {

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

PerformanceImp::PerformanceImp(OsSysman *pOsSysman, ze_device_handle_t handle, zes_engine_type_flag_t domain) {
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    Device::fromHandle(handle)->getProperties(&deviceProperties);
    pOsPerformance = OsPerformance::create(pOsSysman, deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE,
                                           deviceProperties.subdeviceId, domain);
    UNRECOVERABLE_IF(nullptr == pOsPerformance);
    init();
}

PerformanceImp::~PerformanceImp() {
    if (pOsPerformance != nullptr) {
        delete pOsPerformance;
        pOsPerformance = nullptr;
    }
}

} // namespace L0
