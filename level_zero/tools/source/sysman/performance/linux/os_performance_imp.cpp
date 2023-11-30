/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/performance/linux/os_performance_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

namespace L0 {

ze_result_t LinuxPerformanceImp::osPerformanceGetProperties(zes_perf_properties_t &pProperties) {
    NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s() returning UNSUPPORTED_FEATURE \n", __FUNCTION__);
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxPerformanceImp::osPerformanceGetConfig(double *pFactor) {
    NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s() returning UNSUPPORTED_FEATURE \n", __FUNCTION__);
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxPerformanceImp::osPerformanceSetConfig(double pFactor) {
    NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s() returning UNSUPPORTED_FEATURE \n", __FUNCTION__);
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

bool LinuxPerformanceImp::isPerformanceSupported(void) {
    NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s() returning UNSUPPORTED_FEATURE \n", __FUNCTION__);
    return false;
}

std::unique_ptr<OsPerformance> OsPerformance::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_engine_type_flag_t domain) {
    std::unique_ptr<LinuxPerformanceImp> pLinuxPerformanceImp = std::make_unique<LinuxPerformanceImp>(pOsSysman, onSubdevice, subdeviceId, domain);
    return pLinuxPerformanceImp;
}

} // namespace L0
