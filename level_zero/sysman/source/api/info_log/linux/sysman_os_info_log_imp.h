/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/api/info_log/sysman_os_info_log.h"

namespace L0 {
namespace Sysman {

class LinuxInfoLogImp : public OsInfoLog {
  public:
    LinuxInfoLogImp();
    ~LinuxInfoLogImp() override = default;

    ze_result_t getProperties(zes_intel_info_log_properties_exp_t *pProperties) override;
    ze_result_t infoLogRead(uint32_t *pSize, uint8_t *pBuffer) override;
    ze_result_t infoLogEnable(bool state) override;
};

} // namespace Sysman
} // namespace L0
