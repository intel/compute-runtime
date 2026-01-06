/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/mutable_cmdlist/mutable_semaphore_wait.h"

namespace L0::MCL {

template <typename GfxFamily>
struct MutableSemaphoreWaitHw : public MutableSemaphoreWait {
    using SemaphoreWait = typename GfxFamily::MI_SEMAPHORE_WAIT;

    MutableSemaphoreWaitHw(void *semWait, size_t offset, size_t inOrderPatchListIndex, Type type, bool qwordDataIndirect)
        : MutableSemaphoreWait(inOrderPatchListIndex),
          semWait(semWait),
          offset(offset),
          type(type),
          qwordDataIndirect(qwordDataIndirect) {}
    ~MutableSemaphoreWaitHw() override {}

    void setSemaphoreAddress(GpuAddress semaphoreAddress) override;
    void setSemaphoreValue(uint64_t value) override;
    void noop() override;
    void restoreWithSemaphoreAddress(GpuAddress semaphoreAddress) override;

  protected:
    void *semWait;
    size_t offset;
    Type type;
    bool qwordDataIndirect = false;

  private:
    static GpuAddress commandAddressRange;
};

} // namespace L0::MCL
