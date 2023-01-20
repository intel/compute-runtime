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
    void setUp(CommandQueue *pCmdQ);
    void tearDown() {
    }

    IndirectHeap *pDSH = nullptr;
    IndirectHeap *pIOH = nullptr;
    IndirectHeap *pSSH = nullptr;
};
} // namespace NEO
