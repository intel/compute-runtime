/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/api/info_log/sysman_info_log_decoder_context.h"

#include <cstdint>

namespace L0 {
namespace Sysman {

class OsInfoLog;

class InfoLogDecoderContextImp : public InfoLogDecoderContext {
  public:
    InfoLogDecoderContextImp(OsInfoLog *pOsInfoLog);
    ~InfoLogDecoderContextImp() override = default;

    ze_result_t getFieldInfo(uint32_t *pCount, zes_intel_info_log_decodable_field_info_exp_t *pFieldInfos) override;
    ze_result_t decodeStart(uint32_t size, const uint8_t *pBuffer, uint32_t recordCount,
                            const zes_info_log_metadata_ext_t *pDescriptors) override;
    ze_result_t readField(uint32_t fieldId, uint32_t recordId, void *pOutBuffer,
                          size_t bufferLen, size_t *pBytesCopied) override;
    ze_result_t decodeStop() override;

  private:
    OsInfoLog *pOsInfoLog = nullptr;

    uint32_t size = 0;
    const uint8_t *pBuffer = nullptr;
    const zes_info_log_metadata_ext_t *pDescriptors = nullptr;
    uint32_t recordCount = 0;
    bool batchActive = false;
};

} // namespace Sysman
} // namespace L0
