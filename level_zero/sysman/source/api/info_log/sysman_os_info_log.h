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

class OsInfoLog {
  public:
    virtual ~OsInfoLog() = default;

    virtual ze_result_t getProperties(zes_intel_info_log_properties_exp_t *pProperties) = 0;
    virtual ze_result_t infoLogRead(uint32_t *pSize, uint8_t *pBuffer) = 0;
    virtual ze_result_t infoLogEnable(bool state) = 0;
    static std::unique_ptr<OsInfoLog> create(zes_intel_info_log_format_exp_t format);
    static std::vector<zes_intel_info_log_format_exp_t> getSupportedInfoLogFormats();
};

} // namespace Sysman
} // namespace L0
