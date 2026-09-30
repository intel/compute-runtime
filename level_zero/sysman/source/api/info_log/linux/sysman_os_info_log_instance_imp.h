/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/api/info_log/sysman_os_info_log_instance.h"

#include <cstdint>
#include <string>
#include <vector>

struct tracefs_instance; // NOLINT(readability-identifier-naming)

namespace L0 {
namespace Sysman {

class TraceFsApi;

extern const std::vector<std::string> tracefsPaths;

class LinuxInfoLogInstanceImp : public OsInfoLogInstance {
  public:
    LinuxInfoLogInstanceImp(TraceFsApi *pTraceFsApi, zes_info_log_format_ext_t format,
                            struct tracefs_instance *pTraceFsInstance, const std::string &instanceName,
                            bool instanceWasPreExisting, bool eventWasAlreadyEnabled, bool tracingWasAlreadyOn,
                            int ownershipFd = -1);
    ~LinuxInfoLogInstanceImp() override;

    ze_result_t readWithMetadata(uint64_t timeout, uint32_t *pSize, uint8_t *pBuffer,
                                 uint32_t *pRecordCount, zes_info_log_metadata_ext_t *pDescriptors,
                                 zes_info_log_read_status_ext_t *pReadStatus) override;
    ze_result_t peekWithMetadata(uint64_t timeout, uint32_t *pSize, uint8_t *pBuffer,
                                 uint32_t *pRecordCount, zes_info_log_metadata_ext_t *pDescriptors,
                                 zes_info_log_read_status_ext_t *pReadStatus) override;
    ze_result_t teardown() override;
    int getTracePipeFd() const override { return tracePipeFd; }
    ze_result_t applyBufferConfiguration(zes_info_log_instance_ext_desc_t *pDesc);
    ze_result_t startCollection();

  protected:
    MOCKABLE_VIRTUAL uint64_t getCurrentTimeInMs();

  private:
    enum class StopReason { drained,
                            recordLimit,
                            sizeLimit,
                            deadline,
                            error };

    bool isDeadlineReached(uint64_t deadlineMs);
    bool countCperRecordsAndSize(const std::string &traceOutput, uint32_t &cperCount, uint32_t &totalSize,
                                 uint64_t deadlineMs);
    ze_result_t queryRecords(uint64_t deadlineMs, uint32_t *pSize, uint32_t *pRecordCount, bool &anyFound,
                             StopReason &stopReason);
    ze_result_t extractFromTracePipe(uint64_t deadlineMs, uint32_t *pSize, uint8_t *pBuffer,
                                     uint32_t *pRecordCount, zes_info_log_metadata_ext_t *pDescriptors,
                                     StopReason &stopReason, uint64_t &consumedDataSize);
    ze_result_t extractFromTraceSnapshot(uint64_t deadlineMs, uint32_t *pSize, uint8_t *pBuffer,
                                         uint32_t *pRecordCount, zes_info_log_metadata_ext_t *pDescriptors,
                                         StopReason &stopReason, uint64_t &consumedDataSize);
    ze_result_t collect(uint64_t timeout, uint32_t *pSize, uint8_t *pBuffer, uint32_t *pRecordCount,
                        zes_info_log_metadata_ext_t *pDescriptors,
                        zes_info_log_read_status_ext_t *pReadStatus, bool consuming);
    ze_result_t reportReadStatus(zes_info_log_read_status_ext_t *pReadStatus, StopReason stopReason,
                                 bool queryCall, bool anyRecordFound, uint64_t consumedDataSize);
    ze_result_t openTracePipe();
    void closeTracePipe();
    ze_result_t applyBufferSize(uint32_t *pBufferSize);
    uint32_t readTotalBufferSize();
    uint32_t getPerCpuBufferCount();
    uint32_t scanPerCpuBufferCount();
    uint64_t readDroppedRecordTotal(bool &isComplete);
    int64_t consumeDroppedRecordCount(bool isReported);
    void setImmediateWakeBufferPercent();
    void restoreBufferPercent();
    void restoreBufferSize();
    void restoreBufferConfiguration();

    static constexpr size_t kTraceLineBufferSize = 8192;
    static constexpr size_t kMaxAccumulatedLineSize = 16384;

    TraceFsApi *pTraceFsApi = nullptr;
    zes_info_log_format_ext_t infoLogFormat = ZES_INFO_LOG_FORMAT_EXT_CPER;
    struct tracefs_instance *pTraceFsInstance = nullptr;
    std::string instanceName;
    bool instanceWasPreExisting = false;
    bool eventWasAlreadyEnabled = false;
    bool tracingWasAlreadyOn = false;
    int savedBufferPercent = -1;
    long long savedPerCpuBufferSizeKb = -1;
    uint32_t perCpuBufferCount = 0;
    uint64_t reportedDroppedRecordTotal = 0;
    uint64_t warnedDroppedRecordTotal = 0;
    int tracePipeFd = -1;
    // Instance directory descriptor holding the advisory lock which marks the tracefs instance as
    // owned by this API. Closing it on teardown is what releases the lock. -1 when nothing is owned.
    int ownershipFd = -1;
    bool tornDown = false;
    std::string lineBuffer;
};

} // namespace Sysman
} // namespace L0
