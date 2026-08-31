/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/info_log/sysman_info_log_instance_imp.h"

namespace L0 {
namespace Sysman {

InfoLogInstanceImp::InfoLogInstanceImp(InfoLog *pInfoLog, const char *pInstanceName,
                                       std::unique_ptr<OsInfoLogInstance> pOsInfoLogInstance)
    : pOsInfoLogInstance(std::move(pOsInfoLogInstance)), pInfoLog(pInfoLog) {
    named = (pInstanceName != nullptr);
    if (named) {
        instanceName = pInstanceName;
    }
}

ze_result_t InfoLogInstanceImp::readWithMetadata(uint64_t timeout, uint32_t *pSize, uint8_t *pBuffer,
                                                 uint32_t *pRecordCount, zes_intel_info_log_metadata_exp *pDescriptors,
                                                 zes_intel_info_log_read_status_exp_t *pReadStatus) {
    return pOsInfoLogInstance->readWithMetadata(timeout, pSize, pBuffer, pRecordCount, pDescriptors, pReadStatus);
}

ze_result_t InfoLogInstanceImp::peekWithMetadata(uint64_t timeout, uint32_t *pSize, uint8_t *pBuffer,
                                                 uint32_t *pRecordCount, zes_intel_info_log_metadata_exp *pDescriptors,
                                                 zes_intel_info_log_read_status_exp_t *pReadStatus) {
    return pOsInfoLogInstance->peekWithMetadata(timeout, pSize, pBuffer, pRecordCount, pDescriptors, pReadStatus);
}

ze_result_t InfoLogInstanceImp::destroy() {
    return pInfoLog->destroyInstance(this);
}

ze_result_t InfoLogInstanceImp::teardown() {
    if (tornDown) {
        return ZE_RESULT_SUCCESS;
    }
    tornDown = true;
    return pOsInfoLogInstance->teardown();
}

} // namespace Sysman
} // namespace L0
