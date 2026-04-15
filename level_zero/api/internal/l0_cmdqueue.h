/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

namespace L0 {

ze_result_t ZE_APICALL zeCommandQueueGetOrdinal(
    ze_command_queue_handle_t hCommandQueue,
    uint32_t *pOrdinal);
ze_result_t ZE_APICALL zeCommandQueueGetIndex(
    ze_command_queue_handle_t hCommandQueue,
    uint32_t *pIndex);
ze_result_t ZE_APICALL zeCommandQueueGetFlags(
    ze_command_queue_handle_t hCommandQueue,
    ze_command_queue_flags_t *pFlags);
ze_result_t ZE_APICALL zeCommandQueueGetMode(
    ze_command_queue_handle_t hCommandQueue,
    ze_command_queue_mode_t *pMode);
ze_result_t ZE_APICALL zeCommandQueueGetPriority(
    ze_command_queue_handle_t hCommandQueue,
    ze_command_queue_priority_t *pPriority);

} // namespace L0
