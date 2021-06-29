/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/extensions/public/ze_exp_ext.h"

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

#if defined(__cplusplus)
} // extern "C"
#endif
