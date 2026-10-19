/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"

namespace NEO {
size_t getDefaultStagingBufferSize() {
    return MemoryConstants::pageSize;
}

} // namespace NEO
