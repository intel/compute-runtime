/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper_with_capture.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "gtest/gtest.h"

using namespace NEO;

class DrmAllocationMemAdviseTest : public ::testing::Test {
  public:
    void SetUp() override {
        executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        drm = std::make_unique<DrmMockWithCaptureHelper>(*executionEnvironment->rootDeviceEnvironments[0]);
        drm->getMockIoctlHelper()->preferredLocationRegionAvailable = true;
    }

    uint32_t getVmAdviseCallCount() {
        return drm->getMockIoctlHelper()->vmBoAdviseCalled + drm->getMockIoctlHelper()->vmBoAdviseForChunkingCalled;
    }

    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    std::unique_ptr<DrmMockWithCaptureHelper> drm;
};

TEST_F(DrmAllocationMemAdviseTest, givenDrmAllocationWhenSetMemAdviseFailsThenDontUpdateMemAdviceFlags) {
    drm->getMockIoctlHelper()->vmBoAdviseResult = false;

    MockBufferObject bo(0u, drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    MemAdviseFlags memAdviseFlags{};
    memAdviseFlags.nonAtomic = 1;

    allocation.setMemAdvise(drm.get(), memAdviseFlags);

    EXPECT_EQ(1u, getVmAdviseCallCount());
    EXPECT_NE(memAdviseFlags.allFlags, allocation.getMemAdviseFlags().allFlags);
}

TEST_F(DrmAllocationMemAdviseTest, givenDrmAllocationWhenSetMemAdviseForChunkingFailsThenDontUpdateMemAdviceFlags) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableBOChunking.set(1);
    debugManager.flags.EnableBOChunkingPreferredLocationHint.set(true);

    drm->getMockIoctlHelper()->vmBoAdviseResult = false;

    MockBufferObject bo(0u, drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    MemAdviseFlags memAdviseFlags{};
    memAdviseFlags.nonAtomic = 1;
    allocation.storageInfo.isChunked = 1;

    allocation.setMemAdvise(drm.get(), memAdviseFlags);

    EXPECT_EQ(1u, getVmAdviseCallCount());
    EXPECT_NE(memAdviseFlags.allFlags, allocation.getMemAdviseFlags().allFlags);
}

TEST_F(DrmAllocationMemAdviseTest, givenDrmAllocationWhenSetMemAdviseWithNonAtomicIsCalledThenUpdateTheCorrespondingVmAdviceForBufferObject) {
    MockBufferObject bo(0u, drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    MemAdviseFlags memAdviseFlags{};

    for (auto nonAtomic : {true, false}) {
        memAdviseFlags.nonAtomic = nonAtomic;

        EXPECT_TRUE(allocation.setMemAdvise(drm.get(), memAdviseFlags));
        EXPECT_EQ(memAdviseFlags.allFlags, allocation.getMemAdviseFlags().allFlags);
    }
    EXPECT_EQ(2u, getVmAdviseCallCount());
}

TEST_F(DrmAllocationMemAdviseTest, givenDrmAllocationWhenSetMemAdviseWithDevicePreferredLocationIsCalledThenUpdateTheCorrespondingVmAdviceForBufferObject) {
    MockBufferObject bo(0u, drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;
    allocation.storageInfo.memoryBanks = 0x1;
    allocation.setNumHandles(1);

    MemAdviseFlags memAdviseFlags{};

    for (auto devicePreferredLocation : {true, false}) {
        memAdviseFlags.devicePreferredLocation = devicePreferredLocation;

        EXPECT_TRUE(allocation.setMemAdvise(drm.get(), memAdviseFlags));
        EXPECT_EQ(memAdviseFlags.allFlags, allocation.getMemAdviseFlags().allFlags);
    }
    EXPECT_EQ(2u, getVmAdviseCallCount());
}

TEST_F(DrmAllocationMemAdviseTest, givenDrmAllocationWhenSetMemAdviseWithSystemPreferredLocationIsCalledThenUpdateTheCorrespondingVmAdviceForBufferObject) {
    MockBufferObject bo(0u, drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;
    allocation.storageInfo.memoryBanks = 0x1;
    allocation.setNumHandles(1);

    MemAdviseFlags memAdviseFlags{};

    for (auto systemPreferredLocation : {true, false}) {
        memAdviseFlags.systemPreferredLocation = systemPreferredLocation;

        EXPECT_TRUE(allocation.setMemAdvise(drm.get(), memAdviseFlags));
        EXPECT_EQ(memAdviseFlags.allFlags, allocation.getMemAdviseFlags().allFlags);
    }
    EXPECT_EQ(2u, getVmAdviseCallCount());
}

TEST_F(DrmAllocationMemAdviseTest, givenDrmAllocationWhenSetMemAdviseWithChunkingPreferredLocationIsCalledThenUpdateTheCorrespondingVmAdviceForBufferChunks) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableBOChunking.set(1);
    debugManager.flags.EnableBOChunkingPreferredLocationHint.set(true);

    std::vector<MemoryRegion> memRegions{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 2}, MemoryConstants::chunkThreshold * 4, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions, *drm));

    MockBufferObject bo(0u, drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;
    allocation.storageInfo.memoryBanks = 0x1;
    allocation.setNumHandles(1);
    allocation.storageInfo.isChunked = 1;
    allocation.storageInfo.numOfChunks = 4;

    MemAdviseFlags memAdviseFlags{};

    for (auto devicePreferredLocation : {true, false}) {
        memAdviseFlags.devicePreferredLocation = devicePreferredLocation;

        EXPECT_TRUE(allocation.setMemAdvise(drm.get(), memAdviseFlags));
        EXPECT_EQ(memAdviseFlags.allFlags, allocation.getMemAdviseFlags().allFlags);
    }
    EXPECT_EQ(allocation.storageInfo.numOfChunks * 2, getVmAdviseCallCount());
}

TEST_F(DrmAllocationMemAdviseTest, givenDrmAllocationWhenSetMemAdviseWithChunkingButWithoutEnableBOChunkingPreferredLocationHintCalledThenUpdateTheCorrespondingVmAdviceForBufferObject) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableBOChunking.set(1);
    debugManager.flags.EnableBOChunkingPreferredLocationHint.set(0);

    std::vector<MemoryRegion> memRegions{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 2}, MemoryConstants::chunkThreshold * 4, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions, *drm));

    MockBufferObject bo(0u, drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;
    allocation.storageInfo.memoryBanks = 0x1;
    allocation.setNumHandles(1);
    allocation.storageInfo.isChunked = 1;
    allocation.storageInfo.numOfChunks = 4;

    MemAdviseFlags memAdviseFlags{};

    for (auto devicePreferredLocation : {true, false}) {
        memAdviseFlags.devicePreferredLocation = devicePreferredLocation;

        EXPECT_TRUE(allocation.setMemAdvise(drm.get(), memAdviseFlags));
        EXPECT_EQ(memAdviseFlags.allFlags, allocation.getMemAdviseFlags().allFlags);
    }
    EXPECT_EQ(2u, getVmAdviseCallCount());
}

