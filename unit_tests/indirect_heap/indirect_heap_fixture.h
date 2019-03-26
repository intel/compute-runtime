/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/indirect_heap/indirect_heap.h"

namespace NEO {

class CommandQueue;

struct IndirectHeapFixture {
    IndirectHeapFixture() : pDSH(nullptr),
                            pIOH(nullptr),
                            pSSH(nullptr) {
    }

    virtual void SetUp(CommandQueue *pCmdQ);
    virtual void TearDown() {
    }

    IndirectHeap *pDSH;
    IndirectHeap *pIOH;
    IndirectHeap *pSSH;
};
} // namespace NEO
