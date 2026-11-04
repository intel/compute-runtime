/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zes_api.h>
#include <level_zero/zes_intel_gpu_sysman.h>

namespace L0 {
namespace Sysman {

class InfoLogDecoderContext : _zes_intel_info_log_decoder_context_handle_t {
  public:
    virtual ~InfoLogDecoderContext() = default;

    virtual ze_result_t getFieldInfo(uint32_t *pCount, zes_intel_info_log_decodable_field_info_exp_t *pFieldInfos) = 0;
    virtual ze_result_t decodeStart(uint32_t size, const uint8_t *pBuffer, uint32_t recordCount,
                                    const zes_info_log_metadata_ext_t *pDescriptors) = 0;
    virtual ze_result_t readField(uint32_t fieldId, uint32_t recordId, void *pOutBuffer,
                                  size_t bufferLen, size_t *pBytesCopied) = 0;
    virtual ze_result_t decodeStop() = 0;

    static InfoLogDecoderContext *fromHandle(zes_intel_info_log_decoder_context_handle_t handle) {
        return static_cast<InfoLogDecoderContext *>(handle);
    }

    inline zes_intel_info_log_decoder_context_handle_t toHandle() { return this; }
};

} // namespace Sysman
} // namespace L0
