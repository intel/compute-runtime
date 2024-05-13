/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/driver_experimental/public/zex_context.h"

namespace L0 {
ZE_APIEXPORT ze_result_t ZE_APICALL zeIntelMediaCommunicationCreate(ze_context_handle_t hContext, ze_device_handle_t hDevice, ze_intel_media_communication_desc_t *desc, ze_intel_media_doorbell_handle_desc_t *phDoorbell) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeIntelMediaCommunicationDestroy(ze_context_handle_t hContext, ze_device_handle_t hDevice, ze_intel_media_doorbell_handle_desc_t *phDoorbell) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace L0
