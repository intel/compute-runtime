/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/info_log/linux/sysman_os_info_log_imp.h"

#include "level_zero/sysman/source/shared/linux/tracefs_api/sysman_tracefs_api.h"

namespace L0 {
namespace Sysman {

LinuxInfoLogImp::LinuxInfoLogImp(zes_intel_info_log_format_exp_t format)
    : infoLogFormat(format), pTraceFsApi(createTraceFsApi()) {}

std::unique_ptr<OsInfoLog> OsInfoLog::create(zes_intel_info_log_format_exp_t format) {
    return std::make_unique<LinuxInfoLogImp>(format);
}

} // namespace Sysman
} // namespace L0
