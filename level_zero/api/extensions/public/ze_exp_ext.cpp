/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/extensions/public/ze_exp_ext.h"

#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/source/kernel/kernel.h"

#if defined(__cplusplus)
extern "C" {
#endif

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSetGlobalOffsetExp(
    ze_kernel_handle_t hKernel,
    uint32_t offsetX,
    uint32_t offsetY,
    uint32_t offsetZ) {
    return L0::Kernel::fromHandle(hKernel)->setGlobalOffsetExp(offsetX, offsetY, offsetZ);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeImageGetMemoryPropertiesExp(
    ze_image_handle_t hImage,
    ze_image_memory_properties_exp_t *pMemoryProperties) {
    return L0::Image::fromHandle(hImage)->getMemoryProperties(pMemoryProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeImageGetAllocPropertiesExt(
    ze_context_handle_t hContext,
    ze_image_handle_t hImage,
    ze_image_allocation_ext_properties_t *pAllocProperties) {
    return L0::Context::fromHandle(hContext)->getImageAllocProperties(L0::Image::fromHandle(hImage), pAllocProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeImageViewCreateExp(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_image_desc_t *desc,
    ze_image_handle_t hImage,
    ze_image_handle_t *phImageView) {
    return L0::Image::fromHandle(hImage)->createView(L0::Device::fromHandle(hDevice), desc, phImageView);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventQueryTimestampsExp(
    ze_event_handle_t hEvent,
    ze_device_handle_t hDevice,
    uint32_t *pCount,
    ze_kernel_timestamp_result_t *pTimestamps) {
    return L0::Event::fromHandle(hEvent)->queryTimestampsExp(L0::Device::fromHandle(hDevice), pCount, pTimestamps);
}

#if defined(__cplusplus)
} // extern "C"
#endif
