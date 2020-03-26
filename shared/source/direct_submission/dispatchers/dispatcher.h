/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstddef>
#include <cstdint>

namespace NEO {

struct HardwareInfo;
class LinearStream;

template <typename GfxFamily>
class Dispatcher {
  public:
    static void dispatchStartCommandBuffer(LinearStream &cmdBuffer, uint64_t gpuStartAddress);
    static size_t getSizeStartCommandBuffer();

    static void dispatchStopCommandBuffer(LinearStream &cmdBuffer);
    static size_t getSizeStopCommandBuffer();

    static void dispatchStoreDwordCommand(LinearStream &cmdBuffer, uint64_t gpuVa, uint32_t value);
    static size_t getSizeStoreDwordCommand();
};
} // namespace NEO
