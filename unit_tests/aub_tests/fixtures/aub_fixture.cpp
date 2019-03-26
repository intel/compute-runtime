/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/aub_tests/fixtures/aub_fixture.h"

#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/memory_manager/memory_manager.h"

namespace NEO {

GraphicsAllocation *AUBFixture::createHostPtrAllocationFromSvmPtr(void *svmPtr, size_t size) {
    GraphicsAllocation *allocation = csr->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, size}, svmPtr);
    csr->makeResidentHostPtrAllocation(allocation);
    csr->getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), TEMPORARY_ALLOCATION);
    allocation->setAllocationType(GraphicsAllocation::AllocationType::BUFFER);
    allocation->setMemObjectsAllocationWithWritableFlags(true);
    return allocation;
}

} // namespace NEO
