/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "performance_imp.h"

#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/tools/source/sysman/sysman_imp.h"

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
    uint32_t subdeviceId = std::numeric_limits<uint32_t>::max();
    ze_bool_t onSubdevice = false;
    SysmanDeviceImp::getSysmanDeviceInfo(handle, subdeviceId, onSubdevice, false);
    pOsPerformance = OsPerformance::create(pOsSysman, onSubdevice,
                                           subdeviceId, domain);

    UNRECOVERABLE_IF(nullptr == pOsPerformance);
    init();
}

PerformanceImp::~PerformanceImp() = default;

} // namespace L0
