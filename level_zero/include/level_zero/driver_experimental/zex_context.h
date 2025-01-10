/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/ze_api.h>

#include "zex_common.h"

namespace L0 {

ZE_APIEXPORT ze_result_t ZE_APICALL zeIntelMediaCommunicationCreate(ze_context_handle_t hContext, ze_device_handle_t hDevice, ze_intel_media_communication_desc_t *desc, ze_intel_media_doorbell_handle_desc_t *phDoorbell);

ZE_APIEXPORT ze_result_t ZE_APICALL zeIntelMediaCommunicationDestroy(ze_context_handle_t hContext, ze_device_handle_t hDevice, ze_intel_media_doorbell_handle_desc_t *phDoorbell);
} // namespace L0
