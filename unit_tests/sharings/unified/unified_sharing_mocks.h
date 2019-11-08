/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/sharings/unified/unified_buffer.h"

namespace NEO {

struct MockUnifiedBuffer : UnifiedBuffer {
    using UnifiedBuffer::acquireCount;
    using UnifiedBuffer::UnifiedBuffer;
};

} // namespace NEO
