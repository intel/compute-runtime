/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/image/image.h"
#include <level_zero/ze_api.h>

extern "C" {

__zedllexport ze_result_t __zecall
zeImageGetProperties(
    ze_device_handle_t hDevice,
    const ze_image_desc_t *desc,
    ze_image_properties_t *pImageProperties) {
    return L0::Device::fromHandle(hDevice)->imageGetProperties(desc, pImageProperties);
}

__zedllexport ze_result_t __zecall
zeImageCreate(
    ze_device_handle_t hDevice,
    const ze_image_desc_t *desc,
    ze_image_handle_t *phImage) {
    return L0::Device::fromHandle(hDevice)->createImage(desc, phImage);
}

__zedllexport ze_result_t __zecall
zeImageDestroy(
    ze_image_handle_t hImage) {
    return L0::Image::fromHandle(hImage)->destroy();
}

} // extern "C"
