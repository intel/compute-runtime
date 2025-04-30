/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/driver_experimental/zex_event.h"
#include <level_zero/ze_api.h>

namespace L0 {

namespace DefaultDescriptors {
extern const ze_context_desc_t contextDesc;
extern const ze_command_queue_desc_t commandQueueDesc;
extern const ze_device_mem_alloc_desc_t deviceMemDesc;
extern const ze_host_mem_alloc_desc_t hostMemDesc;
extern const zex_counter_based_event_desc_t counterBasedEventDesc;
} // namespace DefaultDescriptors
} // namespace L0