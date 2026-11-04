/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/info_log/windows/sysman_os_info_log_imp.h"

namespace L0 {
namespace Sysman {

WddmInfoLogImp::WddmInfoLogImp() {}

std::unique_ptr<OsInfoLog> OsInfoLog::create(zes_info_log_format_ext_t format) {
    return std::make_unique<WddmInfoLogImp>();
}

ze_result_t WddmInfoLogImp::getProperties(zes_info_log_ext_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmInfoLogImp::createInstance(const char *pInstanceName,
                                           zes_info_log_instance_ext_desc_t *pDesc,
                                           std::unique_ptr<OsInfoLogInstance> &pOsInfoLogInstance) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmInfoLogImp::getFieldInfo(uint32_t *pCount, zes_intel_info_log_decodable_field_info_exp_t *pFieldInfos) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmInfoLogImp::validateRecords(uint32_t size, uint32_t recordCount,
                                            const zes_info_log_metadata_ext_t *pDescriptors) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmInfoLogImp::readField(uint32_t size, const uint8_t *pBuffer, const zes_info_log_metadata_ext_t *pDescriptors,
                                      uint32_t fieldId, uint32_t recordId,
                                      void *pOutBuffer, size_t bufferLen, size_t *pBytesCopied) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

std::vector<zes_info_log_format_ext_t> OsInfoLog::getSupportedInfoLogFormats() {
    return {};
}

} // namespace Sysman
} // namespace L0
