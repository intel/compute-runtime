/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

class CommandQueue;
class IndirectHeap;

struct IndirectHeapFixture {
    void setUp(CommandQueue *pCmdQ);
    void tearDown() {
    }

    IndirectHeap *pDSH = nullptr;
    IndirectHeap *pIOH = nullptr;
    IndirectHeap *pSSH = nullptr;
};
} // namespace NEO
