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
    static bool (*isCperEventSupported)();

    LinuxInfoLogImp(zes_info_log_format_ext_t format);
    ~LinuxInfoLogImp() override;

    ze_result_t getProperties(zes_info_log_ext_properties_t *pProperties) override;
    ze_result_t createInstance(const char *pInstanceName,
                               zes_info_log_instance_ext_desc_t *pDesc,
                               std::unique_ptr<OsInfoLogInstance> &pOsInfoLogInstance) override;
    ze_result_t getFieldInfo(uint32_t *pCount, zes_intel_info_log_decodable_field_info_exp_t *pFieldInfos) override;
    ze_result_t validateRecords(uint32_t size, uint32_t recordCount,
                                const zes_info_log_metadata_ext_t *pDescriptors) override;
    ze_result_t readField(uint32_t size, const uint8_t *pBuffer, const zes_info_log_metadata_ext_t *pDescriptors,
                          uint32_t fieldId, uint32_t recordId,
                          void *pOutBuffer, size_t bufferLen, size_t *pBytesCopied) override;

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
