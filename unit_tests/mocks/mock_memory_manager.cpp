/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/deferred_deleter.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/helpers/surface_formats.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include <cstring>

namespace OCLRT {

void MockMemoryManager::setDeferredDeleter(DeferredDeleter *deleter) {
    deferredDeleter.reset(deleter);
}

void MockMemoryManager::overrideAsyncDeleterFlag(bool newValue) {
    asyncDeleterEnabled = newValue;
    if (asyncDeleterEnabled && deferredDeleter == nullptr) {
        deferredDeleter = createDeferredDeleter();
    }
}
GraphicsAllocation *MockMemoryManager::allocateGraphicsMemoryForImage(ImageInfo &imgInfo, Gmm *gmm) {
    imgInfo.size *= redundancyRatio;
    auto *allocation = OsAgnosticMemoryManager::allocateGraphicsMemoryForImage(imgInfo, gmm);
    imgInfo.size /= redundancyRatio;
    if (redundancyRatio != 1) {
        memset((unsigned char *)allocation->getUnderlyingBuffer(), 0, imgInfo.size * redundancyRatio);
    }
    return allocation;
}

void MockMemoryManager::setCommandStreamReceiver(CommandStreamReceiver *csr) {
    this->csr = csr;
}

bool MockMemoryManager::isAllocationListEmpty() {
    return graphicsAllocations.peekIsEmpty();
}

GraphicsAllocation *MockMemoryManager::peekAllocationListHead() {
    return graphicsAllocations.peekHead();
}

GraphicsAllocation *MockMemoryManager::allocateGraphicsMemory64kb(size_t size, size_t alignment, bool forcePin, bool preferRenderCompressed) {
    allocation64kbPageCreated = true;
    preferRenderCompressedFlagPassed = preferRenderCompressed;

    auto allocation = OsAgnosticMemoryManager::allocateGraphicsMemory64kb(size, alignment, forcePin, preferRenderCompressed);
    if (allocation) {
        allocation->gmm = new Gmm(allocation->getUnderlyingBuffer(), size, false, preferRenderCompressed, true);
        allocation->gmm->isRenderCompressed = preferRenderCompressed;
    }
    return allocation;
}

GraphicsAllocation *MockMemoryManager::allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) {
    if (failInDevicePool) {
        status = AllocationStatus::RetryInNonDevicePool;
        return nullptr;
    }
    if (failInDevicePoolWithError) {
        status = AllocationStatus::Error;
        return nullptr;
    }

    auto allocation = OsAgnosticMemoryManager::allocateGraphicsMemoryInDevicePool(allocationData, status);
    if (allocation) {
        allocationInDevicePoolCreated = true;
    }
    return allocation;
}

GraphicsAllocation *MockMemoryManager::allocateGraphicsMemory(size_t size, size_t alignment, bool forcePin, bool uncacheable) {
    if (failInAllocateWithSizeAndAlignment) {
        return nullptr;
    }
    allocationCreated = true;
    return OsAgnosticMemoryManager::allocateGraphicsMemory(size, alignment, forcePin, uncacheable);
}

} // namespace OCLRT
