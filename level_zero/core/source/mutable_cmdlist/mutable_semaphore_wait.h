/*
 * Copyright (C) 2025-2026 Intel Corporation
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

    virtual ~MutableSemaphoreWait() {}

    virtual void setSemaphoreAddress(GpuAddress semaphoreAddress) = 0;
    virtual void setSemaphoreValue(uint64_t value) = 0;
    virtual void noop() = 0;
    virtual void restoreWithSemaphoreAddress(GpuAddress semaphoreAddress) = 0;
};

} // namespace L0::MCL
