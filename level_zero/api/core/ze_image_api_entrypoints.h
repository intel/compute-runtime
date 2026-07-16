/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

namespace L0 {
ze_result_t ZE_APICALL zeImageGetProperties(
    ze_device_handle_t hDevice,
    const ze_image_desc_t *desc,
    ze_image_properties_t *pImageProperties);

ze_result_t ZE_APICALL zeImageCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_image_desc_t *desc,
    ze_image_handle_t *phImage);

ze_result_t ZE_APICALL zeImageDestroy(
    ze_image_handle_t hImage);

} // namespace L0

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL zeImageGetProperties(
    ze_device_handle_t hDevice,
    const ze_image_desc_t *desc,
    ze_image_properties_t *pImageProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL zeImageCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_image_desc_t *desc,
    ze_image_handle_t *phImage);

ZE_APIEXPORT ze_result_t ZE_APICALL zeImageDestroy(
    ze_image_handle_t hImage);

} // extern "C"
