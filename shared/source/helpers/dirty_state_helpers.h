/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstddef>
#include <cstdint>

namespace NEO {
class IndirectHeap;

class HeapDirtyState {
  public:
    bool updateAndCheck(const IndirectHeap *heap);
    bool updateAndCheck(const IndirectHeap *heap,
                        const uint64_t comparedGpuAddress, const size_t comparedSize);

  protected:
    uint64_t gpuBaseAddress = 0llu;
    size_t sizeInPages = 0u;
};
} // namespace NEO
