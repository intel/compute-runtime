/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/memory_manager/memory_manager.h"
#include "unit_tests/aub_tests/fixtures/aub_fixture.h"

namespace OCLRT {

GraphicsAllocation *AUBFixture::createHostPtrAllocationFromSvmPtr(void *svmPtr, size_t size) {
    GraphicsAllocation *allocation = csr->getMemoryManager()->allocateGraphicsMemory(MockAllocationProperties{false, size}, svmPtr);
    csr->makeResidentHostPtrAllocation(allocation);
    csr->getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), TEMPORARY_ALLOCATION);
    allocation->setAllocationType(GraphicsAllocation::AllocationType::BUFFER);
    allocation->setMemObjectsAllocationWithWritableFlags(true);
    return allocation;
}

} // namespace OCLRT
