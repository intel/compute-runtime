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

const ze_device_mem_alloc_desc_t deviceMemDesc = {
    .stype = ze_structure_type_t::ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC,
    .pNext = nullptr,
    .flags = static_cast<ze_device_mem_alloc_flags_t>(ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_CACHED),
    .ordinal = 0};

const ze_host_mem_alloc_desc_t hostMemDesc = {
    .stype = ze_structure_type_t::ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC,
    .pNext = nullptr,
    .flags = static_cast<ze_host_mem_alloc_flags_t>(ZE_HOST_MEM_ALLOC_FLAG_BIAS_CACHED | ZE_HOST_MEM_ALLOC_FLAG_BIAS_INITIAL_PLACEMENT)};

const zex_counter_based_event_desc_t counterBasedEventDesc = {
    .stype = ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC,
    .pNext = nullptr,
    .flags = static_cast<zex_counter_based_event_exp_flags_t>(
        ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE |
        ZEX_COUNTER_BASED_EVENT_FLAG_NON_IMMEDIATE |
        ZEX_COUNTER_BASED_EVENT_FLAG_HOST_VISIBLE),
    .signalScope = static_cast<ze_event_scope_flags_t>(ZE_EVENT_SCOPE_FLAG_HOST),
    .waitScope = static_cast<ze_event_scope_flags_t>(ZE_EVENT_SCOPE_FLAG_DEVICE)};
} // namespace DefaultDescriptors

} // namespace L0