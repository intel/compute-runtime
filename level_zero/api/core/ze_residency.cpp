/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/device.h"
#include <level_zero/ze_api.h>

extern "C" {

__zedllexport ze_result_t __zecall
zeDeviceMakeMemoryResident(
    ze_device_handle_t hDevice,
    void *ptr,
    size_t size) {
    return L0::Device::fromHandle(hDevice)->makeMemoryResident(ptr, size);
}

__zedllexport ze_result_t __zecall
zeDeviceEvictMemory(
    ze_device_handle_t hDevice,
    void *ptr,
    size_t size) {
    return L0::Device::fromHandle(hDevice)->evictMemory(ptr, size);
}

__zedllexport ze_result_t __zecall
zeDeviceMakeImageResident(
    ze_device_handle_t hDevice,
    ze_image_handle_t hImage) {
    return L0::Device::fromHandle(hDevice)->makeImageResident(hImage);
}

__zedllexport ze_result_t __zecall
zeDeviceEvictImage(
    ze_device_handle_t hDevice,
    ze_image_handle_t hImage) {
    return L0::Device::fromHandle(hDevice)->evictImage(hImage);
}

} // extern "C"
