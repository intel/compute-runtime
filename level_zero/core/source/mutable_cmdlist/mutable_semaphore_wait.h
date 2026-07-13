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

    MutableSemaphoreWait(uint64_t gpuDestination, void *cmdView, size_t commandSize)
        : gpuDestinationAddress(gpuDestination), commandView(cmdView), commandSize(commandSize) {}
    virtual ~MutableSemaphoreWait() = default;

    virtual void setSemaphoreAddress(GpuAddress semaphoreAddress) = 0;
    virtual void setSemaphoreValue(uint64_t value) = 0;
    virtual void noop() = 0;
    virtual void restoreWithSemaphoreAddress(GpuAddress semaphoreAddress) = 0;

    uint64_t getGpuDestinationAddress() const { return gpuDestinationAddress; }
    void *getCommandView() const { return commandView; }
    size_t getCommandSize() const { return commandSize; }

  protected:
    uint64_t gpuDestinationAddress = 0;
    void *commandView = nullptr;
    size_t commandSize = 0;
};

} // namespace L0::MCL
