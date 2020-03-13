/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/indirect_heap/indirect_heap.h"

namespace NEO {

class CommandQueue;

struct IndirectHeapFixture {
    virtual void SetUp(CommandQueue *pCmdQ);
    virtual void TearDown() {
    }

    IndirectHeap *pDSH = nullptr;
    IndirectHeap *pIOH = nullptr;
    IndirectHeap *pSSH = nullptr;
};
} // namespace NEO
