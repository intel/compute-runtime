/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/api/info_log/sysman_os_info_log.h"
#include "level_zero/sysman/source/api/info_log/sysman_os_info_log_instance.h"

#include <string>
#include <string_view>

struct tracefs_instance; // NOLINT(readability-identifier-naming)

namespace L0 {
namespace Sysman {

class TraceFsApi;
class LinuxInfoLogImp : public OsInfoLog {
  public:
    static std::unique_ptr<TraceFsApi> (*createTraceFsApi)();

    LinuxInfoLogImp(zes_info_log_format_ext_t format);
    ~LinuxInfoLogImp() override;

    ze_result_t getProperties(zes_info_log_ext_properties_t *pProperties) override;
    ze_result_t createInstance(const char *pInstanceName,
                               zes_info_log_instance_ext_desc_t *pDesc,
                               std::unique_ptr<OsInfoLogInstance> &pOsInfoLogInstance) override;

  private:
    bool isNamedInstancedCollectionAvailable();
    bool isPeekAvailable();
    std::string getTracefsInstancesDirPath();
    ze_result_t claimInstanceOwnership(std::string_view instanceName, int &ownershipFd);
    bool checkEventEnabled(struct tracefs_instance *instance);
    bool checkTracingOn(struct tracefs_instance *instance);

  protected:
    zes_info_log_format_ext_t infoLogFormat = ZES_INFO_LOG_FORMAT_EXT_CPER;
    std::unique_ptr<TraceFsApi> pTraceFsApi;
};

} // namespace Sysman
} // namespace L0
