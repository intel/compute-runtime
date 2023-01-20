/*
 * Copyright (C) 2020-2023 Intel Corporation
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

  protected:
    uint64_t gpuBaseAddress = 0llu;
    size_t sizeInPages = 0u;
};
} // namespace NEO
