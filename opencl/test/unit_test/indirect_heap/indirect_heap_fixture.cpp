/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/indirect_heap/indirect_heap_fixture.h"

#include "opencl/source/command_queue/command_queue.h"

namespace NEO {

void IndirectHeapFixture::SetUp(CommandQueue *pCmdQ) {
    pDSH = &pCmdQ->getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, 8192);
    pSSH = &pCmdQ->getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 4096);
    pIOH = &pCmdQ->getIndirectHeap(IndirectHeap::Type::INDIRECT_OBJECT, 4096);
}
} // namespace NEO
