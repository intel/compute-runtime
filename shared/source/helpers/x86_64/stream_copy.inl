/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/utilities/cpu_info.h"
#include "shared/source/utilities/cpuintrinsics.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <immintrin.h>
#include <limits>

namespace NEO {

inline size_t cacheBypassLimit() noexcept {
    const size_t lastLevelCacheSize = CpuInfo::getInstance().getLastLevelCacheSize();
    if (lastLevelCacheSize == 0) {
        return std::numeric_limits<size_t>::max();
    }
    return lastLevelCacheSize / 2u;
}

template <typename Block>
inline void copyPartialBlock(uint8_t *dst, const uint8_t *src, size_t count) noexcept {
    alignas(Block::width) uint8_t blockBuffer[Block::width];

    const auto *blockStart = alignDown(src, Block::width);
    const size_t offsetInBlock = static_cast<size_t>(src - blockStart);
    Block::storeUnaligned(blockBuffer, Block::streamLoad(blockStart));

    std::memcpy(dst, blockBuffer + offsetInBlock, count);
}

template <typename Block>
inline void streamCopyFromWriteCombinedImpl(void *dst, const void *src, size_t bytes) noexcept {
    constexpr size_t cacheLineSize = MemoryConstants::cacheLineSize;
    constexpr size_t blocksPerCacheLine = cacheLineSize / Block::width;

    auto *dstBytes = static_cast<uint8_t *>(dst);
    auto *srcBytes = static_cast<const uint8_t *>(src);
    size_t remainingBytes = bytes;

    const size_t headOffset = static_cast<size_t>(srcBytes - alignDown(srcBytes, Block::width));
    if (headOffset != 0) {
        const size_t headBytes = std::min(Block::width - headOffset, remainingBytes);
        copyPartialBlock<Block>(dstBytes, srcBytes, headBytes);
        dstBytes += headBytes;
        srcBytes += headBytes;
        remainingBytes -= headBytes;
    }

    const bool useNonTemporalStore = (bytes >= cacheBypassLimit()) && isAligned<Block::width>(dstBytes);

    while (remainingBytes >= cacheLineSize) {
        for (size_t i = 0; i < blocksPerCacheLine; ++i) {
            const auto block = Block::streamLoad(srcBytes);
            if (useNonTemporalStore) {
                Block::streamStore(dstBytes, block);
            } else {
                Block::storeUnaligned(dstBytes, block);
            }
            dstBytes += Block::width;
            srcBytes += Block::width;
        }
        remainingBytes -= cacheLineSize;
    }

    while (remainingBytes >= Block::width) {
        const auto block = Block::streamLoad(srcBytes);
        if (useNonTemporalStore) {
            Block::streamStore(dstBytes, block);
        } else {
            Block::storeUnaligned(dstBytes, block);
        }
        dstBytes += Block::width;
        srcBytes += Block::width;
        remainingBytes -= Block::width;
    }

    if (remainingBytes != 0) {
        std::memcpy(dstBytes, srcBytes, remainingBytes);
    }

    if (useNonTemporalStore) {
        CpuIntrinsics::sfence();
    }
}

} // namespace NEO
