/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/helpers/default_descriptors.h"

#include "level_zero/ze_intel_gpu.h"

namespace L0 {
namespace DefaultDescriptors {

static const zex_intel_queue_copy_operations_offload_hint_exp_desc_t copyOffloadHint = {
    .stype = ZEX_INTEL_STRUCTURE_TYPE_QUEUE_COPY_OPERATIONS_OFFLOAD_HINT_EXP_PROPERTIES,
    .pNext = nullptr,
    .copyOffloadEnabled = true};

const ze_command_queue_desc_t commandQueueDesc = {
    .stype = ze_structure_type_t::ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
    .pNext = &copyOffloadHint,
    .ordinal = 0,
    .index = 0,
    .flags = static_cast<ze_command_queue_flags_t>(ZE_COMMAND_QUEUE_FLAG_IN_ORDER),
    .mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
    .priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL,
};
} // namespace DefaultDescriptors

} // namespace L0