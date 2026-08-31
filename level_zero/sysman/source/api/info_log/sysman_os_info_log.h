/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zes_intel_gpu_sysman.h>

#include <memory>
#include <vector>

namespace L0 {
namespace Sysman {

struct OsSysman;
class OsInfoLogInstance;

class OsInfoLog {
  public:
    virtual ~OsInfoLog() = default;

    virtual ze_result_t getProperties(zes_intel_info_log_properties_exp_t *pProperties) = 0;
    virtual ze_result_t createInstance(const char *pInstanceName,
                                       zes_intel_info_log_instance_exp_desc_t *pDesc,
                                       std::unique_ptr<OsInfoLogInstance> &pOsInfoLogInstance) = 0;
    static std::unique_ptr<OsInfoLog> create(zes_intel_info_log_format_exp_t format);
    static std::vector<zes_intel_info_log_format_exp_t> getSupportedInfoLogFormats();
};

} // namespace Sysman
} // namespace L0
