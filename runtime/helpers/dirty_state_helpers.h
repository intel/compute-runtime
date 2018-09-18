/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdlib>
#include <cstdint>

namespace OCLRT {
class IndirectHeap;

class HeapDirtyState {
  public:
    bool updateAndCheck(const IndirectHeap *heap);

  protected:
    uint64_t gpuBaseAddress = 0llu;
    size_t sizeInPages = 0u;
};
} // namespace OCLRT
