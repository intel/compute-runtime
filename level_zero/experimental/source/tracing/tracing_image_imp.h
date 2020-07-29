/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeImageGetProperties_Tracing(ze_device_handle_t hDevice,
                             const ze_image_desc_t *desc,
                             ze_image_properties_t *pImageProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeImageCreate_Tracing(ze_context_handle_t hContext,
                      ze_device_handle_t hDevice,
                      const ze_image_desc_t *desc,
                      ze_image_handle_t *phImage);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeImageDestroy_Tracing(ze_image_handle_t hImage);
}
