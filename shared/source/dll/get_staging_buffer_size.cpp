/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/staging_buffer_manager.h"

namespace NEO {
size_t getDefaultStagingBufferSize() {
    return MemoryConstants::pageSize2M;
}

} // namespace NEO