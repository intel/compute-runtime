/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/queue_throttle.h"

#include <level_zero/ze_api.h>

namespace L0 {

// NOLINTBEGIN(readability-identifier-naming)

constexpr ze_structure_type_t ZE_STRUCTURE_TYPE_QUEUE_THROTTLE_EXT_DESC = static_cast<ze_structure_type_t>(0x00050010);

struct ze_queue_throttle_ext_desc_t {
    ze_structure_type_t stype = ZE_STRUCTURE_TYPE_QUEUE_THROTTLE_EXT_DESC;
    const void *pNext = nullptr;
    NEO::QueueThrottle throttle = NEO::QueueThrottle::MEDIUM;
};

// NOLINTEND(readability-identifier-naming)

} // namespace L0
