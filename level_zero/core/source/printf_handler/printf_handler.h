/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/core/source/kernel/kernel.h"

#include <cstdint>

namespace NEO {
class Kernel;
class GraphicsAllocation;
} // namespace NEO

namespace L0 {
struct Device;

struct PrintfHandler : NEO::NonCopyableAndNonMovableClass {
    static NEO::GraphicsAllocation *createPrintfBuffer(Device *device);
    static void printOutput(const KernelImmutableData *kernelData,
                            NEO::GraphicsAllocation *printfBuffer, Device *device, bool useInternalBlitter);
    static size_t getPrintBufferSize();

  protected:
    PrintfHandler() = delete;

    constexpr static size_t printfBufferSize = 4 * MemoryConstants::megaByte;
    constexpr static uint32_t printfSurfaceInitialDataSize = sizeof(uint32_t);
};

static_assert(NEO::NonCopyableAndNonMovable<PrintfHandler>);

} // namespace L0
