/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/image/image.h"
#include <level_zero/ze_api.h>

namespace L0 {
ze_result_t zeImageGetProperties(
    ze_device_handle_t hDevice,
    const ze_image_desc_t *desc,
    ze_image_properties_t *pImageProperties) {
    return L0::Device::fromHandle(hDevice)->imageGetProperties(desc, pImageProperties);
}

ze_result_t zeImageCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_image_desc_t *desc,
    ze_image_handle_t *phImage) {
    return L0::Context::fromHandle(hContext)->createImage(hDevice, desc, phImage);
}

ze_result_t zeImageDestroy(
    ze_image_handle_t hImage) {
    return L0::Image::fromHandle(hImage)->destroy();
}

} // namespace L0

extern "C" {
ZE_APIEXPORT ze_result_t ZE_APICALL zeImageGetProperties(
    ze_device_handle_t hDevice,
    const ze_image_desc_t *desc,
    ze_image_properties_t *pImageProperties) {
    return L0::zeImageGetProperties(
        hDevice,
        desc,
        pImageProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeImageCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_image_desc_t *desc,
    ze_image_handle_t *phImage) {
    return L0::zeImageCreate(
        hContext,
        hDevice,
        desc,
        phImage);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeImageDestroy(
    ze_image_handle_t hImage) {
    return L0::zeImageDestroy(
        hImage);
}
}
