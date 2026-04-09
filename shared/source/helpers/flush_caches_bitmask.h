/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

namespace FlushCachesBitmask {
// Keep bit 0 free for backwards compatibility with FlushAllCaches debug variable, which is set to 1 to flush all caches.
constexpr int64_t dcFlush = 1 << 1;
constexpr int64_t renderTargetCache = 1 << 2;
constexpr int64_t instructionCache = 1 << 3;
constexpr int64_t textureCache = 1 << 4;
constexpr int64_t pipeControl = 1 << 5;
constexpr int64_t vfCache = 1 << 6;
constexpr int64_t constantCache = 1 << 7;
constexpr int64_t stateCache = 1 << 8;
constexpr int64_t tlb = 1 << 9;
constexpr int64_t hdcPipeline = 1 << 10;
constexpr int64_t unTypedDataPortCache = 1 << 11;
constexpr int64_t compressionControlSurfaceCcs = 1 << 12;
constexpr int64_t l2Flush = 1 << 13;
constexpr int64_t l2TransientFlush = 1 << 14;

constexpr int64_t allCaches = dcFlush | renderTargetCache | instructionCache | textureCache |
                              pipeControl | vfCache | constantCache | stateCache | tlb |
                              hdcPipeline | unTypedDataPortCache | compressionControlSurfaceCcs |
                              l2Flush | l2TransientFlush;
} // namespace FlushCachesBitmask

} // namespace NEO
