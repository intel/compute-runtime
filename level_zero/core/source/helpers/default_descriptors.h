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
} // namespace DefaultDescriptors
} // namespace L0