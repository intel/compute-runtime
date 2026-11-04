/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/info_log/sysman_info_log_decoder_context_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/preprocessor.h"

#include "level_zero/sysman/source/api/info_log/sysman_os_info_log.h"

namespace L0 {
namespace Sysman {

InfoLogDecoderContextImp::InfoLogDecoderContextImp(OsInfoLog *pOsInfoLog)
    : pOsInfoLog(pOsInfoLog) {}

ze_result_t InfoLogDecoderContextImp::getFieldInfo(uint32_t *pCount, zes_intel_info_log_decodable_field_info_exp_t *pFieldInfos) {
    return this->pOsInfoLog->getFieldInfo(pCount, pFieldInfos);
}

ze_result_t InfoLogDecoderContextImp::decodeStart(uint32_t size, const uint8_t *pBuffer, uint32_t recordCount,
                                                  const zes_info_log_metadata_ext_t *pDescriptors) {
    if (this->batchActive) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): The decoder context is already decoding a batch of %u records, returning error: 0x%x\n", NEO_FUNCTION_NAME, this->recordCount, ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE);
        return ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE;
    }

    if (recordCount > 0) {
        if (pBuffer == nullptr) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): pBuffer is nullptr while recordCount is %u, returning error: 0x%x\n", NEO_FUNCTION_NAME, recordCount, ZE_RESULT_ERROR_INVALID_NULL_POINTER);
            return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
        }
        if (pDescriptors == nullptr) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): pDescriptors is nullptr while recordCount is %u, returning error: 0x%x\n", NEO_FUNCTION_NAME, recordCount, ZE_RESULT_ERROR_INVALID_NULL_POINTER);
            return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
        }
    }

    auto result = this->pOsInfoLog->validateRecords(size, recordCount, pDescriptors);
    if (result != ZE_RESULT_SUCCESS) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): The backend rejected the %u records of the batch, returning error: 0x%x\n", NEO_FUNCTION_NAME, recordCount, result);
        return result;
    }

    this->size = size;
    this->pBuffer = pBuffer;
    this->pDescriptors = pDescriptors;
    this->recordCount = recordCount;
    this->batchActive = true;
    return ZE_RESULT_SUCCESS;
}

ze_result_t InfoLogDecoderContextImp::readField(uint32_t fieldId, uint32_t recordId, void *pOutBuffer,
                                                size_t bufferLen, size_t *pBytesCopied) {
    if (!this->batchActive) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): The decoder context is not decoding a batch, returning error: 0x%x\n", NEO_FUNCTION_NAME, ZE_RESULT_ERROR_UNINITIALIZED);
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    if (recordId >= this->recordCount) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Record %u is not within the %u records being decoded, returning error: 0x%x\n", NEO_FUNCTION_NAME, recordId, this->recordCount, ZE_RESULT_ERROR_INVALID_ARGUMENT);
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    return this->pOsInfoLog->readField(this->size, this->pBuffer, this->pDescriptors, fieldId, recordId,
                                       pOutBuffer, bufferLen, pBytesCopied);
}

ze_result_t InfoLogDecoderContextImp::decodeStop() {
    if (!this->batchActive) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): The decoder context is not decoding a batch, returning error: 0x%x\n", NEO_FUNCTION_NAME, ZE_RESULT_ERROR_UNINITIALIZED);
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    this->size = 0;
    this->pBuffer = nullptr;
    this->pDescriptors = nullptr;
    this->recordCount = 0;
    this->batchActive = false;
    return ZE_RESULT_SUCCESS;
}

} // namespace Sysman
} // namespace L0
