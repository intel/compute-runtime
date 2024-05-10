/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/driver_experimental/public/zex_common.h"
#include <level_zero/ze_api.h>

namespace L0 {

ZE_APIEXPORT ze_result_t ZE_APICALL zeIntelMediaCommunicationCreate(ze_context_handle_t hContext, ze_device_handle_t hDevice, ze_intel_media_communication_desc_t *desc, uint64_t *doorbellHandle);

ZE_APIEXPORT ze_result_t ZE_APICALL zeIntelMediaCommunicationDestroy(ze_context_handle_t hContext, ze_device_handle_t hDevice, uint64_t doorbellHandle);
} // namespace L0
