/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/device.h"
#include <level_zero/ze_api.h>

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceMakeMemoryResident(
    ze_device_handle_t hDevice,
    void *ptr,
    size_t size) {
    return L0::Device::fromHandle(hDevice)->makeMemoryResident(ptr, size);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceEvictMemory(
    ze_device_handle_t hDevice,
    void *ptr,
    size_t size) {
    return L0::Device::fromHandle(hDevice)->evictMemory(ptr, size);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceMakeImageResident(
    ze_device_handle_t hDevice,
    ze_image_handle_t hImage) {
    return L0::Device::fromHandle(hDevice)->makeImageResident(hImage);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceEvictImage(
    ze_device_handle_t hDevice,
    ze_image_handle_t hImage) {
    return L0::Device::fromHandle(hDevice)->evictImage(hImage);
}
