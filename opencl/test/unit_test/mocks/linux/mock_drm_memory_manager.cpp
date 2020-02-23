/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/linux/mock_drm_memory_manager.h"

#include "shared/source/os_interface/linux/allocator_helper.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "opencl/test/unit_test/mocks/mock_allocation_properties.h"
#include "opencl/test/unit_test/mocks/mock_host_ptr_manager.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"

#include <atomic>

namespace NEO {
off_t lseekReturn = 4096u;
std::atomic<int> lseekCalledCount(0);

TestedDrmMemoryManager::TestedDrmMemoryManager(ExecutionEnvironment &executionEnvironment) : MemoryManagerCreate(gemCloseWorkerMode::gemCloseWorkerInactive,
                                                                                                                 false,
                                                                                                                 false,
                                                                                                                 executionEnvironment) {
    this->lseekFunction = &lseekMock;
    this->closeFunction = &closeMock;
    lseekReturn = 4096;
    lseekCalledCount = 0;
    hostPtrManager.reset(new MockHostPtrManager);
};

TestedDrmMemoryManager::TestedDrmMemoryManager(bool enableLocalMemory,
                                               bool allowForcePin,
                                               bool validateHostPtrMemory,
                                               ExecutionEnvironment &executionEnvironment) : MemoryManagerCreate(false, enableLocalMemory,
                                                                                                                 gemCloseWorkerMode::gemCloseWorkerInactive,
                                                                                                                 allowForcePin,
                                                                                                                 validateHostPtrMemory,
                                                                                                                 executionEnvironment) {
    this->lseekFunction = &lseekMock;
    this->closeFunction = &closeMock;
    lseekReturn = 4096;
    lseekCalledCount = 0;
}

void TestedDrmMemoryManager::injectPinBB(BufferObject *newPinBB) {
    BufferObject *currentPinBB = pinBBs.at(newPinBB->peekRootDeviceIndex());
    pinBBs.at(newPinBB->peekRootDeviceIndex()) = nullptr;
    DrmMemoryManager::unreference(currentPinBB, true);
    pinBBs.at(newPinBB->peekRootDeviceIndex()) = newPinBB;
}

DrmGemCloseWorker *TestedDrmMemoryManager::getgemCloseWorker() { return this->gemCloseWorker.get(); }
void TestedDrmMemoryManager::forceLimitedRangeAllocator(uint64_t range) { getGfxPartition(0)->init(range, getSizeToReserve(), 0, 1); }
void TestedDrmMemoryManager::overrideGfxPartition(GfxPartition *newGfxPartition) { gfxPartitions[0].reset(newGfxPartition); }

DrmAllocation *TestedDrmMemoryManager::allocate32BitGraphicsMemory(size_t size, const void *ptr, GraphicsAllocation::AllocationType allocationType) {
    bool allocateMemory = ptr == nullptr;
    AllocationData allocationData;
    MockAllocationProperties properties(allocateMemory, size, allocationType);
    getAllocationData(allocationData, properties, ptr, createStorageInfoFromProperties(properties));
    return allocate32BitGraphicsMemoryImpl(allocationData);
}
TestedDrmMemoryManager::~TestedDrmMemoryManager() {
    DrmMemoryManager::commonCleanup();
}
}; // namespace NEO
