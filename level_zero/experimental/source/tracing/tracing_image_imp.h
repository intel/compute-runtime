/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

__zedllexport ze_result_t __zecall
zeImageGetProperties_Tracing(ze_device_handle_t hDevice,
                             const ze_image_desc_t *desc,
                             ze_image_properties_t *pImageProperties);

__zedllexport ze_result_t __zecall
zeImageCreate_Tracing(ze_device_handle_t hDevice,
                      const ze_image_desc_t *desc,
                      ze_image_handle_t *phImage);

__zedllexport ze_result_t __zecall
zeImageDestroy_Tracing(ze_image_handle_t hImage);
}
