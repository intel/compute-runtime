/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/info_log/sysman_info_log_instance_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/preprocessor.h"

namespace L0 {
namespace Sysman {

static ze_result_t validateReadArguments(const uint32_t *pSize, const uint32_t *pRecordCount, const zes_info_log_metadata_ext_t *pDescriptors,
                                         const zes_info_log_read_status_ext_t *pReadStatus) {
    bool hasExtension = (pReadStatus != nullptr && pReadStatus->pNext != nullptr);
    bool isQuery = (*pSize == 0 || *pRecordCount == 0);
    if (!isQuery && pDescriptors != nullptr) {
        for (uint32_t i = 0; i < *pRecordCount && !hasExtension; i++) {
            hasExtension = (pDescriptors[i].pNext != nullptr);
        }
    }
    if (hasExtension) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Extension structures are not supported, pNext must be nullptr, returning error: 0x%x\n", NEO_FUNCTION_NAME, ZE_RESULT_ERROR_INVALID_ARGUMENT);
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    return ZE_RESULT_SUCCESS;
}

InfoLogInstanceImp::InfoLogInstanceImp(InfoLog *pInfoLog, const char *pInstanceName,
                                       std::unique_ptr<OsInfoLogInstance> pOsInfoLogInstance)
    : pOsInfoLogInstance(std::move(pOsInfoLogInstance)), pInfoLog(pInfoLog) {
    named = (pInstanceName != nullptr);
    if (named) {
        instanceName = pInstanceName;
    }
}

ze_result_t InfoLogInstanceImp::readWithMetadata(uint64_t timeout, uint32_t *pSize, uint8_t *pBuffer,
                                                 uint32_t *pRecordCount, zes_info_log_metadata_ext_t *pDescriptors,
                                                 zes_info_log_read_status_ext_t *pReadStatus) {
    auto result = validateReadArguments(pSize, pRecordCount, pDescriptors, pReadStatus);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    return pOsInfoLogInstance->readWithMetadata(timeout, pSize, pBuffer, pRecordCount, pDescriptors, pReadStatus);
}

ze_result_t InfoLogInstanceImp::peekWithMetadata(uint64_t timeout, uint32_t *pSize, uint8_t *pBuffer,
                                                 uint32_t *pRecordCount, zes_info_log_metadata_ext_t *pDescriptors,
                                                 zes_info_log_read_status_ext_t *pReadStatus) {
    auto result = validateReadArguments(pSize, pRecordCount, pDescriptors, pReadStatus);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
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
