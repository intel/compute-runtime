/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/indirect_heap/indirect_heap_fixture.h"

#include "shared/source/indirect_heap/indirect_heap.h"

#include "opencl/source/command_queue/command_queue.h"

namespace NEO {

void IndirectHeapFixture::setUp(CommandQueue *pCmdQ) {
    pDSH = &pCmdQ->getIndirectHeap(IndirectHeap::Type::dynamicState, 8192);
    pSSH = &pCmdQ->getIndirectHeap(IndirectHeap::Type::surfaceState, 4096);
    pIOH = &pCmdQ->getIndirectHeap(IndirectHeap::Type::indirectObject, 4096);
}
} // namespace NEO
