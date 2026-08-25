/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/x86_64/stream_copy.h"
#include "shared/source/helpers/x86_64/stream_copy.inl"
#include "shared/test/common/helpers/x86_64/stream_copy_blocks_ult.h"

#include <cstring>

namespace NEO {

namespace StreamCopyBlocksUlt {
uint32_t streamLoadCount = 0u;
uint32_t storeUnalignedCount = 0u;
uint32_t streamStoreCount = 0u;
uint32_t misalignedAccessCount = 0u;

void reset() {
    streamLoadCount = 0u;
    storeUnalignedCount = 0u;
    streamStoreCount = 0u;
    misalignedAccessCount = 0u;
}

namespace {
void countMisalignedAccess(const void *ptr, size_t blockWidth) {
    if ((reinterpret_cast<uintptr_t>(ptr) & (blockWidth - 1u)) != 0u) {
        misalignedAccessCount++;
    }
}
} // namespace

} // namespace StreamCopyBlocksUlt

template <typename Block>
static typename Block::Value streamLoadUlt(const void *alignedSrc) {
    StreamCopyBlocksUlt::streamLoadCount++;
    StreamCopyBlocksUlt::countMisalignedAccess(alignedSrc, Block::width);
    typename Block::Value value{};
    std::memcpy(value.bytes, alignedSrc, Block::width);
    return value;
}

template <typename Block>
static void storeUnalignedUlt(void *dst, typename Block::Value value) {
    StreamCopyBlocksUlt::storeUnalignedCount++;
    std::memcpy(dst, value.bytes, Block::width);
}

template <typename Block>
static void streamStoreUlt(void *alignedDst, typename Block::Value value) {
    StreamCopyBlocksUlt::streamStoreCount++;
    StreamCopyBlocksUlt::countMisalignedAccess(alignedDst, Block::width);
    std::memcpy(alignedDst, value.bytes, Block::width);
}

StreamBlockSse::Value StreamBlockSse::streamLoad(const void *alignedSrc) {
    return streamLoadUlt<StreamBlockSse>(alignedSrc);
}

void StreamBlockSse::storeUnaligned(void *dst, Value value) {
    storeUnalignedUlt<StreamBlockSse>(dst, value);
}

void StreamBlockSse::streamStore(void *alignedDst, Value value) {
    streamStoreUlt<StreamBlockSse>(alignedDst, value);
}

StreamBlockAvx2::Value StreamBlockAvx2::streamLoad(const void *alignedSrc) {
    return streamLoadUlt<StreamBlockAvx2>(alignedSrc);
}

void StreamBlockAvx2::storeUnaligned(void *dst, Value value) {
    storeUnalignedUlt<StreamBlockAvx2>(dst, value);
}

void StreamBlockAvx2::streamStore(void *alignedDst, Value value) {
    streamStoreUlt<StreamBlockAvx2>(alignedDst, value);
}

StreamBlockAvx512::Value StreamBlockAvx512::streamLoad(const void *alignedSrc) {
    return streamLoadUlt<StreamBlockAvx512>(alignedSrc);
}

void StreamBlockAvx512::storeUnaligned(void *dst, Value value) {
    storeUnalignedUlt<StreamBlockAvx512>(dst, value);
}

void StreamBlockAvx512::streamStore(void *alignedDst, Value value) {
    streamStoreUlt<StreamBlockAvx512>(alignedDst, value);
}

void streamCopyFromWriteCombinedSse(void *dst, const void *src, size_t bytes) noexcept {
    streamCopyFromWriteCombinedImpl<StreamBlockSse>(dst, src, bytes);
}

void streamCopyFromWriteCombinedAvx2(void *dst, const void *src, size_t bytes) noexcept {
    streamCopyFromWriteCombinedImpl<StreamBlockAvx2>(dst, src, bytes);
}

void streamCopyFromWriteCombinedAvx512(void *dst, const void *src, size_t bytes) noexcept {
    streamCopyFromWriteCombinedImpl<StreamBlockAvx512>(dst, src, bytes);
}

} // namespace NEO
