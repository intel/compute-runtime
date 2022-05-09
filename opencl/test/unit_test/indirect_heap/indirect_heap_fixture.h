/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/indirect_heap/indirect_heap.h"

namespace NEO {

class CommandQueue;

struct IndirectHeapFixture {
    virtual void SetUp(CommandQueue *pCmdQ); // NOLINT(readability-identifier-naming)
    virtual void TearDown() {                // NOLINT(readability-identifier-naming)
    }

    IndirectHeap *pDSH = nullptr;
    IndirectHeap *pIOH = nullptr;
    IndirectHeap *pSSH = nullptr;
};
} // namespace NEO
