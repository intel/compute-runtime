/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "third_party/level_zero/ze_api_ext.h"

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeImageGetProperties_Tracing(ze_device_handle_t hDevice,
                             const ze_image_desc_t *desc,
                             ze_image_properties_t *pImageProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeImageCreate_Tracing(ze_device_handle_t hDevice,
                      const ze_image_desc_t *desc,
                      ze_image_handle_t *phImage);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeImageDestroy_Tracing(ze_image_handle_t hImage);
}
