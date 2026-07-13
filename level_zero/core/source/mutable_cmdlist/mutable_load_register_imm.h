/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/mutable_cmdlist/mcl_types.h"

namespace L0::MCL {

struct MutableLoadRegisterImm {
    MutableLoadRegisterImm(uint64_t gpuDestination, void *cmdView, size_t commandSize)
        : gpuDestinationAddress(gpuDestination), commandView(cmdView), commandSize(commandSize) {}
    virtual ~MutableLoadRegisterImm() = default;

    virtual void noop() = 0;
    virtual void restore() = 0;
    virtual void setValue(uint32_t value) = 0;

    uint64_t getGpuDestinationAddress() const { return gpuDestinationAddress; }
    void *getCommandView() const { return commandView; }
    size_t getCommandSize() const { return commandSize; }

  protected:
    uint64_t gpuDestinationAddress = 0;
    void *commandView = nullptr;
    size_t commandSize = 0;
};

} // namespace L0::MCL
