/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/api/info_log/sysman_info_log.h"
#include "level_zero/sysman/source/api/info_log/sysman_info_log_instance_imp.h"
#include "level_zero/sysman/source/api/info_log/sysman_os_info_log.h"

#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace L0 {
namespace Sysman {

class InfoLogImp : public InfoLog {
  public:
    InfoLogImp(zes_info_log_format_ext_t format);
    ~InfoLogImp() override;

    ze_result_t infoLogGetProperties(zes_info_log_ext_properties_t *pProperties) override;
    ze_result_t infoLogCreateInstance(const char *pInstanceName,
                                      zes_info_log_instance_ext_desc_t *pDesc,
                                      zes_info_log_instance_handle_t *phInfoLogInstance) override;
    ze_result_t destroyInstance(InfoLogInstance *pInstance) override;
    void destroyAllInstances() override;

    void init();
    std::unique_ptr<OsInfoLog> pOsInfoLog;

  private:
    // Captured once at construction. Nothing changes these afterwards, so every query is served
    // from here, and a failed capture is reported to every caller.
    ze_result_t initResult = ZE_RESULT_ERROR_UNINITIALIZED;
    zes_info_log_ext_properties_t infoLogProperties = {};

    std::mutex instancesMutex;
    std::vector<std::unique_ptr<InfoLogInstance>> instances;
    std::set<std::string> activeInstanceNames;
};

} // namespace Sysman
} // namespace L0
