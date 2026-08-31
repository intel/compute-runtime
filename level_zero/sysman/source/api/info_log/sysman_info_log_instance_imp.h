/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/api/info_log/sysman_info_log.h"
#include "level_zero/sysman/source/api/info_log/sysman_info_log_instance.h"
#include "level_zero/sysman/source/api/info_log/sysman_os_info_log_instance.h"

#include <memory>
#include <string>

namespace L0 {
namespace Sysman {

class InfoLogInstanceImp : public InfoLogInstance {
  public:
    InfoLogInstanceImp(InfoLog *pInfoLog, const char *pInstanceName,
                       std::unique_ptr<OsInfoLogInstance> pOsInfoLogInstance);
    ~InfoLogInstanceImp() override = default;

    ze_result_t readWithMetadata(uint64_t timeout, uint32_t *pSize, uint8_t *pBuffer,
                                 uint32_t *pRecordCount, zes_intel_info_log_metadata_exp *pDescriptors,
                                 zes_intel_info_log_read_status_exp_t *pReadStatus) override;
    ze_result_t peekWithMetadata(uint64_t timeout, uint32_t *pSize, uint8_t *pBuffer,
                                 uint32_t *pRecordCount, zes_intel_info_log_metadata_exp *pDescriptors,
                                 zes_intel_info_log_read_status_exp_t *pReadStatus) override;
    ze_result_t destroy() override;
    ze_result_t teardown() override;

    const std::string &getInstanceName() const { return instanceName; }
    bool isNamed() const { return named; }

    std::unique_ptr<OsInfoLogInstance> pOsInfoLogInstance;

  private:
    InfoLog *pInfoLog = nullptr;
    std::string instanceName;
    bool named = false;
    bool tornDown = false;
};

} // namespace Sysman
} // namespace L0
