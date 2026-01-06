/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/mutable_cmdlist/mcl_types.h"

namespace L0::MCL {

struct MutableSemaphoreWait {
    enum Type {
        regularEventWait,
        cbEventTimestampSyncWait,
        cbEventWait
    };

    MutableSemaphoreWait(size_t inOrderPatchListIndex)
        : inOrderPatchListIndex(inOrderPatchListIndex) {}

    virtual ~MutableSemaphoreWait() {}

    virtual void setSemaphoreAddress(GpuAddress semaphoreAddress) = 0;
    virtual void setSemaphoreValue(uint64_t value) = 0;
    virtual void noop() = 0;
    virtual void restoreWithSemaphoreAddress(GpuAddress semaphoreAddress) = 0;
    size_t getInOrderPatchListIndex() const {
        return inOrderPatchListIndex;
    }

  protected:
    size_t inOrderPatchListIndex;
};

} // namespace L0::MCL
