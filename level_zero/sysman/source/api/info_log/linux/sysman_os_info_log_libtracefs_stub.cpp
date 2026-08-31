/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/info_log/linux/sysman_os_info_log_imp.h"

namespace L0 {
namespace Sysman {

class TraceFsApi {};

std::unique_ptr<TraceFsApi> (*LinuxInfoLogImp::createTraceFsApi)() = []() -> std::unique_ptr<TraceFsApi> { return nullptr; };

LinuxInfoLogImp::~LinuxInfoLogImp() = default;

std::vector<zes_intel_info_log_format_exp_t> OsInfoLog::getSupportedInfoLogFormats() {
    return {};
}

ze_result_t LinuxInfoLogImp::getProperties(zes_intel_info_log_properties_exp_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxInfoLogImp::createInstance(const char *pInstanceName,
                                            zes_intel_info_log_instance_exp_desc_t *pDesc,
                                            std::unique_ptr<OsInfoLogInstance> &pOsInfoLogInstance) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace Sysman
} // namespace L0
