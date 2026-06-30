/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/info_log/linux/sysman_os_info_log_imp.h"

namespace L0 {
namespace Sysman {

LinuxInfoLogImp::LinuxInfoLogImp() {}

std::unique_ptr<OsInfoLog> OsInfoLog::create() {
    return std::make_unique<LinuxInfoLogImp>();
}

ze_result_t LinuxInfoLogImp::getProperties(zes_intel_info_log_properties_exp_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxInfoLogImp::infoLogRead(uint32_t *pSize, uint8_t *pBuffer) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxInfoLogImp::infoLogEnable(bool state) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace Sysman
} // namespace L0
