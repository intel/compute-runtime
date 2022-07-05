/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <utility>

namespace NEO {
enum class DebugDataType : uint32_t {
    CMD_QUEUE_CREATED = 0x100,
    CMD_QUEUE_DESTROYED
};
} // namespace NEO
