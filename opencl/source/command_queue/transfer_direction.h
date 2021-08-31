/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {
enum class TransferDirection {
    HostToHost,
    HostToLocal,
    LocalToHost,
    LocalToLocal,
};

struct TransferDirectionHelper {
    static inline TransferDirection fromGfxAllocToHost(const GraphicsAllocation &src) {
        const bool srcLocal = src.isAllocatedInLocalMemoryPool();
        return create(srcLocal, false);
    }

    static inline TransferDirection fromHostToGfxAlloc(const GraphicsAllocation &dst) {
        const bool dstLocal = dst.isAllocatedInLocalMemoryPool();
        return create(false, dstLocal);
    }

    static inline TransferDirection fromGfxAllocToGfxAlloc(const GraphicsAllocation &src, const GraphicsAllocation &dst) {
        const bool srcLocal = src.isAllocatedInLocalMemoryPool();
        const bool dstLocal = dst.isAllocatedInLocalMemoryPool();
        return create(srcLocal, dstLocal);
    }

    static inline TransferDirection create(bool srcLocal, bool dstLocal) {
        if (srcLocal) {
            if (dstLocal) {
                return TransferDirection::LocalToLocal;
            } else {
                return TransferDirection::LocalToHost;
            }
        } else {
            if (dstLocal) {
                return TransferDirection::HostToLocal;
            } else {
                return TransferDirection::HostToHost;
            }
        }
    }
};
} // namespace NEO
