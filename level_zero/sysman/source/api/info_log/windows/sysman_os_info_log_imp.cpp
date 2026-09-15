/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/info_log/windows/sysman_os_info_log_imp.h"

namespace L0 {
namespace Sysman {

WddmInfoLogImp::WddmInfoLogImp() {}

std::unique_ptr<OsInfoLog> OsInfoLog::create(zes_intel_info_log_format_exp_t format) {
    return std::make_unique<WddmInfoLogImp>();
}

ze_result_t WddmInfoLogImp::getProperties(zes_intel_info_log_properties_exp_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmInfoLogImp::createInstance(const char *pInstanceName,
                                           zes_intel_info_log_instance_exp_desc_t *pDesc,
                                           std::unique_ptr<OsInfoLogInstance> &pOsInfoLogInstance) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

std::vector<zes_intel_info_log_format_exp_t> OsInfoLog::getSupportedInfoLogFormats() {
    return {};
}

} // namespace Sysman
} // namespace L0
