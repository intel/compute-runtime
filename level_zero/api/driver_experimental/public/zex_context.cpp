/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/driver_experimental/zex_context.h"

#include "shared/source/device/device.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/device.h"

namespace L0 {
ze_result_t ZE_APICALL zeIntelMediaCommunicationCreate(ze_context_handle_t hContext, ze_device_handle_t hDevice, ze_intel_media_communication_desc_t *desc, ze_intel_media_doorbell_handle_desc_t *phDoorbell) {
    auto device = Device::fromHandle(toInternalType(hDevice));

    if (!device || !desc || !phDoorbell) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (device->getNEODevice()->getMemoryManager()->createMediaContext(device->getRootDeviceIndex(), desc->controlSharedMemoryBuffer, desc->controlSharedMemoryBufferSize,
                                                                       desc->controlBatchBuffer, desc->controlBatchBufferSize, phDoorbell->doorbell)) {
        return ZE_RESULT_SUCCESS;
    }

    return ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t ZE_APICALL zeIntelMediaCommunicationDestroy(ze_context_handle_t hContext, ze_device_handle_t hDevice, ze_intel_media_doorbell_handle_desc_t *phDoorbell) {
    auto device = Device::fromHandle(toInternalType(hDevice));

    if (!device || !phDoorbell) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (device->getNEODevice()->getMemoryManager()->releaseMediaContext(device->getRootDeviceIndex(), phDoorbell->doorbell)) {
        return ZE_RESULT_SUCCESS;
    }

    return ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t ZE_APICALL zexMemFreeRegisterCallbackExt(ze_context_handle_t hContext, zex_memory_free_callback_ext_desc_t *hFreeCallbackDesc, void *ptr) {
    auto context = Context::fromHandle(toInternalType(hContext));

    if (!context || !hFreeCallbackDesc || !ptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (hFreeCallbackDesc->pfnCallback == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    return context->registerMemoryFreeCallback(hFreeCallbackDesc, ptr);
}

} // namespace L0

extern "C" {
ZE_APIEXPORT ze_result_t ZE_APICALL zexMemFreeRegisterCallbackExt(ze_context_handle_t hContext, zex_memory_free_callback_ext_desc_t *hFreeCallbackDesc, void *ptr) {
    return L0::zexMemFreeRegisterCallbackExt(hContext, hFreeCallbackDesc, ptr);
}
ZE_APIEXPORT ze_result_t ZE_APICALL zeIntelMediaCommunicationCreate(ze_context_handle_t hContext, ze_device_handle_t hDevice, ze_intel_media_communication_desc_t *desc, ze_intel_media_doorbell_handle_desc_t *phDoorbell) {
    return L0::zeIntelMediaCommunicationCreate(hContext, hDevice, desc, phDoorbell);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeIntelMediaCommunicationDestroy(ze_context_handle_t hContext, ze_device_handle_t hDevice, ze_intel_media_doorbell_handle_desc_t *phDoorbell) {
    return L0::zeIntelMediaCommunicationDestroy(hContext, hDevice, phDoorbell);
}
} // extern "C"
