/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/api/info_log/sysman_os_info_log.h"

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
    ze_result_t infoLogRead(uint32_t *pSize, uint8_t *pBuffer) override;
    ze_result_t infoLogEnable(zes_intel_info_log_enable_descriptor_exp *pEnableDescriptor) override;
    ze_result_t infoLogDisable() override;
    ze_result_t infoLogReadWithMetaData(uint32_t *pSize, uint8_t *pBuffer,
                                        uint32_t *pEventCount, zes_intel_info_log_metadata_exp *pDescriptors) override;

  private:
    uint32_t countCperRecords(const std::string &traceOutput, uint32_t bufferSize);
    ze_result_t extractCperRecords(int fd, uint32_t cperCount, uint8_t *pBuffer, uint32_t bufferSize, uint32_t &aggregatedCperLen);
    void countCperRecordsAndSize(const std::string &traceOutput, uint32_t &cperCount, uint32_t &totalSize);
    bool isInstancedCollectionAvailable();

    static constexpr size_t kTraceLineBufferSize = 8192;     // Sized to handle max CPER line (~8292 bytes)
    static constexpr size_t kMaxAccumulatedLineSize = 16384; // Safety limit: 2x buffer size
    bool checkInstancePreExists(const char *instanceName);
    bool checkEventEnabled(struct tracefs_instance *instance);
    bool checkTracingOn(struct tracefs_instance *instance);
    void cleanupInstanceOnFailure(struct tracefs_instance *instance, bool isNew);

    // The trace_pipe descriptor is owned by the sysman driver so that the events
    // path can poll it; these helpers are the only accessors to it.
    int getTracePipeFd() const;
    ze_result_t openTracePipe(struct tracefs_instance *instance);
    void closeTracePipe();
    void setImmediateWakeBufferPercent(struct tracefs_instance *instance);
    void restoreBufferPercent();

  protected:
    zes_intel_info_log_format_exp_t infoLogFormat;
    std::unique_ptr<TraceFsApi> pTraceFsApi;

    struct tracefs_instance *pTraceFsInstance = nullptr;
    bool isEnabled = false;
    bool instanceWasPreExisting = false;
    bool eventWasAlreadyEnabled = false;
    bool tracingWasAlreadyOn = false;
    std::string activeInstanceName;
    int savedBufferPercent = -1;
    struct tracefs_instance *savedBufferPercentInstance = nullptr;
    std::string lineBuffer;
};

} // namespace Sysman
} // namespace L0