TEST_F(DrmAllocationMemAdviseTest,
       givenDrmAllocationWhenSetMemAdviseWithChunkingPreferredLocationIsCalledWithFailureThenReturnFalse) {
    drm->getMockIoctlHelper()->vmBoAdviseResult = false;
    DebugManagerStateRestore restore;
    debugManager.flags.EnableBOChunking.set(1);
    debugManager.flags.EnableBOChunkingPreferredLocationHint.set(true);

    std::vector<MemoryRegion> memRegions{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 2}, MemoryConstants::chunkThreshold * 4, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions, *drm));

    MockBufferObject bo(0u, drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;
    allocation.storageInfo.memoryBanks = 0x5;
    allocation.setNumHandles(1);
    allocation.storageInfo.isChunked = 1;
    allocation.storageInfo.numOfChunks = 4;

    MemAdviseFlags memAdviseFlags{};

    memAdviseFlags.devicePreferredLocation = true;

    EXPECT_FALSE(allocation.setMemAdvise(drm.get(), memAdviseFlags));

    EXPECT_EQ(allocation.storageInfo.numOfChunks, getVmAdviseCallCount());
}

TEST_F(DrmAllocationMemAdviseTest, givenDrmAllocationWhenSetAtomicAccessWithModeCalledThenIoctlCalled) {
    MockBufferObject bo(0u, drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;
    allocation.storageInfo.memoryBanks = 0x1;
    allocation.setNumHandles(1);

    size_t size = 16;
    AtomicAccessMode mode = AtomicAccessMode::none;
    EXPECT_TRUE(allocation.setAtomicAccess(drm.get(), size, mode));
    EXPECT_EQ(1u, getVmAdviseCallCount());

    mode = AtomicAccessMode::device;
    EXPECT_TRUE(allocation.setAtomicAccess(drm.get(), size, mode));
    EXPECT_EQ(2u, getVmAdviseCallCount());

    mode = AtomicAccessMode::system;
    EXPECT_TRUE(allocation.setAtomicAccess(drm.get(), size, mode));
    EXPECT_EQ(3u, getVmAdviseCallCount());

    mode = AtomicAccessMode::host;
    // No IOCTL call for Host mode
    EXPECT_TRUE(allocation.setAtomicAccess(drm.get(), size, mode));
    EXPECT_EQ(3u, getVmAdviseCallCount());
}

TEST_F(DrmAllocationMemAdviseTest, givenDrmAllocationWhenSetAtomicAccessWithNullBufferObjectThenIoctlNotCalled) {
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = nullptr;
    allocation.storageInfo.memoryBanks = 0x1;
    allocation.setNumHandles(1);

    size_t size = 16;
    AtomicAccessMode mode = AtomicAccessMode::none;
    EXPECT_TRUE(allocation.setAtomicAccess(drm.get(), size, mode));
    EXPECT_EQ(0u, getVmAdviseCallCount());
}

TEST_F(DrmAllocationMemAdviseTest, givenDrmAllocationWhenSetMemPrefetchSucceedsThenReturnTrue) {
    SubDeviceIdsVec subDeviceIds{0};
    MockBufferObject bo(0u, drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    EXPECT_TRUE(allocation.setMemPrefetch(drm.get(), subDeviceIds));
}

TEST_F(DrmAllocationMemAdviseTest, givenDrmAllocationWhenSetMemPrefetchFailsThenReturnFalse) {
    SubDeviceIdsVec subDeviceIds{0};
    MockBufferObject bo(0u, drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;
    allocation.setNumHandles(1);

    drm->getMockIoctlHelper()->vmPrefetchResult = false;
    EXPECT_FALSE(allocation.setMemPrefetch(drm.get(), subDeviceIds));
}

TEST_F(DrmAllocationMemAdviseTest,
       givenDrmAllocationWithChunkingAndsetMemPrefetchCalledSuccessIsReturned) {
    SubDeviceIdsVec subDeviceIds{0, 1};
    DebugManagerStateRestore restore;
    debugManager.flags.EnableBOChunking.set(1);
    debugManager.flags.EnableBOChunkingPrefetch.set(true);
    debugManager.flags.EnableBOChunkingPreferredLocationHint.set(true);

    std::vector<MemoryRegion> memRegions{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 2}, MemoryConstants::chunkThreshold * 4, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions, *drm));

    MockBufferObject bo(0u, drm.get(), 3, 0, 0, 1);
    bo.setChunked(true);
    bo.setSize(1024);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;
    allocation.storageInfo.memoryBanks = 0x5;
    allocation.setNumHandles(1);
    allocation.storageInfo.isChunked = 1;
    allocation.storageInfo.numOfChunks = 4;
    allocation.storageInfo.subDeviceBitfield = 0b0001;
    EXPECT_TRUE(allocation.setMemPrefetch(drm.get(), subDeviceIds));
}

TEST_F(DrmAllocationMemAdviseTest,
       givenDrmAllocationWithChunkingAndsetMemPrefetchWithIoctlFailureThenFailureReturned) {
    SubDeviceIdsVec subDeviceIds{0, 1};
    DebugManagerStateRestore restore;
    debugManager.flags.EnableBOChunking.set(1);
    debugManager.flags.EnableBOChunkingPrefetch.set(true);
    debugManager.flags.EnableBOChunkingPreferredLocationHint.set(true);

    std::vector<MemoryRegion> memRegions{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 2}, MemoryConstants::chunkThreshold * 4, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions, *drm));

    MockBufferObject bo(0u, drm.get(), 3, 0, 0, 1);
    bo.setChunked(true);
    bo.setSize(1024);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;
    allocation.storageInfo.memoryBanks = 0x5;
    allocation.setNumHandles(1);
    allocation.storageInfo.isChunked = 1;
    allocation.storageInfo.numOfChunks = 4;
    allocation.storageInfo.subDeviceBitfield = 0b0001;
    drm->getMockIoctlHelper()->vmPrefetchResult = false;
    EXPECT_FALSE(allocation.setMemPrefetch(drm.get(), subDeviceIds));
}
