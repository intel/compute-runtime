/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/indirect_heap/indirect_heap_fixture.h"
#include "runtime/command_queue/command_queue.h"

namespace OCLRT {

void IndirectHeapFixture::SetUp(CommandQueue *pCmdQ) {
    pDSH = &pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 8192);
    pSSH = &pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 4096);
    pIOH = &pCmdQ->getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 4096);
}
} // namespace OCLRT
