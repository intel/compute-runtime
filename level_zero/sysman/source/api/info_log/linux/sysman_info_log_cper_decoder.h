/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zes_intel_gpu_sysman.h>

#include <cstddef>
#include <cstdint>

namespace L0 {
namespace Sysman {

class CperDecoder {
  public:
    enum FieldIndex : uint32_t {
        sigIdField = 0,
        uuidField,
        fieldCount
    };

    static ze_result_t getFieldInfo(uint32_t *pCount, zes_intel_info_log_decodable_field_info_exp_t *pFieldInfos);

    static ze_result_t validateRecords(uint32_t size, uint32_t recordCount, const zes_info_log_metadata_ext_t *pDescriptors);

    static ze_result_t readField(uint32_t size, const uint8_t *pBuffer, const zes_info_log_metadata_ext_t *pDescriptors,
                                 uint32_t fieldId, uint32_t recordId,
                                 void *pOutBuffer, size_t bufferLen, size_t *pBytesCopied);
};

} // namespace Sysman
} // namespace L0
