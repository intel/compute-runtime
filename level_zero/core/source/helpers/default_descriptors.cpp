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

const ze_context_desc_t contextDesc{
    .stype = ze_structure_type_t::ZE_STRUCTURE_TYPE_CONTEXT_DESC,
    .pNext = nullptr,
    .flags = static_cast<ze_context_flags_t>(0),
};

const ze_command_queue_desc_t commandQueueDesc = {
    .stype = ze_structure_type_t::ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
    .pNext = nullptr,
    .ordinal = 0,
    .index = 0,
    .flags = static_cast<ze_command_queue_flags_t>(ZE_COMMAND_QUEUE_FLAG_IN_ORDER | ZE_COMMAND_QUEUE_FLAG_COPY_OFFLOAD_HINT),
    .mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
    .priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL,
};
} // namespace DefaultDescriptors

} // namespace L0