/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

__zedllexport ze_result_t __zecall
zeDeviceMakeMemoryResident_Tracing(ze_device_handle_t hDevice,
                                   void *ptr,
                                   size_t size);

__zedllexport ze_result_t __zecall
zeDeviceEvictMemory_Tracing(ze_device_handle_t hDevice,
                            void *ptr,
                            size_t size);

__zedllexport ze_result_t __zecall
zeDeviceMakeImageResident_Tracing(ze_device_handle_t hDevice,
                                  ze_image_handle_t hImage);

__zedllexport ze_result_t __zecall
zeDeviceEvictImage_Tracing(ze_device_handle_t hDevice,
                           ze_image_handle_t hImage);

} // extern "C"
