/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zes_api.h>
#include <level_zero/zes_intel_gpu_sysman.h>

#include <memory>
#include <vector>

namespace L0 {
namespace Sysman {

struct OsSysman;
class OsInfoLogInstance;

class OsInfoLog {
  public:
    virtual ~OsInfoLog() = default;

    virtual ze_result_t getProperties(zes_info_log_ext_properties_t *pProperties) = 0;
    virtual ze_result_t createInstance(const char *pInstanceName,
                                       zes_info_log_instance_ext_desc_t *pDesc,
                                       std::unique_ptr<OsInfoLogInstance> &pOsInfoLogInstance) = 0;

    virtual ze_result_t getFieldInfo(uint32_t *pCount, zes_intel_info_log_decodable_field_info_exp_t *pFieldInfos) = 0;
    virtual ze_result_t validateRecords(uint32_t size, uint32_t recordCount,
                                        const zes_info_log_metadata_ext_t *pDescriptors) = 0;
    virtual ze_result_t readField(uint32_t size, const uint8_t *pBuffer, const zes_info_log_metadata_ext_t *pDescriptors,
                                  uint32_t fieldId, uint32_t recordId,
                                  void *pOutBuffer, size_t bufferLen, size_t *pBytesCopied) = 0;

    static std::unique_ptr<OsInfoLog> create(zes_info_log_format_ext_t format);
    static std::vector<zes_info_log_format_ext_t> getSupportedInfoLogFormats();
};

} // namespace Sysman
} // namespace L0
