/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"

#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"

namespace NEO {

GraphicsAllocation *AUBFixture::createHostPtrAllocationFromSvmPtr(void *svmPtr, size_t size) {
    GraphicsAllocation *allocation = csr->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, size}, svmPtr);
    csr->makeResidentHostPtrAllocation(allocation);
    csr->getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), TEMPORARY_ALLOCATION);
    allocation->setAllocationType(AllocationType::buffer);
    allocation->setMemObjectsAllocationWithWritableFlags(true);
    return allocation;
}

GraphicsAllocation *AUBFixture::createResidentAllocationAndStoreItInCsr(const void *address, size_t size) {
    GraphicsAllocation *allocation = csr->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, size}, address);
    csr->makeResidentHostPtrAllocation(allocation);
    csr->getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), TEMPORARY_ALLOCATION);
    return allocation;
}

} // namespace NEO
