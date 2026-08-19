/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/api/info_log/sysman_os_info_log.h"

#include <string>

namespace L0 {
namespace Sysman {

class TraceFsApi;
class LinuxInfoLogImp : public OsInfoLog {
  public:
    static std::unique_ptr<TraceFsApi> (*createTraceFsApi)();

    LinuxInfoLogImp(zes_intel_info_log_format_exp_t format);
    ~LinuxInfoLogImp() override;

    ze_result_t getProperties(zes_intel_info_log_properties_exp_t *pProperties) override;
    ze_result_t infoLogRead(uint32_t *pSize, uint8_t *pBuffer) override;
    ze_result_t infoLogEnable(bool state) override;

  private:
    uint32_t countCperRecords(const std::string &traceOutput, uint32_t bufferSize);
    ze_result_t extractCperRecords(int fd, uint32_t cperCount, uint8_t *pBuffer, uint32_t bufferSize, uint32_t &aggregatedCperLen);
    int getTracePipeFd() const;
    ze_result_t openTracePipe();
    void closeTracePipe();
    void setImmediateWakeBufferPercent();
    void restoreBufferPercent();

  protected:
    zes_intel_info_log_format_exp_t infoLogFormat;
    std::unique_ptr<TraceFsApi> pTraceFsApi;
    int savedBufferPercent = -1;
    std::string lineBuffer;
};

} // namespace Sysman
} // namespace L0
