/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/x86_64/stream_copy.h"

#include <cstddef>
#include <cstdint>

namespace NEO {

namespace StreamCopyBlocksUlt {
extern uint32_t streamLoadCount;
extern uint32_t storeUnalignedCount;
extern uint32_t streamStoreCount;
extern uint32_t misalignedAccessCount;

void reset();
} // namespace StreamCopyBlocksUlt

template <size_t blockWidth>
struct StreamBlockValueUlt {
    uint8_t bytes[blockWidth];
};

struct StreamBlockSse {
    static constexpr size_t width = streamCopySseWidth;
    using Value = StreamBlockValueUlt<width>;

    static Value streamLoad(const void *alignedSrc);
    static void storeUnaligned(void *dst, Value value);
    static void streamStore(void *alignedDst, Value value);
};

struct StreamBlockAvx2 {
    static constexpr size_t width = streamCopyAvx2Width;
    using Value = StreamBlockValueUlt<width>;

    static Value streamLoad(const void *alignedSrc);
    static void storeUnaligned(void *dst, Value value);
    static void streamStore(void *alignedDst, Value value);
};

struct StreamBlockAvx512 {
    static constexpr size_t width = streamCopyAvx512Width;
    using Value = StreamBlockValueUlt<width>;

    static Value streamLoad(const void *alignedSrc);
    static void storeUnaligned(void *dst, Value value);
    static void streamStore(void *alignedDst, Value value);
};

} // namespace NEO
