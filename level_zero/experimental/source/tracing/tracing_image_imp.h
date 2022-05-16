/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeImageGetPropertiesTracing(ze_device_handle_t hDevice,
                            const ze_image_desc_t *desc,
                            ze_image_properties_t *pImageProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeImageCreateTracing(ze_context_handle_t hContext,
                     ze_device_handle_t hDevice,
                     const ze_image_desc_t *desc,
                     ze_image_handle_t *phImage);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeImageDestroyTracing(ze_image_handle_t hImage);
}
