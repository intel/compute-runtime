/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/performance/linux/sysman_os_performance_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

namespace L0 {
namespace Sysman {

ze_result_t LinuxPerformanceImp::osPerformanceGetProperties(zes_perf_properties_t &pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxPerformanceImp::osPerformanceGetConfig(double *pFactor) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxPerformanceImp::osPerformanceSetConfig(double pFactor) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

bool LinuxPerformanceImp::isPerformanceSupported(void) {
    return false;
}

OsPerformance *OsPerformance::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_engine_type_flag_t domain) {
    LinuxPerformanceImp *pLinuxPerformanceImp = new LinuxPerformanceImp(pOsSysman, onSubdevice, subdeviceId, domain);
    return static_cast<OsPerformance *>(pLinuxPerformanceImp);
}

} // namespace Sysman
} // namespace L0
