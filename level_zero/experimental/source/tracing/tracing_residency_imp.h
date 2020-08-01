/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceMakeMemoryResident_Tracing(ze_device_handle_t hDevice,
                                   void *ptr,
                                   size_t size);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceEvictMemory_Tracing(ze_device_handle_t hDevice,
                            void *ptr,
                            size_t size);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceMakeImageResident_Tracing(ze_device_handle_t hDevice,
                                  ze_image_handle_t hImage);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceEvictImage_Tracing(ze_device_handle_t hDevice,
                           ze_image_handle_t hImage);

} // extern "C"
