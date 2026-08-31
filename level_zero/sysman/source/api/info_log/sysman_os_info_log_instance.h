/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zes_intel_gpu_sysman.h>

namespace L0 {
namespace Sysman {

class OsInfoLogInstance {
  public:
    virtual ~OsInfoLogInstance() = default;

    virtual ze_result_t readWithMetadata(uint64_t timeout, uint32_t *pSize, uint8_t *pBuffer,
                                         uint32_t *pRecordCount,
                                         zes_intel_info_log_metadata_exp *pDescriptors,
                                         zes_intel_info_log_read_status_exp_t *pReadStatus) = 0;
    virtual ze_result_t peekWithMetadata(uint64_t timeout, uint32_t *pSize, uint8_t *pBuffer,
                                         uint32_t *pRecordCount,
                                         zes_intel_info_log_metadata_exp *pDescriptors,
                                         zes_intel_info_log_read_status_exp_t *pReadStatus) = 0;
    virtual ze_result_t teardown() = 0;
    virtual int getTracePipeFd() const = 0;
};

} // namespace Sysman
} // namespace L0
