/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextCreate_Tracing(ze_driver_handle_t hDriver,
                        const ze_context_desc_t *desc,
                        ze_context_handle_t *phContext);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextDestroy_Tracing(ze_context_handle_t hContext);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextGetStatus_Tracing(ze_context_handle_t hContext);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextSystemBarrier_Tracing(ze_context_handle_t hContext,
                               ze_device_handle_t hDevice);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextMakeMemoryResident_Tracing(ze_context_handle_t hContext,
                                    ze_device_handle_t hDevice,
                                    void *ptr,
                                    size_t size);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextEvictMemory_Tracing(ze_context_handle_t hContext,
                             ze_device_handle_t hDevice,
                             void *ptr,
                             size_t size);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextMakeImageResident_Tracing(ze_context_handle_t hContext,
                                   ze_device_handle_t hDevice,
                                   ze_image_handle_t hImage);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextEvictImage_Tracing(ze_context_handle_t hContext,
                            ze_device_handle_t hDevice,
                            ze_image_handle_t hImage);

} // extern "C"
