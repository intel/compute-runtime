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

struct tracefs_instance; // NOLINT(readability-identifier-naming)

namespace L0 {
namespace Sysman {

class TraceFsApi;
class LinuxInfoLogImp : public OsInfoLog {
  public:
    static std::unique_ptr<TraceFsApi> (*createTraceFsApi)();

    LinuxInfoLogImp(zes_intel_info_log_format_exp_t format);
    ~LinuxInfoLogImp() override;

    ze_result_t getProperties(zes_intel_info_log_properties_exp_t *pProperties) override;
    ze_result_t createInstance(const char *pInstanceName,
                               zes_intel_info_log_instance_exp_desc_t *pDesc,
                               std::unique_ptr<OsInfoLogInstance> &pOsInfoLogInstance) override;

  private:
    bool isNamedInstancedCollectionAvailable();
    bool isPeekAvailable();
    std::string getTracefsInstancesDirPath();
    bool checkInstancePreExists(const char *instanceName);
    ze_result_t claimInstanceOwnership(const char *instanceName, int &ownershipFd);
    bool checkEventEnabled(struct tracefs_instance *instance);
    bool checkTracingOn(struct tracefs_instance *instance);

  protected:
    zes_intel_info_log_format_exp_t infoLogFormat;
    std::unique_ptr<TraceFsApi> pTraceFsApi;
};

} // namespace Sysman
} // namespace L0
