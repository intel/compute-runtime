/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/info_log/linux/sysman_os_info_log_imp.h"

#include "level_zero/sysman/source/api/info_log/linux/sysman_info_log_cper_decoder.h"
#include "level_zero/sysman/source/shared/linux/tracefs_api/sysman_tracefs_api.h"

namespace L0 {
namespace Sysman {

LinuxInfoLogImp::LinuxInfoLogImp(zes_info_log_format_ext_t format)
    : infoLogFormat(format), pTraceFsApi(createTraceFsApi()) {}

std::unique_ptr<OsInfoLog> OsInfoLog::create(zes_info_log_format_ext_t format) {
    return std::make_unique<LinuxInfoLogImp>(format);
}

ze_result_t LinuxInfoLogImp::getFieldInfo(uint32_t *pCount, zes_intel_info_log_decodable_field_info_exp_t *pFieldInfos) {
    return CperDecoder::getFieldInfo(pCount, pFieldInfos);
}

ze_result_t LinuxInfoLogImp::validateRecords(uint32_t size, uint32_t recordCount,
                                             const zes_info_log_metadata_ext_t *pDescriptors) {
    return CperDecoder::validateRecords(size, recordCount, pDescriptors);
}

ze_result_t LinuxInfoLogImp::readField(uint32_t size, const uint8_t *pBuffer, const zes_info_log_metadata_ext_t *pDescriptors,
                                       uint32_t fieldId, uint32_t recordId,
                                       void *pOutBuffer, size_t bufferLen, size_t *pBytesCopied) {
    return CperDecoder::readField(size, pBuffer, pDescriptors, fieldId, recordId, pOutBuffer, bufferLen, pBytesCopied);
}

} // namespace Sysman
} // namespace L0
