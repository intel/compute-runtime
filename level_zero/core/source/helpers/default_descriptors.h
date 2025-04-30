/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/ze_api.h>

namespace L0 {

namespace DefaultDescriptors {
extern const ze_command_queue_desc_t commandQueueDesc;
extern const ze_device_mem_alloc_desc_t deviceMemDesc;
extern const ze_host_mem_alloc_desc_t hostMemDesc;
} // namespace DefaultDescriptors
} // namespace L0