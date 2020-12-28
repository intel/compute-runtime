/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/performance/os_performance.h"

namespace L0 {

class WddmPerformanceImp : public OsPerformance {
  public:
    ze_result_t osPerformanceGetProperties(zes_perf_properties_t &pProperties) override;
    ze_result_t osPerformanceGetConfig(double *pFactor) override;
    ze_result_t osPerformanceSetConfig(double pFactor) override;
    bool isPerformanceSupported(void) override;
};

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

OsPerformance *OsPerformance::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId,
                                     zes_engine_type_flag_t domain) {
    WddmPerformanceImp *pWddmPerformanceImp = new WddmPerformanceImp();
    return static_cast<OsPerformance *>(pWddmPerformanceImp);
}

} // namespace L0
