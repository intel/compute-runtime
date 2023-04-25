/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/performance/windows/os_performance_imp.h"

namespace L0 {
namespace Sysman {

ze_result_t WddmPerformanceImp::osPerformanceGetConfig(double *pFactor) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmPerformanceImp::osPerformanceSetConfig(double pFactor) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmPerformanceImp::osPerformanceGetProperties(zes_perf_properties_t &properties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

bool WddmPerformanceImp::isPerformanceSupported(void) {
    return false;
}

WddmPerformanceImp::WddmPerformanceImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId,
                                       zes_engine_type_flag_t domain) {}

OsPerformance *OsPerformance::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId,
                                     zes_engine_type_flag_t domain) {
    WddmPerformanceImp *pWddmPerformanceImp = new WddmPerformanceImp(pOsSysman, onSubdevice, subdeviceId, domain);
    return static_cast<OsPerformance *>(pWddmPerformanceImp);
}

} // namespace Sysman
} // namespace L0
