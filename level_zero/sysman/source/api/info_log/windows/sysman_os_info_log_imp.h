/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/api/info_log/sysman_os_info_log.h"
#include "level_zero/sysman/source/api/info_log/sysman_os_info_log_instance.h"

namespace L0 {
namespace Sysman {

class WddmInfoLogImp : public OsInfoLog {
  public:
    WddmInfoLogImp();
    ~WddmInfoLogImp() override = default;

    ze_result_t getProperties(zes_intel_info_log_properties_exp_t *pProperties) override;
    ze_result_t createInstance(const char *pInstanceName,
                               zes_intel_info_log_instance_exp_desc_t *pDesc,
                               std::unique_ptr<OsInfoLogInstance> &pOsInfoLogInstance) override;
};

} // namespace Sysman
} // namespace L0
