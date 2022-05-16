/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextCreateTracing(ze_driver_handle_t hDriver,
                       const ze_context_desc_t *desc,
                       ze_context_handle_t *phContext);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextDestroyTracing(ze_context_handle_t hContext);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextGetStatusTracing(ze_context_handle_t hContext);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextSystemBarrierTracing(ze_context_handle_t hContext,
                              ze_device_handle_t hDevice);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextMakeMemoryResidentTracing(ze_context_handle_t hContext,
                                   ze_device_handle_t hDevice,
                                   void *ptr,
                                   size_t size);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextEvictMemoryTracing(ze_context_handle_t hContext,
                            ze_device_handle_t hDevice,
                            void *ptr,
                            size_t size);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextMakeImageResidentTracing(ze_context_handle_t hContext,
                                  ze_device_handle_t hDevice,
                                  ze_image_handle_t hImage);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextEvictImageTracing(ze_context_handle_t hContext,
                           ze_device_handle_t hDevice,
                           ze_image_handle_t hImage);

} // extern "C"
