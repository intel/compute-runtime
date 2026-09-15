/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/x86_64/stream_copy.h"
#include "shared/source/helpers/x86_64/stream_copy.inl"
#include "shared/source/helpers/x86_64/stream_copy_blocks.h"

namespace NEO {

void streamCopyFromWriteCombinedSse(void *dst, const void *src, size_t bytes) noexcept {
    streamCopyFromWriteCombinedImpl<StreamBlockSse>(dst, src, bytes);
}

} // namespace NEO
