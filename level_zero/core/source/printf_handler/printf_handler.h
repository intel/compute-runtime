/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"

#include "level_zero/core/source/kernel/kernel.h"

#include <cstdint>

namespace NEO {
class Kernel;
class GraphicsAllocation;
} // namespace NEO

namespace L0 {
struct Device;

struct PrintfHandler {
    static NEO::GraphicsAllocation *createPrintfBuffer(Device *device);
    static void printOutput(const KernelImmutableData *kernelData,
                            NEO::GraphicsAllocation *printfBuffer, Device *device, bool useInternalBlitter);
    static size_t getPrintBufferSize();

  protected:
    PrintfHandler(const PrintfHandler &) = delete;
    PrintfHandler &operator=(PrintfHandler const &) = delete;
    PrintfHandler() = delete;

    constexpr static size_t printfBufferSize = 4 * MemoryConstants::megaByte;
    constexpr static uint32_t printfSurfaceInitialDataSize = sizeof(uint32_t);
};

} // namespace L0
