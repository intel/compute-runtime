/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zes_api.h>

#include <memory>
#include <vector>

namespace L0 {
namespace Sysman {

struct OsSysman;
class OsInfoLogInstance;

class OsInfoLog {
  public:
    virtual ~OsInfoLog() = default;

    virtual ze_result_t getProperties(zes_info_log_ext_properties_t *pProperties) = 0;
    virtual ze_result_t createInstance(const char *pInstanceName,
                                       zes_info_log_instance_ext_desc_t *pDesc,
                                       std::unique_ptr<OsInfoLogInstance> &pOsInfoLogInstance) = 0;
    static std::unique_ptr<OsInfoLog> create(zes_info_log_format_ext_t format);
    static std::vector<zes_info_log_format_ext_t> getSupportedInfoLogFormats();
};

} // namespace Sysman
} // namespace L0
