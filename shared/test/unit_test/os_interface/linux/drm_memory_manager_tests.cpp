/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_stream/tag_allocation_layout.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_bind.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/linux/mock_drm_command_stream_receiver.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper.h"
#include "shared/test/common/mocks/linux/mock_os_context_linux.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gfx_partition.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_client_context_base.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/mocks/mock_host_ptr_manager.h"
#include "shared/test/common/mocks/mock_product_helper.h"
// #include "shared/test/common/mocks/mock_release_helper.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_fixture.h"
#include "shared/test/common/os_interface/linux/drm_mock_cache_info.h"
#include "shared/test/common/os_interface/linux/drm_mock_memory_info.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "clos_matchers.h"
#include "gtest/gtest.h"

#include <array>
#include <fcntl.h>
#include <memory>
#include <vector>

namespace NEO {
namespace SysCalls {
extern bool failMmap;
} // namespace SysCalls
} // namespace NEO

namespace {
using DrmMemoryManagerTest = Test<DrmMemoryManagerFixture>;
using DrmMemoryManagerWithLocalMemoryTest = Test<DrmMemoryManagerWithLocalMemoryFixture>;
using DrmMemoryManagerWithExplicitExpectationsTest = Test<DrmMemoryManagerFixtureWithoutQuietIoctlExpectation>;
using DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest = Test<DrmMemoryManagerFixtureWithLocalMemoryAndWithoutQuietIoctlExpectation>;
using DrmMemoryManagerUSMHostAllocationTests = Test<DrmMemoryManagerFixture>;

AllocationProperties createAllocationProperties(uint32_t rootDeviceIndex, size_t size, bool forcePin) {
    MockAllocationProperties properties(rootDeviceIndex, size);
    properties.alignment = MemoryConstants::preferredAlignment;
    properties.flags.forcePin = forcePin;
    return properties;
}
} // namespace

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenCallingHasPageFaultsEnabledThenReturnCorrectValue) {
    DebugManagerStateRestore dbgState;

    EXPECT_FALSE(memoryManager->hasPageFaultsEnabled(*device));

    for (auto debugFlag : {-1, 0, 1}) {
        debugManager.flags.EnableRecoverablePageFaults.set(debugFlag);
        if (debugFlag == 1) {
            EXPECT_TRUE(memoryManager->hasPageFaultsEnabled(*device));
        } else {
            EXPECT_FALSE(memoryManager->hasPageFaultsEnabled(*device));
        }
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenEnableDirectSubmissionWhenCreateDrmMemoryManagerThenGemCloseWorkerInactive) {
    DebugManagerStateRestore dbgState;
    debugManager.flags.EnableDirectSubmission.set(1);

    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);

    EXPECT_EQ(memoryManager.peekGemCloseWorker(), nullptr);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDebugFlagSetWhenUsingMmapFunctionsThenPrintContent) {
    DebugManagerStateRestore dbgState;
    debugManager.flags.PrintMmapAndMunMapCalls.set(1);

    MockDrmMemoryManager memoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, *executionEnvironment);

    StreamCapture capture;
    capture.captureStdout();
    size_t len = 2;
    off_t offset = 6;
    void *ptr = reinterpret_cast<void *>(0x1234);
    void *retPtr = memoryManager.mmapFunction(ptr, len, 3, 4, 5, offset);

    std::string output = capture.getCapturedStdout();
    char expected1[256] = {};

    sprintf(expected1, "mmap(%p, %zu, %d, %d, %d, %ld) = %p, errno: %d \n", ptr, len, 3, 4, 5, offset, retPtr, errno);

    EXPECT_NE(std::string::npos, output.find(expected1));

    capture.captureStdout();
    int retVal = memoryManager.munmapFunction(retPtr, len);

    output = capture.getCapturedStdout();
    char expected2[256] = {};

    sprintf(expected2, "munmap(%p, %zu) = %d, errno: %d \n", retPtr, len, retVal, errno);

    EXPECT_NE(std::string::npos, output.find(expected2));
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDebugVariableWhenCreatingDrmMemoryManagerThenSetSupportForMultiStorageResources) {
    DebugManagerStateRestore dbgState;

    EXPECT_TRUE(memoryManager->supportsMultiStorageResources);

    {
        debugManager.flags.EnableMultiStorageResources.set(0);
        TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
        EXPECT_FALSE(memoryManager.supportsMultiStorageResources);
    }

    {
        debugManager.flags.EnableMultiStorageResources.set(1);
        TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
        EXPECT_TRUE(memoryManager.supportsMultiStorageResources);
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenCheckForKmdMigrationThenCorrectValueIsReturned) {
    DebugManagerStateRestore restorer;
    {
        debugManager.flags.UseKmdMigration.set(1);

        auto retVal = memoryManager->isKmdMigrationAvailable(rootDeviceIndex);

        EXPECT_TRUE(retVal);
    }
    {
        debugManager.flags.UseKmdMigration.set(0);

        auto retVal = memoryManager->isKmdMigrationAvailable(rootDeviceIndex);

        EXPECT_FALSE(retVal);
    }

    this->dontTestIoctlInTearDown = true;
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenGraphicsAllocationWhenAddAndRemoveAllocationToHostPtrManagerThenfragmentHasCorrectValues) {
    void *cpuPtr = (void *)0x30000;
    size_t size = 0x1000;

    const uint32_t rootDeviceIndex = 0u;
    DrmAllocation gfxAllocation(rootDeviceIndex, 1u /*num gmms*/, AllocationType::unknown, nullptr, cpuPtr, size, static_cast<osHandle>(1u), MemoryPool::memoryNull);
    memoryManager->addAllocationToHostPtrManager(&gfxAllocation);
    auto fragment = memoryManager->getHostPtrManager()->getFragment({gfxAllocation.getUnderlyingBuffer(), rootDeviceIndex});
    EXPECT_NE(fragment, nullptr);
    EXPECT_TRUE(fragment->driverAllocation);
    EXPECT_EQ(fragment->refCount, 1);
    EXPECT_EQ(fragment->refCount, 1);
    EXPECT_EQ(fragment->fragmentCpuPointer, cpuPtr);
    EXPECT_EQ(fragment->fragmentSize, size);
    EXPECT_NE(fragment->osInternalStorage, nullptr);
    EXPECT_EQ(static_cast<OsHandleLinux *>(fragment->osInternalStorage)->bo, gfxAllocation.getBO());
    EXPECT_NE(fragment->residency, nullptr);

    FragmentStorage fragmentStorage = {};
    fragmentStorage.fragmentCpuPointer = cpuPtr;
    memoryManager->getHostPtrManager()->storeFragment(rootDeviceIndex, fragmentStorage);
    fragment = memoryManager->getHostPtrManager()->getFragment({gfxAllocation.getUnderlyingBuffer(), rootDeviceIndex});
    EXPECT_EQ(fragment->refCount, 2);

    fragment->driverAllocation = false;
    memoryManager->removeAllocationFromHostPtrManager(&gfxAllocation);
    fragment = memoryManager->getHostPtrManager()->getFragment({gfxAllocation.getUnderlyingBuffer(), rootDeviceIndex});
    EXPECT_EQ(fragment->refCount, 2);
    fragment->driverAllocation = true;

    memoryManager->removeAllocationFromHostPtrManager(&gfxAllocation);
    fragment = memoryManager->getHostPtrManager()->getFragment({gfxAllocation.getUnderlyingBuffer(), rootDeviceIndex});
    EXPECT_EQ(fragment->refCount, 1);

    memoryManager->removeAllocationFromHostPtrManager(&gfxAllocation);
    fragment = memoryManager->getHostPtrManager()->getFragment({gfxAllocation.getUnderlyingBuffer(), rootDeviceIndex});
    EXPECT_EQ(fragment, nullptr);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenAllocatePhysicalDeviceMemoryThenSuccessReturnedAndNoVirtualMemoryAssigned) {
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemCreate = 1;
    mock->ioctlExpected.gemClose = 1;

    allocationData.size = MemoryConstants::pageSize;
    allocationData.flags.shareable = true;
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    auto allocation = memoryManager->allocatePhysicalDeviceMemory(allocationData, status);
    EXPECT_EQ(status, MemoryManager::AllocationStatus::Success);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0u, allocation->getGpuAddress());
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenAllocatePhysicalHostMemoryWithoutMemoryInfoThenNullptrIsReturned) {
    allocationData.size = MemoryConstants::pageSize;
    allocationData.flags.shareable = true;
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    auto allocation = memoryManager->allocatePhysicalHostMemory(allocationData, status);
    EXPECT_EQ(status, MemoryManager::AllocationStatus::Error);
    EXPECT_EQ(nullptr, allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenAllocatePhysicalHostMemoryAndMmapOffsetFailsThenNullptrIsReturned) {
    mock->ioctlExpected.gemCreateExt = 1;
    mock->ioctlExpected.gemMmapOffset = 1;
    mock->ioctlExpected.gemClose = 1;

    mock->failOnMmapOffset = true;

    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    this->mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    this->mock->memoryInfoQueried = true;

    allocationData.size = MemoryConstants::pageSize;
    allocationData.flags.shareable = true;
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    auto allocation = memoryManager->allocatePhysicalHostMemory(allocationData, status);
    EXPECT_EQ(status, MemoryManager::AllocationStatus::Error);
    EXPECT_EQ(nullptr, allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenAllocatePhysicalHostMemoryThenSuccessReturnedAndNoVirtualMemoryAssigned) {
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemCreateExt = 1;
    mock->ioctlExpected.gemMmapOffset = 1;
    mock->ioctlExpected.gemClose = 1;

    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    this->mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    this->mock->memoryInfoQueried = true;

    allocationData.size = MemoryConstants::pageSize;
    allocationData.flags.shareable = true;
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    auto allocation = memoryManager->allocatePhysicalHostMemory(allocationData, status);
    EXPECT_EQ(status, MemoryManager::AllocationStatus::Success);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0u, allocation->getGpuAddress());
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenAllocatePhysicalHostMemoryThenSuccessReturnedAndCacheableFlagIsOverriden) {
    mock->ioctlExpected.gemWait = 49;
    mock->ioctlExpected.gemCreateExt = 49;
    mock->ioctlExpected.gemMmapOffset = 49;
    mock->ioctlExpected.gemClose = 49;

    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    this->mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    this->mock->memoryInfoQueried = true;

    allocationData.size = MemoryConstants::pageSize;
    allocationData.flags.shareable = true;
    auto gmmHelper = memoryManager->getGmmHelper(0);
    auto &productHelper = gmmHelper->getRootDeviceEnvironment().getHelper<ProductHelper>();

    for (uint32_t i = 0; i < static_cast<uint32_t>(AllocationType::count); i++) {
        allocationData.type = static_cast<AllocationType>(i);
        MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
        auto allocation = memoryManager->allocatePhysicalHostMemory(allocationData, status);
        EXPECT_EQ(status, MemoryManager::AllocationStatus::Success);
        EXPECT_NE(nullptr, allocation);
        EXPECT_EQ(0u, allocation->getGpuAddress());

        if (productHelper.overrideAllocationCpuCacheable(allocationData)) {
            EXPECT_TRUE(allocation->getDefaultGmm()->resourceParams.Flags.Info.Cacheable);
        } else {
            auto gmmResourceUsage = CacheSettingsHelper::getGmmUsageType(allocationData.type, allocationData.flags.uncacheable, productHelper, gmmHelper->getHardwareInfo());
            auto preferNoCpuAccess = CacheSettingsHelper::preferNoCpuAccess(gmmResourceUsage, gmmHelper->getRootDeviceEnvironment());
            bool cacheable = !preferNoCpuAccess && !CacheSettingsHelper::isUncachedType(gmmResourceUsage);
            EXPECT_EQ(cacheable, allocation->getDefaultGmm()->resourceParams.Flags.Info.Cacheable);
        }

        memoryManager->freeGraphicsMemory(allocation);
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenCallingCheckUnexpectedGpuPagedfaultThenAllEnginesWereChecked) {
    mock->ioctlExpected.total = -1; // don't care
    memoryManager->checkUnexpectedGpuPageFault();
    size_t allEnginesSize = 0u;
    for (auto &engineContainer : memoryManager->allRegisteredEngines) {
        allEnginesSize += engineContainer.size();
    }
    ASSERT_NE(0u, allEnginesSize);
    EXPECT_EQ(allEnginesSize, mock->checkResetStatusCalled);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerWhenGpuAddressReservationIsAttemptedAtIndex1ThenAddressFromGfxPartitionIsUsed) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(1);
    uint32_t rootDeviceIndexReserved = 0;
    auto gmmHelper = memoryManager->getGmmHelper(1);
    auto addressRange = memoryManager->reserveGpuAddressOnHeap(0ull, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved, NEO::HeapIndex::heapStandard, MemoryConstants::pageSize64k);

    EXPECT_EQ(rootDeviceIndexReserved, 1u);
    EXPECT_LE(memoryManager->getGfxPartition(1)->getHeapBase(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memoryManager->getGfxPartition(1)->getHeapLimit(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    memoryManager->freeGpuAddress(addressRange, 1);

    addressRange = memoryManager->reserveGpuAddress(0ull, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved);

    EXPECT_EQ(rootDeviceIndexReserved, 1u);
    EXPECT_LE(memoryManager->getGfxPartition(1)->getHeapBase(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memoryManager->getGfxPartition(1)->getHeapLimit(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    memoryManager->freeGpuAddress(addressRange, 1);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerWhenGpuAddressIsReservedAndFreedThenAddressFromGfxPartitionIsUsed) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    uint32_t rootDeviceIndexReserved = 1;
    auto gmmHelper = memoryManager->getGmmHelper(0);
    auto addressRange = memoryManager->reserveGpuAddressOnHeap(0ull, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved, NEO::HeapIndex::heapStandard, MemoryConstants::pageSize64k);

    EXPECT_LE(memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    memoryManager->freeGpuAddress(addressRange, 0);

    addressRange = memoryManager->reserveGpuAddress(0ull, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved);

    EXPECT_LE(memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    memoryManager->freeGpuAddress(addressRange, 0);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDebugVariableToDisableAddressAlignmentAndCallToSelectAlignmentAndHeapWithPow2MemoryThenAlignmentIs2Mb) {
    if (!memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapExtended)) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restorer;
    debugManager.flags.UseHighAlignmentForHeapExtended.set(0);

    auto size = 8 * MemoryConstants::gigaByte;

    HeapIndex heap = HeapIndex::heapStandard;
    auto alignment = memoryManager->selectAlignmentAndHeap(size, &heap);
    EXPECT_EQ(heap, HeapIndex::heapExtended);
    EXPECT_EQ(MemoryConstants::pageSize2M, alignment);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenCalltoSelectAlignmentAndHeapWithPow2MemoryThenAlignmentIsPreviousPow2) {
    if (!memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapExtended)) {
        GTEST_SKIP();
    }

    auto size = Math::nextPowerOfTwo(8 * MemoryConstants::gigaByte);

    HeapIndex heap = HeapIndex::heapStandard;
    auto alignment = memoryManager->selectAlignmentAndHeap(size, &heap);
    EXPECT_EQ(heap, HeapIndex::heapExtended);
    EXPECT_EQ(Math::prevPowerOfTwo(size), alignment);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenCalltoSelectAlignmentAndHeapWithNonPow2SizeThenAlignmentIs2MB) {
    if (!memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapExtended)) {
        GTEST_SKIP();
    }

    auto size = (8 * MemoryConstants::gigaByte) + 5;

    HeapIndex heap = HeapIndex::heapStandard;
    auto alignment = memoryManager->selectAlignmentAndHeap(size, &heap);
    EXPECT_EQ(heap, HeapIndex::heapExtended);
    EXPECT_EQ(MemoryConstants::pageSize2M, alignment);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDebugVariableToDisableAddressAlignmentWhenSelectAlignmentAndHeapNewCustomAlignmentReturned) {
    if (!memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapExtended)) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restorer;
    debugManager.flags.UseHighAlignmentForHeapExtended.set(0);

    auto size = 16 * MemoryConstants::megaByte;

    HeapIndex heap = HeapIndex::heapStandard;
    auto alignment = memoryManager->selectAlignmentAndHeap(size, &heap);
    EXPECT_EQ(heap, HeapIndex::heapExtended);
    EXPECT_NE(MemoryConstants::pageSize64k, alignment);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenSelectAlignmentAndHeapThen64KbAlignmentReturned) {
    if (!memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapExtended)) {
        GTEST_SKIP();
    }

    auto size = 16 * MemoryConstants::megaByte;

    HeapIndex heap = HeapIndex::heapStandard;
    auto alignment = memoryManager->selectAlignmentAndHeap(size, &heap);
    EXPECT_EQ(heap, HeapIndex::heapExtended);
    EXPECT_NE(MemoryConstants::pageSize64k, alignment);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerWhenGpuAddressReservationIsAttemptedWithTheQuieriedHeapThenSuccessReturned) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    uint32_t rootDeviceIndexReserved = 1;
    HeapIndex heap = HeapIndex::heapStandard;
    auto alignment = memoryManager->selectAlignmentAndHeap(MemoryConstants::pageSize, &heap);
    auto gfxPartition = memoryManager->getGfxPartition(0);
    if (gfxPartition->getHeapLimit(HeapIndex::heapExtended) > 0) {
        EXPECT_EQ(heap, HeapIndex::heapExtended);
    } else {
        EXPECT_EQ(heap, HeapIndex::heapStandard64KB);
    }
    EXPECT_EQ(MemoryConstants::pageSize64k, alignment);
    auto addressRange = memoryManager->reserveGpuAddressOnHeap(0ull, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved, heap, alignment);
    EXPECT_NE(static_cast<int>(addressRange.address), 0);
    EXPECT_NE(static_cast<int>(addressRange.size), 0);
    memoryManager->freeGpuAddress(addressRange, 0);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerWhenGpuAddressReservationIsAttemptedWithSizeEqualToHalfOfHeapLimitThenSuccessReturned) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    auto gfxPartition = memoryManager->getGfxPartition(0);

    if (!gfxPartition->getHeapLimit(HeapIndex::heapExtended)) {
        GTEST_SKIP();
    }

    gfxPartition->heapInitWithAllocationAlignment(HeapIndex::heapExtended, 0x0, 0x800000000000, MemoryConstants::pageSize);

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    uint32_t rootDeviceIndexReserved = 1;

    HeapIndex heap = HeapIndex::heapStandard;
    size_t size = 64 * MemoryConstants::teraByte;
    auto alignment = memoryManager->selectAlignmentAndHeap(size, &heap);
    auto addressRange = memoryManager->reserveGpuAddressOnHeap(0ull, size, rootDeviceIndices, &rootDeviceIndexReserved, heap, alignment);
    EXPECT_EQ(heap, HeapIndex::heapExtended);
    EXPECT_EQ(size, alignment);
    EXPECT_NE(addressRange.address, 0ull);
    EXPECT_NE(addressRange.size, 0ull);

    memoryManager->freeGpuAddress(addressRange, 0);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerWhenGpuAddressReservationIsAttemptedWithDifferentSizesThenCorrectResultsReturned) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    auto gfxPartition = memoryManager->getGfxPartition(0);

    if (!gfxPartition->getHeapLimit(HeapIndex::heapExtended)) {
        GTEST_SKIP();
    }

    gfxPartition->heapInitWithAllocationAlignment(HeapIndex::heapExtended, 0x0, 0x800200400000, MemoryConstants::pageSize);

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    uint32_t rootDeviceIndexReserved = 1;

    HeapIndex heap = HeapIndex::heapStandard;
    size_t size = 128 * MemoryConstants::teraByte;
    auto alignment = memoryManager->selectAlignmentAndHeap(size, &heap);
    auto addressRange = memoryManager->reserveGpuAddressOnHeap(0ull, size, rootDeviceIndices, &rootDeviceIndexReserved, heap, alignment);
    EXPECT_EQ(heap, HeapIndex::heapExtended);
    EXPECT_EQ(size, alignment);
    EXPECT_NE(addressRange.address, 0ull);
    EXPECT_NE(addressRange.size, 0ull);

    heap = HeapIndex::heapStandard;
    size = 8 * MemoryConstants::gigaByte;
    alignment = memoryManager->selectAlignmentAndHeap(size, &heap);
    auto addressRange2 = memoryManager->reserveGpuAddressOnHeap(0ull, size, rootDeviceIndices, &rootDeviceIndexReserved, heap, alignment);
    EXPECT_EQ(heap, HeapIndex::heapExtended);
    EXPECT_EQ(size, alignment);
    EXPECT_EQ(addressRange2.address, 0ull);

    memoryManager->freeGpuAddress(addressRange, 0);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerWhenGpuAddressReservationIsAttemptedWithAnInvalidRequiredPtrThenDifferentRangeReturned) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    uint32_t rootDeviceIndexReserved = 1;
    auto addressRange = memoryManager->reserveGpuAddressOnHeap(0x1234, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved, NEO::HeapIndex::heapStandard, MemoryConstants::pageSize64k);
    EXPECT_NE(static_cast<int>(addressRange.address), 0x1234);
    EXPECT_NE(static_cast<int>(addressRange.size), 0);
    memoryManager->freeGpuAddress(addressRange, 0);
    addressRange = memoryManager->reserveGpuAddress(0x1234, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved);
    EXPECT_NE(static_cast<int>(addressRange.address), 0x1234);
    EXPECT_NE(static_cast<int>(addressRange.size), 0);
    memoryManager->freeGpuAddress(addressRange, 0);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerWhenGpuAddressReservationIsAttemptedWhichFailsThenNullRangeReturned) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    uint32_t rootDeviceIndexReserved = 1;
    // emulate GPU address space exhaust
    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);
    memoryManager->getGfxPartition(0)->heapInit(HeapIndex::heapStandard, 0x0, 0x10000);
    size_t invalidSize = (size_t)memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapStandard) + MemoryConstants::pageSize;
    auto addressRange = memoryManager->reserveGpuAddressOnHeap(0ull, invalidSize, rootDeviceIndices, &rootDeviceIndexReserved, NEO::HeapIndex::heapStandard, MemoryConstants::pageSize64k);
    EXPECT_EQ(static_cast<int>(addressRange.address), 0);
    addressRange = memoryManager->reserveGpuAddress(0ull, invalidSize, rootDeviceIndices, &rootDeviceIndexReserved);
    EXPECT_EQ(static_cast<int>(addressRange.address), 0);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerWhenHeapAndAlignmentRequestedWithoutAllExtendedHeapsForRootDevicesThenHeapStandardReturned) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    // emulate GPU address space exhaust
    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);
    memoryManager->getGfxPartition(0)->heapInit(HeapIndex::heapExtended, 0x11000, 0x10000);
    memoryManager->getGfxPartition(1)->heapInit(HeapIndex::heapStandard, 0, 0x10000);
    memoryManager->getGfxPartition(1)->heapInit(HeapIndex::heapExtended, 0, 0);
    auto size = MemoryConstants::pageSize64k;

    HeapIndex heap = HeapIndex::heapStandard;
    auto alignment = memoryManager->selectAlignmentAndHeap(size, &heap);
    EXPECT_EQ(heap, HeapIndex::heapStandard64KB);
    EXPECT_EQ(MemoryConstants::pageSize64k, alignment);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenSmallSizeAndGpuAddressSetWhenGraphicsMemoryIsAllocatedThenAllocationWithSpecifiedGpuAddressInSystemMemoryIsCreated) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    auto osContext = device->getDefaultEngine().osContext;

    MockAllocationProperties properties = {rootDeviceIndex, MemoryConstants::pageSize};
    properties.gpuAddress = 0x2000;
    properties.osContext = osContext;

    mock->reset();
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.execbuffer2 = 0; // pinBB not called

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    EXPECT_EQ(MemoryPool::system4KBPages, allocation->getMemoryPool());
    EXPECT_EQ(0x2000u, allocation->getGpuAddress());

    mock->testIoctls();

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenMemoryManagerWhenAllocateGraphicsMemoryIsCalledThenAllocationSizeIsRegistered) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(true, false, false, *executionEnvironment);
    auto osContext = device->getDefaultEngine().osContext;

    const auto allocationSize = MemoryConstants::pageSize;
    auto allocationProperties = MockAllocationProperties{device->getRootDeviceIndex(), allocationSize};
    allocationProperties.osContext = osContext;
    allocationProperties.allocationType = AllocationType::buffer;
    EXPECT_EQ(0u, memoryManager->getUsedLocalMemorySize(device->getRootDeviceIndex()));
    EXPECT_EQ(0u, memoryManager->getUsedSystemMemorySize());
    auto localAllocation = memoryManager->allocateGraphicsMemoryWithProperties(allocationProperties);
    EXPECT_NE(nullptr, localAllocation);
    EXPECT_EQ(localAllocation->getUnderlyingBufferSize(), memoryManager->getUsedLocalMemorySize(device->getRootDeviceIndex()));
    EXPECT_EQ(0u, memoryManager->getUsedSystemMemorySize());

    allocationProperties.allocationType = AllocationType::bufferHostMemory;
    auto systemAllocation = memoryManager->allocateGraphicsMemoryWithProperties(allocationProperties);
    EXPECT_NE(nullptr, systemAllocation);
    EXPECT_EQ(localAllocation->getUnderlyingBufferSize(), memoryManager->getUsedLocalMemorySize(device->getRootDeviceIndex()));
    EXPECT_EQ(systemAllocation->getUnderlyingBufferSize(), memoryManager->getUsedSystemMemorySize());

    memoryManager->freeGraphicsMemory(localAllocation);
    EXPECT_EQ(0u, memoryManager->getUsedLocalMemorySize(device->getRootDeviceIndex()));
    EXPECT_EQ(systemAllocation->getUnderlyingBufferSize(), memoryManager->getUsedSystemMemorySize());

    memoryManager->freeGraphicsMemory(systemAllocation);
    EXPECT_EQ(0u, memoryManager->getUsedLocalMemorySize(device->getRootDeviceIndex()));
    EXPECT_EQ(0u, memoryManager->getUsedSystemMemorySize());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenInjectedFailuresWhenGraphicsMemoryWithGpuVaIsAllocatedThenNullptrIsReturned) {
    mock->ioctlExpected.total = -1; // don't care

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    auto osContext = device->getDefaultEngine().osContext;

    MockAllocationProperties properties = {rootDeviceIndex, MemoryConstants::pageSize};
    properties.gpuAddress = 0x2000;
    properties.osContext = osContext;

    InjectedFunction method = [&](size_t failureIndex) {
        auto ptr = memoryManager->allocateGraphicsMemoryWithProperties(properties);

        if (MemoryManagement::nonfailingAllocation != failureIndex) {
            EXPECT_EQ(nullptr, ptr);
        } else {
            EXPECT_NE(nullptr, ptr);
            memoryManager->freeGraphicsMemory(ptr);
        }
    };
    injectFailures(method);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenSizeExceedingThresholdAndGpuAddressSetWhenGraphicsMemoryIsAllocatedThenAllocationWithSpecifiedGpuAddressInSystemMemoryIsCreated) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    auto osContext = device->getDefaultEngine().osContext;

    MockAllocationProperties properties = {rootDeviceIndex, memoryManager->pinThreshold + MemoryConstants::pageSize};
    properties.gpuAddress = 0x2000;
    properties.osContext = osContext;

    mock->reset();
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.execbuffer2 = 1; // pinBB called

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    EXPECT_EQ(MemoryPool::system4KBPages, allocation->getMemoryPool());
    EXPECT_EQ(0x2000u, allocation->getGpuAddress());

    mock->testIoctls();

    memoryManager->freeGraphicsMemory(allocation);

    memoryManager->injectPinBB(nullptr, rootDeviceIndex); // pinBB not available

    mock->reset();
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.execbuffer2 = 0; // pinBB not called

    properties.gpuAddress = 0x5000;

    allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    EXPECT_EQ(MemoryPool::system4KBPages, allocation->getMemoryPool());
    EXPECT_EQ(0x5000u, allocation->getGpuAddress());

    mock->testIoctls();

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDisabledForcePinAndSizeExceedingThresholdAndGpuAddressSetWhenGraphicsMemoryIsAllocatedThenBufferIsNotPinned) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);
    auto osContext = device->getDefaultEngine().osContext;

    MockAllocationProperties properties = {rootDeviceIndex, memoryManager->pinThreshold + MemoryConstants::pageSize};
    properties.gpuAddress = 0x2000;
    properties.osContext = osContext;

    mock->reset();
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.execbuffer2 = 0; // pinBB not called

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    EXPECT_EQ(MemoryPool::system4KBPages, allocation->getMemoryPool());
    EXPECT_EQ(0x2000u, allocation->getGpuAddress());

    mock->testIoctls();

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenforcePinAllowedWhenMemoryManagerIsCreatedThenPinBbIsCreated) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    EXPECT_NE(nullptr, memoryManager->pinBBs[device->getRootDeviceIndex()]);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDefaultDrmMemoryManagerWhenItIsCreatedThenItIsInitialized) {
    EXPECT_TRUE(memoryManager->isInitialized());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDefaultDrmMemoryManagerWhenItIsCreatedAndGfxPartitionInitIsFailedThenItIsNotInitialized) {
    EXPECT_TRUE(memoryManager->isInitialized());

    auto failedInitGfxPartition = std::make_unique<FailedInitGfxPartition>();
    memoryManager->gfxPartitions[0].reset(failedInitGfxPartition.release());
    memoryManager->initialize(GemCloseWorkerMode::gemCloseWorkerInactive);
    EXPECT_FALSE(memoryManager->isInitialized());

    auto mockGfxPartitionBasic = std::make_unique<MockGfxPartitionBasic>();
    memoryManager->overrideGfxPartition(mockGfxPartitionBasic.release());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, WhenMemoryManagerIsCreatedThenPinBbIsCreated) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemClose = 1;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    EXPECT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenMemoryManagerIsCreatedWhenInvokingReleaseMemResourcesBasedOnGpuDeviceThenPinBbIsRemoved) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemClose = 1;

    auto drmMemoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    EXPECT_NE(nullptr, drmMemoryManager->pinBBs[rootDeviceIndex]);
    auto length = drmMemoryManager->pinBBs.size();
    drmMemoryManager->releaseDeviceSpecificMemResources(rootDeviceIndex);
    EXPECT_EQ(length, drmMemoryManager->pinBBs.size());
    EXPECT_EQ(nullptr, drmMemoryManager->pinBBs[rootDeviceIndex]);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenMemoryManagerIsCreatedWhenInvokingCreatMemResourcesBasedOnGpuDeviceThenPinBbIsCreated) {
    mock->ioctlExpected.gemUserptr = 2;
    mock->ioctlExpected.gemClose = 2;

    auto drmMemoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    auto rootDeviceBufferObjectOld = drmMemoryManager->pinBBs[rootDeviceIndex];
    EXPECT_NE(nullptr, rootDeviceBufferObjectOld);
    auto length = drmMemoryManager->pinBBs.size();
    auto memoryManagerTest = static_cast<MemoryManager *>(drmMemoryManager.get());
    drmMemoryManager->releaseDeviceSpecificMemResources(rootDeviceIndex);
    EXPECT_EQ(length, drmMemoryManager->pinBBs.size());
    EXPECT_EQ(nullptr, drmMemoryManager->pinBBs[rootDeviceIndex]);

    memoryManagerTest->createDeviceSpecificMemResources(rootDeviceIndex);
    EXPECT_EQ(length, drmMemoryManager->pinBBs.size());
    EXPECT_NE(nullptr, drmMemoryManager->pinBBs[rootDeviceIndex]);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenNotAllowedForcePinWhenMemoryManagerIsCreatedThenPinBBIsNotCreated) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    false,
                                                                                                    *executionEnvironment));
    EXPECT_EQ(nullptr, memoryManager->pinBBs[rootDeviceIndex]);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, WhenIoctlFailsThenPinBbIsNotCreated) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlRes = -1;
    auto memoryManager = new (std::nothrow) TestedDrmMemoryManager(false, true, false, *executionEnvironment);
    EXPECT_EQ(nullptr, memoryManager->pinBBs[rootDeviceIndex]);
    mock->ioctlRes = 0;
    delete memoryManager;
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, WhenAskedAndAllowedAndBigAllocationThenPinAfterAllocate) {
    mock->ioctlExpected.gemUserptr = 2;
    mock->ioctlExpected.execbuffer2 = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 2;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);

    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, 10 * MemoryConstants::megaByte, true)));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenPeekInternalHandleIsCalledThenBoIsReturned) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.handleToPrimeFd = 1;
    mock->outputFd = 1337;
    auto allocation = static_cast<DrmAllocation *>(this->memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, 10 * MemoryConstants::pageSize, true)));
    ASSERT_NE(allocation->getBO(), nullptr);
    uint64_t handle = 0;
    int ret = allocation->peekInternalHandle(this->memoryManager, handle);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(handle, static_cast<uint64_t>(1337));

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenCreateInternalHandleIsCalledThenBoIsReturned) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.handleToPrimeFd = 1;
    mock->outputFd = 1337;
    auto allocation = static_cast<DrmAllocation *>(this->memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, 10 * MemoryConstants::pageSize, true)));
    ASSERT_NE(allocation->getBO(), nullptr);
    uint64_t handle = 0;
    int ret = allocation->createInternalHandle(this->memoryManager, 0u, handle);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(handle, static_cast<uint64_t>(1337));

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenCreateInternalHandleIsCalledThenClearInternalHandleThenSameHandleisReturned) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.handleToPrimeFd = 2;
    mock->outputFd = 1337;
    auto allocation = static_cast<DrmAllocation *>(this->memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, 10 * MemoryConstants::pageSize, true)));
    ASSERT_NE(allocation->getBO(), nullptr);
    uint64_t handle = 0;
    int ret = allocation->createInternalHandle(this->memoryManager, 0u, handle);
    ASSERT_EQ(ret, 0);
    allocation->clearInternalHandle(0u);
    ret = allocation->createInternalHandle(this->memoryManager, 0u, handle);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(handle, static_cast<uint64_t>(1337));

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenPeekInternalHandleIsCalledAndObtainFdFromHandleFailsThenErrorIsReturned) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->outputFd = 1337;
    auto allocation = static_cast<DrmAllocation *>(this->memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, 10 * MemoryConstants::pageSize, true)));
    ASSERT_NE(allocation->getBO(), nullptr);
    memoryManager->failOnObtainFdFromHandle = true;
    uint64_t handle = 0;
    int ret = allocation->peekInternalHandle(this->memoryManager, handle);
    ASSERT_EQ(ret, -1);

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenCallingPeekInternalHandleSeveralTimesThenSameHandleIsReturned) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.handleToPrimeFd = 1;
    uint64_t expectedFd = 1337;
    mock->outputFd = static_cast<int32_t>(expectedFd);
    mock->incrementOutputFdAfterCall = true;
    auto allocation = static_cast<DrmAllocation *>(this->memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, 10 * MemoryConstants::pageSize, true)));
    ASSERT_NE(allocation->getBO(), nullptr);

    EXPECT_EQ(mock->outputFd, static_cast<int32_t>(expectedFd));
    uint64_t handle0 = 0;
    int ret = allocation->peekInternalHandle(this->memoryManager, handle0);
    ASSERT_EQ(ret, 0);
    EXPECT_NE(mock->outputFd, static_cast<int32_t>(expectedFd));

    uint64_t handle1 = 0;
    uint64_t handle2 = 0;

    ret = allocation->peekInternalHandle(this->memoryManager, handle1);
    ASSERT_EQ(ret, 0);
    ret = allocation->peekInternalHandle(this->memoryManager, handle2);
    ASSERT_EQ(ret, 0);

    ASSERT_EQ(handle0, expectedFd);
    ASSERT_EQ(handle1, expectedFd);
    ASSERT_EQ(handle2, expectedFd);

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenCallingCreateInternalHandleSeveralTimesThenSameHandleIsReturned) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.handleToPrimeFd = 1;
    uint64_t expectedFd = 1337;
    mock->outputFd = static_cast<int32_t>(expectedFd);
    mock->incrementOutputFdAfterCall = true;
    auto allocation = static_cast<DrmAllocation *>(this->memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, 10 * MemoryConstants::pageSize, true)));
    ASSERT_NE(allocation->getBO(), nullptr);

    EXPECT_EQ(mock->outputFd, static_cast<int32_t>(expectedFd));
    uint64_t handle0 = 0;
    int ret = allocation->createInternalHandle(this->memoryManager, 0u, handle0);
    ASSERT_EQ(ret, 0);
    EXPECT_NE(mock->outputFd, static_cast<int32_t>(expectedFd));

    uint64_t handle1 = 0;
    uint64_t handle2 = 0;

    ret = allocation->createInternalHandle(this->memoryManager, 0u, handle1);
    ASSERT_EQ(ret, 0);
    ret = allocation->createInternalHandle(this->memoryManager, 0u, handle2);
    ASSERT_EQ(ret, 0);

    ASSERT_EQ(handle0, expectedFd);
    ASSERT_EQ(handle1, expectedFd);
    ASSERT_EQ(handle2, expectedFd);

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenPeekInternalHandleWithHandleIdIsCalledThenBoIsReturned) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.handleToPrimeFd = 1;
    mock->outputFd = 1337;
    auto allocation = static_cast<DrmAllocation *>(this->memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, 10 * MemoryConstants::pageSize, true)));
    ASSERT_NE(allocation->getBO(), nullptr);

    uint64_t handle = 0;
    uint32_t handleId = 0;
    int ret = allocation->peekInternalHandle(this->memoryManager, handleId, handle);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(handle, static_cast<uint64_t>(1337));

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenCallingPeekInternalHandleWithIdSeveralTimesThenSameHandleIsReturned) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.handleToPrimeFd = 1;
    uint64_t expectedFd = 1337;
    mock->outputFd = static_cast<int32_t>(expectedFd);
    mock->incrementOutputFdAfterCall = true;
    auto allocation = static_cast<DrmAllocation *>(this->memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, 10 * MemoryConstants::pageSize, true)));
    ASSERT_NE(allocation->getBO(), nullptr);

    EXPECT_EQ(mock->outputFd, static_cast<int32_t>(expectedFd));
    uint32_t handleId = 0;
    uint64_t handle0 = 0;
    int ret = allocation->peekInternalHandle(this->memoryManager, handleId, handle0);
    ASSERT_EQ(ret, 0);
    EXPECT_NE(mock->outputFd, static_cast<int32_t>(expectedFd));

    uint64_t handle1 = 0;
    uint64_t handle2 = 0;
    ret = allocation->peekInternalHandle(this->memoryManager, handleId, handle1);
    ASSERT_EQ(ret, 0);
    ret = allocation->peekInternalHandle(this->memoryManager, handleId, handle2);
    ASSERT_EQ(ret, 0);

    ASSERT_EQ(handle0, expectedFd);
    ASSERT_EQ(handle1, expectedFd);
    ASSERT_EQ(handle2, expectedFd);

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmContextIdWhenAllocationIsCreatedThenPinWithPassedDrmContextId) {
    mock->ioctlExpected.gemUserptr = 2;
    mock->ioctlExpected.execbuffer2 = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 2;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    auto drmContextId = memoryManager->getDefaultDrmContextId(rootDeviceIndex);
    ASSERT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);
    EXPECT_NE(0u, drmContextId);

    auto alloc = memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, memoryManager->pinThreshold, true));
    EXPECT_EQ(drmContextId, mock->execBuffer.getReserved());

    memoryManager->freeGraphicsMemory(alloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, WhenAskedAndAllowedButSmallAllocationThenDoNotPinAfterAllocate) {
    mock->ioctlExpected.gemUserptr = 2;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 2;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    ASSERT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);

    // one page is too small for early pinning
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, MemoryConstants::pageSize, true)));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, WhenNotAskedButAllowedThenDoNotPinAfterAllocate) {
    mock->ioctlExpected.gemUserptr = 2;
    mock->ioctlExpected.gemClose = 2;
    mock->ioctlExpected.gemWait = 1;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);

    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, MemoryConstants::pageSize, false)));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, WhenAskedButNotAllowedThenDoNotPinAfterAllocate) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }

    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, MemoryConstants::pageSize, true)));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
}

// ---- HostPtr
HWTEST_TEMPLATED_F(DrmMemoryManagerTest, WhenAskedAndAllowedAndBigAllocationHostPtrThenPinAfterAllocate) {
    mock->ioctlExpected.gemUserptr = 2;
    mock->ioctlExpected.gemClose = 2;
    mock->ioctlExpected.execbuffer2 = 1;
    mock->ioctlExpected.gemWait = 1;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);

    allocationData.size = 10 * MemoryConstants::megaByte;
    allocationData.hostPtr = ::alignedMalloc(allocationData.size, 4096);
    allocationData.flags.forcePin = true;
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
    ::alignedFree(const_cast<void *>(allocationData.hostPtr));
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenSmallAllocationHostPtrAllocationWhenForcePinIsTrueThenBufferObjectIsNotPinned) {
    mock->ioctlExpected.gemUserptr = 2;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 2;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);

    // one page is too small for early pinning
    allocationData.size = 4 * 1024;
    allocationData.hostPtr = ::alignedMalloc(allocationData.size, 4096);
    allocationData.flags.forcePin = true;
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);

    ::alignedFree(const_cast<void *>(allocationData.hostPtr));
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, WhenNotAskedButAllowedHostPtrThendoNotPinAfterAllocate) {
    mock->ioctlExpected.gemUserptr = 2;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 2;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    ASSERT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);

    allocationData.size = 4 * 1024;
    allocationData.hostPtr = ::alignedMalloc(allocationData.size, 4096);
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);

    ::alignedFree(const_cast<void *>(allocationData.hostPtr));
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, WhenAskedButNotAllowedHostPtrThenDoNotPinAfterAllocate) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);

    allocationData.size = 4 * 1024;
    allocationData.hostPtr = ::alignedMalloc(allocationData.size, 4096);
    allocationData.flags.forcePin = true;
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);

    ::alignedFree(const_cast<void *>(allocationData.hostPtr));
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, WhenUnreferenceIsCalledThenCallSucceeds) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemClose = 1;
    BufferObject *bo = memoryManager->allocUserptr(1234, (size_t)1024, AllocationType::externalHostPtr, rootDeviceIndex);
    ASSERT_NE(nullptr, bo);
    memoryManager->unreference(bo, false);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenPrintBOCreateDestroyResultIsSetAndAllocUserptrIsCalledThenBufferObjectIsCreatedAndDebugInformationIsPrinted) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.PrintBOCreateDestroyResult.set(true);

    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemClose = 1;

    StreamCapture capture;
    capture.captureStdout();
    BufferObject *bo = memoryManager->allocUserptr(0, (size_t)1024, AllocationType::externalHostPtr, rootDeviceIndex);
    ASSERT_NE(nullptr, bo);

    debugManager.flags.PrintBOCreateDestroyResult.set(false);
    std::string output = capture.getCapturedStdout();
    size_t idx = output.find("Created new BO with GEM_USERPTR, handle: BO-");
    size_t expectedValue = 0;
    EXPECT_EQ(expectedValue, idx);

    memoryManager->unreference(bo, false);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenNullptrWhenUnreferenceIsCalledThenCallSucceeds) {
    memoryManager->unreference(nullptr, false);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerCreatedWithGemCloseWorkerModeInactiveThenGemCloseWorkerIsNotCreated) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useGemCloseWorker = true;
    DrmMemoryManager drmMemoryManger(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, *executionEnvironment);
    EXPECT_EQ(nullptr, drmMemoryManger.peekGemCloseWorker());
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerCreatedWithGemCloseWorkerActiveThenGemCloseWorkerIsCreated) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useGemCloseWorker = true;
    DrmMemoryManager drmMemoryManger(GemCloseWorkerMode::gemCloseWorkerActive, false, false, *executionEnvironment);
    EXPECT_NE(nullptr, drmMemoryManger.peekGemCloseWorker());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenAllocationWhenClosingSharedHandleThenSucceeds) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    osHandle handle = 1u;
    this->mock->outputHandle = 2u;
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::sharedBuffer, false, {});
    TestedDrmMemoryManager::OsHandleData osHandleData{handle};

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    EXPECT_EQ(handle, graphicsAllocation->peekSharedHandle());

    memoryManager->closeSharedHandle(graphicsAllocation);
    EXPECT_EQ(Sharing::nonSharedResource, graphicsAllocation->peekSharedHandle());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenUllsLightWhenCreateGraphicsAllocationFromSharedHandleThenAddToResidencyContainer) {
    mock->ioctlExpected.total = -1;
    this->dontTestIoctlInTearDown = true;

    osHandle handle = 1u;
    this->mock->outputHandle = 2u;
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::sharedBuffer, false, {});
    TestedDrmMemoryManager::OsHandleData osHandleData{handle};

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
        engine.osContext->setDirectSubmissionActive();
    }

    auto drmMemoryOperationsHandler = static_cast<DrmMemoryOperationsHandler *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface.get());
    EXPECT_FALSE(drmMemoryOperationsHandler->obtainAndResetNewResourcesSinceLastRingSubmit());

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    EXPECT_EQ(handle, graphicsAllocation->peekSharedHandle());
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(this->device, *graphicsAllocation), MemoryOperationsStatus::success);
    EXPECT_TRUE(drmMemoryOperationsHandler->obtainAndResetNewResourcesSinceLastRingSubmit());

    memoryManager->closeSharedHandle(graphicsAllocation);
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenAllocationWhenClosingInternalHandleThenSucceeds) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.handleToPrimeFd = 1;

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    uint64_t handleVal = 1u;
    this->mock->outputHandle = 2u;
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::sharedBuffer, false, {});

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    EXPECT_EQ(0, graphicsAllocation->createInternalHandle(this->memoryManager, 0u, handleVal));

    memoryManager->closeInternalHandle(handleVal, 0u, graphicsAllocation);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenNoAllocationWhenClosingInternalHandleThenSucceeds) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.handleToPrimeFd = 1;

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    uint64_t handleVal = 1u;
    this->mock->outputHandle = 2u;
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::sharedBuffer, false, {});

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    EXPECT_EQ(0, graphicsAllocation->createInternalHandle(this->memoryManager, 0u, handleVal));

    memoryManager->closeInternalHandle(handleVal, 0u, nullptr);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenNullptrDrmAllocationWhenTryingToRegisterItThenRegisterSharedBoHandleAllocationDoesNothing) {
    ASSERT_TRUE(memoryManager->sharedBoHandles.empty());

    memoryManager->registerSharedBoHandleAllocation(nullptr);
    EXPECT_TRUE(memoryManager->sharedBoHandles.empty());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenAllocationWhenTryingToRegisterIpcExportedThenItsBoIsMarkedAsSharedHandleAndHandleIsStored) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    ASSERT_TRUE(memoryManager->sharedBoHandles.empty());

    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize}));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->registerIpcExportedAllocation(alloc);
    EXPECT_FALSE(memoryManager->sharedBoHandles.empty());

    auto &bos = alloc->getBOs();
    for (auto *bo : bos) {
        if (bo) {
            EXPECT_TRUE(bo->isBoHandleShared());
            EXPECT_EQ(1u, memoryManager->sharedBoHandles.count(std::make_pair(bo->getHandle(), rootDeviceIndex)));
        }
    }

    memoryManager->freeGraphicsMemory(alloc);
    EXPECT_TRUE(memoryManager->sharedBoHandles.empty());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenEmptySharedBoHandlesContainerWhenTryingToGetSharedOwnershipOfNonregisteredHandleThenCreateNewWrapper) {
    ASSERT_TRUE(memoryManager->sharedBoHandles.empty());

    const int someNonregisteredHandle{123};
    auto boHandleWrapper = memoryManager->tryToGetBoHandleWrapperWithSharedOwnership(someNonregisteredHandle, rootDeviceIndex);
    EXPECT_EQ(someNonregisteredHandle, boHandleWrapper.getBoHandle());
    EXPECT_TRUE(memoryManager->sharedBoHandles.empty());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenWrapperInBoHandlesContainerWhenTryingToGetSharedOwnershipOfWrappedHandleThenGetSharedOwnership) {
    const int boHandle{27};
    BufferObjectHandleWrapper boHandleWrapper{boHandle, rootDeviceIndex};

    memoryManager->sharedBoHandles.emplace(std::make_pair(boHandle, rootDeviceIndex), boHandleWrapper.acquireWeakOwnership());
    ASSERT_EQ(1u, memoryManager->sharedBoHandles.count(std::make_pair(boHandle, rootDeviceIndex)));

    {
        auto newBoHandleWrapper = memoryManager->tryToGetBoHandleWrapperWithSharedOwnership(boHandle, rootDeviceIndex);
        EXPECT_EQ(boHandle, newBoHandleWrapper.getBoHandle());
        EXPECT_EQ(1u, memoryManager->sharedBoHandles.count(std::make_pair(boHandle, rootDeviceIndex)));
        EXPECT_FALSE(newBoHandleWrapper.canCloseBoHandle());
    }

    EXPECT_EQ(1u, memoryManager->sharedBoHandles.count(std::make_pair(boHandle, rootDeviceIndex)));
    EXPECT_TRUE(boHandleWrapper.canCloseBoHandle());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenWrapperInBoHandlesContainerWhenTryingToGetSharedOwnershipOfWrappedHandleWithDifferentDeviceIndexThenGetNonSharedOwnership) {
    const int boHandle{27};
    BufferObjectHandleWrapper boHandleWrapper{boHandle, rootDeviceIndex};

    memoryManager->sharedBoHandles.emplace(std::make_pair(boHandle, rootDeviceIndex), boHandleWrapper.acquireWeakOwnership());
    ASSERT_EQ(1u, memoryManager->sharedBoHandles.count(std::make_pair(boHandle, rootDeviceIndex)));

    {
        auto newBoHandleWrapper = memoryManager->tryToGetBoHandleWrapperWithSharedOwnership(boHandle, rootDeviceIndex + 1);
        EXPECT_EQ(boHandle, newBoHandleWrapper.getBoHandle());
        EXPECT_NE(rootDeviceIndex, newBoHandleWrapper.getRootDeviceIndex());
        EXPECT_EQ(0u, memoryManager->sharedBoHandles.count(std::make_pair(boHandle, rootDeviceIndex + 1)));
        EXPECT_TRUE(newBoHandleWrapper.canCloseBoHandle());
    }

    EXPECT_EQ(1u, memoryManager->sharedBoHandles.count(std::make_pair(boHandle, rootDeviceIndex)));
    EXPECT_TRUE(boHandleWrapper.canCloseBoHandle());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenWrapperInBoHandlesContainerWhenTryingToGetSharedOwnershipOfWrappedHandleWithDifferentHandleIndexPairThenGetNonSharedOwnership) {
    const int boHandle{27};
    BufferObjectHandleWrapper boHandleWrapper{boHandle, rootDeviceIndex};

    memoryManager->sharedBoHandles.emplace(std::make_pair(boHandle, rootDeviceIndex), boHandleWrapper.acquireWeakOwnership());
    ASSERT_EQ(1u, memoryManager->sharedBoHandles.count(std::make_pair(boHandle, rootDeviceIndex)));

    {
        auto newBoHandleWrapper = memoryManager->tryToGetBoHandleWrapperWithSharedOwnership(boHandle + 1, rootDeviceIndex + 1);
        EXPECT_NE(boHandle, newBoHandleWrapper.getBoHandle());
        EXPECT_NE(rootDeviceIndex, newBoHandleWrapper.getRootDeviceIndex());
        EXPECT_EQ(0u, memoryManager->sharedBoHandles.count(std::make_pair(boHandle + 1, rootDeviceIndex + 1)));
        EXPECT_TRUE(newBoHandleWrapper.canCloseBoHandle());
    }

    EXPECT_EQ(1u, memoryManager->sharedBoHandles.count(std::make_pair(boHandle, rootDeviceIndex)));
    EXPECT_TRUE(boHandleWrapper.canCloseBoHandle());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenDeviceSharedAllocationWhichRequiresHostMapThenCorrectAlignmentReturned) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.gemMmapOffset = 1;

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    this->mock->outputHandle = 2u;
    size_t size = 4096u;
    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};

    this->mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    this->mock->memoryInfoQueried = true;
    this->mock->queryEngineInfo();
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::gpuTimestampDeviceBuffer, false, {});

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, true, false, nullptr);
    DrmAllocation *drmAlloc = (DrmAllocation *)graphicsAllocation;

    EXPECT_TRUE(isAligned<MemoryConstants::pageSize64k>(drmAlloc->getMmapPtr()));

    memoryManager->closeSharedHandle(graphicsAllocation);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenDeviceSharedAllocationWhenSetCacheRegionFailsThenNullptrIsReturned) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 0;
    mock->ioctlExpected.gemClose = 0;
    mock->ioctlExpected.gemMmapOffset = 1;

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    this->mock->outputHandle = 2u;
    size_t size = 4096u;
    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};

    this->mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    this->mock->memoryInfoQueried = true;
    this->mock->queryEngineInfo();
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::gpuTimestampDeviceBuffer, false, {});
    properties.cacheRegion = std::numeric_limits<uint32_t>::max();

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, true, false, nullptr);

    EXPECT_EQ(nullptr, graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenAllocationWhenFreeingThenSucceeds) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize}));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    EXPECT_EQ(Sharing::nonSharedResource, alloc->peekSharedHandle());
    memoryManager->freeGraphicsMemory(alloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenInjectedFailureWhenAllocatingThenAllocationFails) {
    mock->ioctlExpected.total = -1; // don't care

    InjectedFunction method = [this](size_t failureIndex) {
        auto ptr = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize});

        if (MemoryManagement::nonfailingAllocation != failureIndex) {
            EXPECT_EQ(nullptr, ptr);
        } else {
            EXPECT_NE(nullptr, ptr);
            memoryManager->freeGraphicsMemory(ptr);
        }
    };
    injectFailures(method);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenZeroBytesWhenAllocatingThenAllocationIsCreated) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    auto ptr = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, 0u});
    ASSERT_NE(nullptr, ptr);
    EXPECT_NE(nullptr, ptr->getUnderlyingBuffer());

    memoryManager->freeGraphicsMemory(ptr);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenThreeBytesWhenAllocatingThenAllocationIsCreated) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    auto ptr = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize});
    ASSERT_NE(nullptr, ptr);
    EXPECT_NE(nullptr, ptr->getUnderlyingBuffer());

    memoryManager->freeGraphicsMemory(ptr);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenUserptrWhenCreatingAllocationThenFail) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlRes = -1;

    auto ptr = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize});
    EXPECT_EQ(nullptr, ptr);
    mock->ioctlRes = 0;
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenNullPtrWhenFreeingThenSucceeds) {
    memoryManager->freeGraphicsMemory(nullptr);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenHostPtrWhenCreatingAllocationThenSucceeds) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    void *ptr = ::alignedMalloc(1024, 4096);
    ASSERT_NE(nullptr, ptr);

    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, false, 1024}, ptr));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getUnderlyingBuffer());
    EXPECT_EQ(ptr, alloc->getUnderlyingBuffer());

    auto bo = alloc->getBO();
    ASSERT_NE(nullptr, bo);
    EXPECT_EQ(ptr, reinterpret_cast<void *>(bo->peekAddress()));
    EXPECT_EQ(Sharing::nonSharedResource, alloc->peekSharedHandle());
    memoryManager->freeGraphicsMemory(alloc);
    ::alignedFree(ptr);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenNullHostPtrWhenCreatingAllocationThenSucceeds) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    void *ptr = nullptr;

    allocationData.hostPtr = nullptr;
    allocationData.size = MemoryConstants::pageSize;
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData));
    ASSERT_NE(nullptr, alloc);
    EXPECT_EQ(ptr, alloc->getUnderlyingBuffer());

    auto bo = alloc->getBO();
    ASSERT_NE(nullptr, bo);
    EXPECT_EQ(ptr, reinterpret_cast<void *>(bo->peekAddress()));

    memoryManager->freeGraphicsMemory(alloc);
    ::alignedFree(ptr);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenMisalignedHostPtrWhenCreatingAllocationThenSucceeds) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    void *ptrT = ::alignedMalloc(1024, 4096);
    ASSERT_NE(nullptr, ptrT);

    void *ptr = ptrOffset(ptrT, 128);

    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, false, 1024}, ptr));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getUnderlyingBuffer());
    EXPECT_EQ(ptr, alloc->getUnderlyingBuffer());

    auto bo = alloc->getBO();
    ASSERT_NE(nullptr, bo);
    EXPECT_EQ(ptrT, reinterpret_cast<void *>(bo->peekAddress()));

    memoryManager->freeGraphicsMemory(alloc);
    ::alignedFree(ptrT);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenHostPtrUserptrWhenCreatingAllocationThenFails) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlRes = -1;

    void *ptrT = ::alignedMalloc(1024, 4096);
    ASSERT_NE(nullptr, ptrT);

    auto alloc = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, false, 1024}, ptrT);
    EXPECT_EQ(nullptr, alloc);

    ::alignedFree(ptrT);
    mock->ioctlRes = 0;
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmAllocationWhenHandleFenceCompletionThenCallBufferObjectWait) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.contextDestroy = 0;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, 1024});
    memoryManager->handleFenceCompletion(allocation);
    mock->testIoctls();

    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.gemWait = 2;
    memoryManager->freeGraphicsMemory(allocation);
}

TEST(DrmMemoryManagerTest2, givenDrmMemoryManagerWhengetSystemSharedMemoryIsCalledThenContextGetParamIsCalled) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(4u);
    for (auto i = 0u; i < 4u; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(DrmMockCustom::create(*executionEnvironment->rootDeviceEnvironments[0]));
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);

    for (auto i = 0u; i < 4u; i++) {
        auto mock = executionEnvironment->rootDeviceEnvironments[i]->osInterface->getDriverModel()->as<DrmMockCustom>();

        mock->callBaseQueryGttSize = true;
        mock->getContextParamRetValue = 16 * MemoryConstants::gigaByte;
        mock->ioctlCnt.contextGetParam = 0;
        uint64_t mem = memoryManager->getSystemSharedMemory(i);
        mock->ioctlExpected.contextGetParam = 1;
        EXPECT_EQ(mock->recordedGetContextParam.param, static_cast<__u64>(I915_CONTEXT_PARAM_GTT_SIZE));
        EXPECT_GT(mem, 0u);

        executionEnvironment->rootDeviceEnvironments[i]->osInterface.reset();
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenBitnessWhenGettingMaxApplicationAddressThenCorrectValueIsReturned) {
    uint64_t maxAddr = memoryManager->getMaxApplicationAddress();
    if constexpr (is64bit) {
        EXPECT_EQ(maxAddr, MemoryConstants::max64BitAppAddress);
    } else {
        EXPECT_EQ(maxAddr, MemoryConstants::max32BitAppAddress);
    }
}

TEST(DrmMemoryManagerTest2, WhenGetMinimumSystemSharedMemoryThenCorrectValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(4u);
    for (auto i = 0u; i < 4u; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(DrmMockCustom::create(*executionEnvironment->rootDeviceEnvironments[0]));
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);

    for (auto i = 0u; i < 4u; i++) {
        auto mock = executionEnvironment->rootDeviceEnvironments[i]->osInterface->getDriverModel()->as<DrmMockCustom>();
        mock->callBaseQueryGttSize = true;

        auto hostMemorySize = MemoryConstants::pageSize * SysCalls::sysconfReturn;
        // gpuMemSize < hostMemSize, no memory info
        auto gpuMemorySize = hostMemorySize - 1u;

        mock->ioctlExpected.contextGetParam = 1;
        mock->getContextParamRetValue = gpuMemorySize;

        uint64_t systemSharedMemorySize = memoryManager->getSystemSharedMemory(i);

        EXPECT_EQ(gpuMemorySize, systemSharedMemorySize);
        mock->ioctlExpected.contextDestroy = 0;
        mock->ioctlExpected.contextCreate = 0;
        mock->testIoctls();

        // gpuMemSize > hostMemSize, no memory info
        gpuMemorySize = hostMemorySize + 1u;
        mock->getContextParamRetValue = gpuMemorySize;
        systemSharedMemorySize = memoryManager->getSystemSharedMemory(i);
        mock->ioctlExpected.contextGetParam = 2;
        EXPECT_EQ(hostMemorySize, systemSharedMemorySize);
        mock->testIoctls();

        // gpuMemSize > hostMemSize > systemMemoryRegionSize

        MemoryRegion memoryRegion{};
        memoryRegion.probedSize = hostMemorySize - 2;

        MemoryInfo::RegionContainer regionInfo{};
        regionInfo.push_back(memoryRegion);

        mock->memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *mock);

        gpuMemorySize = hostMemorySize + 1u;
        mock->getContextParamRetValue = gpuMemorySize;
        systemSharedMemorySize = memoryManager->getSystemSharedMemory(i);
        mock->ioctlExpected.contextGetParam = 3;
        EXPECT_EQ(memoryRegion.probedSize, systemSharedMemorySize);
        mock->testIoctls();

        // hostMemSize > systemMemoryRegionSize > gpuMemSize

        gpuMemorySize = hostMemorySize - 3;
        mock->getContextParamRetValue = gpuMemorySize;
        systemSharedMemorySize = memoryManager->getSystemSharedMemory(i);
        mock->ioctlExpected.contextGetParam = 4;
        EXPECT_EQ(gpuMemorySize, systemSharedMemorySize);
        mock->testIoctls();

        executionEnvironment->rootDeviceEnvironments[i]->osInterface.reset();
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenBoWaitFailureThenExpectThrow) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    BufferObject *bo = memoryManager->allocUserptr(0, (size_t)1024, AllocationType::externalHostPtr, rootDeviceIndex);
    ASSERT_NE(nullptr, bo);
    mock->ioctlRes = -EIO;
    EXPECT_THROW(bo->wait(-1), std::exception);
    mock->ioctlRes = 1;

    memoryManager->unreference(bo, false);
    mock->ioctlRes = 0;
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, WhenNullOsHandleStorageAskedForPopulationThenFilledPointerIsReturned) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    storage.fragmentStorageData[0].fragmentSize = 1;
    memoryManager->populateOsHandles(storage, rootDeviceIndex);
    EXPECT_NE(nullptr, storage.fragmentStorageData[0].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[2].osHandleStorage);
    storage.fragmentStorageData[0].freeTheFragment = true;
    memoryManager->cleanOsHandles(storage, rootDeviceIndex);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledHostMemoryValidationWhenReadOnlyPointerCausesPinningFailWithEfaultThenPopulateOsHandlesReturnsInvalidHostPointerError) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    *executionEnvironment));

    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    storage.fragmentStorageData[0].fragmentSize = 1;

    mock->reset();

    DrmMockCustom::IoctlResExt ioctlResExt = {1, -1};
    ioctlResExt.no.push_back(2);
    ioctlResExt.no.push_back(3);
    mock->ioctlResExt = &ioctlResExt;
    mock->errnoValue = EFAULT;
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.execbuffer2 = 3;

    MemoryManager::AllocationStatus result = memoryManager->populateOsHandles(storage, rootDeviceIndex);

    EXPECT_EQ(MemoryManager::AllocationStatus::InvalidHostPointer, result);
    mock->testIoctls();

    EXPECT_NE(nullptr, storage.fragmentStorageData[0].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[2].osHandleStorage);

    storage.fragmentStorageData[0].freeTheFragment = true;
    memoryManager->cleanOsHandles(storage, rootDeviceIndex);
    mock->ioctlResExt = &mock->none;
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledHostMemoryValidationWhenReadOnlyPointerCausesPinningFailWithEfaultThenAlocateMemoryForNonSvmHostPtrReturnsNullptr) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    *executionEnvironment));

    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }

    mock->reset();
    size_t dummySize = 13u;

    DrmMockCustom::IoctlResExt ioctlResExt = {1, -1};
    ioctlResExt.no.push_back(2);
    ioctlResExt.no.push_back(3);
    mock->ioctlResExt = &ioctlResExt;
    mock->errnoValue = EFAULT;
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.execbuffer2 = 3;
    mock->ioctlExpected.gemClose = 1;

    AllocationData allocationData;
    allocationData.size = dummySize;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x5001);
    allocationData.rootDeviceIndex = device->getRootDeviceIndex();

    auto gfxPartition = memoryManager->getGfxPartition(device->getRootDeviceIndex());

    auto allocatedPointer = gfxPartition->heapAllocate(HeapIndex::heapStandard, dummySize);
    gfxPartition->freeGpuAddressRange(allocatedPointer, dummySize);

    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);

    EXPECT_EQ(nullptr, allocation);
    mock->testIoctls();
    mock->ioctlResExt = &mock->none;

    // make sure that partition is free
    size_t dummySize2 = 13u;
    auto allocatedPointer2 = gfxPartition->heapAllocate(HeapIndex::heapStandard, dummySize2);
    EXPECT_EQ(allocatedPointer2, allocatedPointer);
    gfxPartition->freeGpuAddressRange(allocatedPointer, dummySize2);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledHostMemoryValidationWhenHostPtrDoesntCausePinningFailThenAlocateMemoryForNonSvmHostPtrReturnsAllocation) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    *executionEnvironment));

    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }

    mock->reset();

    DrmMockCustom::IoctlResExt ioctlResExt = {1, -1};
    ioctlResExt.no.push_back(2);
    ioctlResExt.no.push_back(3);
    mock->ioctlResExt = &ioctlResExt;
    mock->errnoValue = 0;
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.execbuffer2 = 3;

    AllocationData allocationData;
    allocationData.size = 13u;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x5001);
    allocationData.rootDeviceIndex = device->getRootDeviceIndex();

    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);

    EXPECT_NE(nullptr, allocation);

    mock->testIoctls();
    mock->ioctlResExt = &mock->none;

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledHostMemoryValidationWhenAllocatingMemoryForNonSvmHostPtrThenAllocatedCorrectly) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    *executionEnvironment));

    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }

    mock->reset();

    DrmMockCustom::IoctlResExt ioctlResExt = {1, -1};
    ioctlResExt.no.push_back(2);
    ioctlResExt.no.push_back(3);
    mock->ioctlResExt = &ioctlResExt;
    mock->errnoValue = 0;
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.execbuffer2 = 3;

    AllocationData allocationData;
    allocationData.size = 13u;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x5001);
    allocationData.rootDeviceIndex = device->getRootDeviceIndex();

    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);

    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(allocation->getGpuAddress() - allocation->getAllocationOffset(), mock->execBufferBufferObjects.getOffset());

    mock->testIoctls();
    mock->ioctlResExt = &mock->none;

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledHostMemoryValidationWhenPinningFailWithErrorDifferentThanEfaultThenPopulateOsHandlesReturnsError) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    *executionEnvironment));
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    storage.fragmentStorageData[0].fragmentSize = 1;

    mock->reset();

    DrmMockCustom::IoctlResExt ioctlResExt = {1, -1};
    ioctlResExt.no.push_back(2);
    ioctlResExt.no.push_back(3);
    mock->ioctlResExt = &ioctlResExt;
    mock->errnoValue = ENOMEM;
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.execbuffer2 = 3;

    MemoryManager::AllocationStatus result = memoryManager->populateOsHandles(storage, rootDeviceIndex);

    EXPECT_EQ(MemoryManager::AllocationStatus::Error, result);
    mock->testIoctls();

    EXPECT_NE(nullptr, storage.fragmentStorageData[0].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[2].osHandleStorage);

    storage.fragmentStorageData[0].freeTheFragment = true;
    memoryManager->cleanOsHandles(storage, rootDeviceIndex);
    mock->ioctlResExt = &mock->none;
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenNoInputsWhenOsHandleIsCreatedThenAllBoHandlesAreInitializedAsNullPtrs) {
    OsHandleLinux boHandle;
    EXPECT_EQ(nullptr, boHandle.bo);

    std::unique_ptr<OsHandleLinux> boHandle2(new OsHandleLinux);
    EXPECT_EQ(nullptr, boHandle2->bo);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenPointerAndSizeWhenAskedToCreateGrahicsAllocationThenGraphicsAllocationIsCreated) {
    OsHandleStorage handleStorage;
    auto ptr = reinterpret_cast<void *>(0x1000);
    auto ptr2 = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize;

    handleStorage.fragmentStorageData[0].cpuPtr = ptr;
    handleStorage.fragmentStorageData[1].cpuPtr = ptr2;
    handleStorage.fragmentStorageData[2].cpuPtr = nullptr;

    handleStorage.fragmentStorageData[0].fragmentSize = size;
    handleStorage.fragmentStorageData[1].fragmentSize = size * 2;
    handleStorage.fragmentStorageData[2].fragmentSize = size * 3;

    allocationData.size = size;
    allocationData.hostPtr = ptr;
    auto allocation = std::unique_ptr<GraphicsAllocation>(memoryManager->createGraphicsAllocation(handleStorage, allocationData));

    EXPECT_EQ(reinterpret_cast<void *>(allocation->getGpuAddress()), ptr);
    EXPECT_EQ(ptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());

    EXPECT_EQ(ptr, allocation->fragmentsStorage.fragmentStorageData[0].cpuPtr);
    EXPECT_EQ(ptr2, allocation->fragmentsStorage.fragmentStorageData[1].cpuPtr);
    EXPECT_EQ(nullptr, allocation->fragmentsStorage.fragmentStorageData[2].cpuPtr);

    EXPECT_EQ(size, allocation->fragmentsStorage.fragmentStorageData[0].fragmentSize);
    EXPECT_EQ(size * 2, allocation->fragmentsStorage.fragmentStorageData[1].fragmentSize);
    EXPECT_EQ(size * 3, allocation->fragmentsStorage.fragmentStorageData[2].fragmentSize);

    EXPECT_NE(&allocation->fragmentsStorage, &handleStorage);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenMemoryManagerWhenCreatingGraphicsAllocation64kbThenNullPtrIsReturned) {
    allocationData.size = MemoryConstants::pageSize64k;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    auto allocation = memoryManager->allocateGraphicsMemory64kb(allocationData);
    EXPECT_EQ(nullptr, allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenRequiresStandardHeapThenStandardHeapIsAcquired) {
    const uint32_t rootDeviceIndex = 0;
    size_t bufferSize = 4096u;
    uint64_t range = memoryManager->acquireGpuRange(bufferSize, rootDeviceIndex, HeapIndex::heapStandard);

    auto gmmHelper = device->getGmmHelper();
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapStandard)), range);
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapStandard)), range);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenRequiresStandard2MBHeapThenStandard2MBHeapIsAcquired) {
    const uint32_t rootDeviceIndex = 0;
    size_t bufferSize = 4096u;
    uint64_t range = memoryManager->acquireGpuRange(bufferSize, rootDeviceIndex, HeapIndex::heapStandard2MB);

    auto gmmHelper = device->getGmmHelper();
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapStandard2MB)), range);
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapStandard2MB)), range);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenCallingAllocateAndReleaseInterruptThenCallIoctlHelper) {
    auto mockIoctlHelper = new MockIoctlHelper(*mock);

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(rootDeviceIndex));
    drm.ioctlHelper.reset(mockIoctlHelper);

    uint32_t handle = 0;

    EXPECT_EQ(0u, mockIoctlHelper->allocateInterruptCalled);
    EXPECT_EQ(0u, mockIoctlHelper->releaseInterruptCalled);

    memoryManager->allocateInterrupt(handle, rootDeviceIndex);
    EXPECT_EQ(1u, mockIoctlHelper->allocateInterruptCalled);
    EXPECT_EQ(0u, mockIoctlHelper->releaseInterruptCalled);

    handle = 123;
    EXPECT_EQ(InterruptId::notUsed, mockIoctlHelper->latestReleaseInterruptHandle);

    memoryManager->releaseInterrupt(handle, rootDeviceIndex);
    EXPECT_EQ(1u, mockIoctlHelper->allocateInterruptCalled);
    EXPECT_EQ(1u, mockIoctlHelper->releaseInterruptCalled);
    EXPECT_EQ(123u, mockIoctlHelper->latestReleaseInterruptHandle);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenCallingCreateAndReleaseMediaContextThenCallIoctlHelper) {
    class MyMockIoctlHelper : public MockIoctlHelper {
      public:
        using MockIoctlHelper::MockIoctlHelper;

        bool createMediaContext(uint32_t vmId, void *controlSharedMemoryBuffer, uint32_t controlSharedMemoryBufferSize, void *controlBatchBuffer, uint32_t controlBatchBufferSize, void *&outDoorbell) override {
            mediaContextVmId = vmId;
            return MockIoctlHelper::createMediaContext(vmId, controlSharedMemoryBuffer, controlSharedMemoryBufferSize, controlBatchBuffer, controlBatchBufferSize, outDoorbell);
        }

        uint32_t mediaContextVmId = 0;
    };
    auto mockIoctlHelper = new MyMockIoctlHelper(*mock);

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(rootDeviceIndex));
    drm.ioctlHelper.reset(mockIoctlHelper);

    void *handle = nullptr;

    EXPECT_EQ(0u, mockIoctlHelper->createMediaContextCalled);
    EXPECT_EQ(0u, mockIoctlHelper->releaseMediaContextCalled);

    memoryManager->createMediaContext(rootDeviceIndex, nullptr, 0, nullptr, 0, handle);
    EXPECT_EQ(1u, mockIoctlHelper->createMediaContextCalled);
    EXPECT_EQ(mock->getVirtualMemoryAddressSpace(0), mockIoctlHelper->mediaContextVmId);
    EXPECT_EQ(0u, mockIoctlHelper->releaseMediaContextCalled);

    memoryManager->releaseMediaContext(rootDeviceIndex, handle);
    EXPECT_EQ(1u, mockIoctlHelper->createMediaContextCalled);
    EXPECT_EQ(1u, mockIoctlHelper->releaseMediaContextCalled);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenCallingGetNumMediaThenCallIoctlHelper) {
    auto mockIoctlHelper = new MockIoctlHelper(*mock);

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(rootDeviceIndex));
    drm.ioctlHelper.reset(mockIoctlHelper);

    EXPECT_EQ(0u, mockIoctlHelper->getNumMediaDecodersCalled);
    EXPECT_EQ(0u, mockIoctlHelper->getNumMediaEncodersCalled);

    memoryManager->getNumMediaDecoders(rootDeviceIndex);
    EXPECT_EQ(1u, mockIoctlHelper->getNumMediaDecodersCalled);
    EXPECT_EQ(0u, mockIoctlHelper->getNumMediaEncodersCalled);

    memoryManager->getNumMediaEncoders(rootDeviceIndex);
    EXPECT_EQ(1u, mockIoctlHelper->getNumMediaDecodersCalled);
    EXPECT_EQ(1u, mockIoctlHelper->getNumMediaEncodersCalled);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenCallingGetExtraDevicePropertiesThenCallIoctlHelper) {
    auto mockIoctlHelper = new MockIoctlHelper(*mock);

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(rootDeviceIndex));
    drm.ioctlHelper.reset(mockIoctlHelper);

    uint32_t moduleId = 0;
    uint16_t serverType = 0;

    EXPECT_EQ(0u, mockIoctlHelper->queryDeviceParamsCalled);

    memoryManager->getExtraDeviceProperties(rootDeviceIndex, &moduleId, &serverType);
    EXPECT_EQ(1u, mockIoctlHelper->queryDeviceParamsCalled);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenShareableEnabledWhenAskedToCreateGraphicsAllocationThenValidAllocationIsReturnedAndStandard64KBHeapIsUsed) {
    mock->ioctlHelper.reset(new MockIoctlHelper(*mock));
    mock->queryMemoryInfo();
    EXPECT_NE(nullptr, mock->getMemoryInfo());
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    if (debugManager.flags.UseGemCreateExtInAllocateMemoryByKMD.get() == 0 ||
        !productHelper.useGemCreateExtInAllocateMemoryByKMD()) {
        mock->ioctlExpected.gemCreate = 1;
    } else {
        mock->ioctlExpected.gemCreateExt = 1;
    }
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    allocationData.size = MemoryConstants::pageSize;
    allocationData.flags.shareable = true;
    auto allocation = memoryManager->allocateMemoryByKMD(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_NE(0u, allocation->getGpuAddress());

    auto gmmHelper = device->getGmmHelper();
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(allocation->getRootDeviceIndex())->getHeapBase(HeapIndex::heapStandard64KB)), allocation->getGpuAddress());
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(allocation->getRootDeviceIndex())->getHeapLimit(HeapIndex::heapStandard64KB)), allocation->getGpuAddress());

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenAllocateMemoryByKMDWhenPreferCompressedThenCompressionEnabled) {
    DebugManagerStateRestore restorer;
    debugManager.flags.RenderCompressedBuffersEnabled.set(true);

    mock->ioctlHelper.reset(new MockIoctlHelper(*mock));
    mock->queryMemoryInfo();
    EXPECT_NE(nullptr, mock->getMemoryInfo());
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    if (debugManager.flags.UseGemCreateExtInAllocateMemoryByKMD.get() == 0 ||
        !productHelper.useGemCreateExtInAllocateMemoryByKMD()) {
        mock->ioctlExpected.gemCreate = 2;
    } else {
        mock->ioctlExpected.gemCreateExt = 2;
    }
    mock->ioctlExpected.gemWait = 2;
    mock->ioctlExpected.gemClose = 2;

    allocationData.size = MemoryConstants::pageSize;
    allocationData.flags.preferCompressed = false;
    auto allocation = memoryManager->allocateMemoryByKMD(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_FALSE(allocation->getDefaultGmm()->isCompressionEnabled());
    memoryManager->freeGraphicsMemory(allocation);

    allocationData.flags.preferCompressed = true;
    allocation = memoryManager->allocateMemoryByKMD(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_TRUE(allocation->getDefaultGmm()->isCompressionEnabled());
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenSizeAndAlignmentWhenAskedToCreateGraphicsAllocationThenValidAllocationIsReturnedAndMemoryIsAligned) {
    mock->ioctlHelper.reset(new MockIoctlHelper(*mock));
    mock->queryMemoryInfo();
    EXPECT_NE(nullptr, mock->getMemoryInfo());
    allocationData.size = 1;
    int ioctlCnt = 0;
    size_t alignment = 8 * MemoryConstants::megaByte;

    do {
        alignment >>= 1;
        allocationData.alignment = alignment;
        auto allocation = memoryManager->allocateMemoryByKMD(allocationData);
        EXPECT_NE(nullptr, allocation);
        auto gpuAddr = allocation->getGpuAddress();
        EXPECT_NE(0u, gpuAddr);
        if (alignment != 0) {
            EXPECT_EQ(gpuAddr & (~(alignment - 1)), gpuAddr);
        }
        memoryManager->freeGraphicsMemory(allocation);
        ioctlCnt += 1;
    } while (alignment != 0);
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    if (debugManager.flags.UseGemCreateExtInAllocateMemoryByKMD.get() == 0 ||
        !productHelper.useGemCreateExtInAllocateMemoryByKMD()) {
        mock->ioctlExpected.gemCreate = ioctlCnt;
    } else {
        mock->ioctlExpected.gemCreateExt = ioctlCnt;
    }
    mock->ioctlExpected.gemWait = ioctlCnt;
    mock->ioctlExpected.gemClose = ioctlCnt;
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenMisalignedHostPtrAndMultiplePagesSizeWhenAskedForGraphicsAllocationThenItContainsAllFragmentsWithProperGpuAdrresses) {
    mock->ioctlExpected.gemUserptr = 3;
    mock->ioctlExpected.gemWait = 3;
    mock->ioctlExpected.gemClose = 3;

    auto ptr = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize * 10;
    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, false, size}, ptr);

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());

    ASSERT_EQ(3u, hostPtrManager->getFragmentCount());

    auto reqs = MockHostPtrManager::getAllocationRequirements(rootDeviceIndex, ptr, size);

    for (int i = 0; i < maxFragmentsCount; i++) {
        auto osHandle = static_cast<OsHandleLinux *>(graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage);
        ASSERT_NE(nullptr, osHandle->bo);
        EXPECT_EQ(reqs.allocationFragments[i].allocationSize, osHandle->bo->peekSize());
        EXPECT_EQ(reqs.allocationFragments[i].allocationPtr, reinterpret_cast<void *>(osHandle->bo->peekAddress()));
    }
    memoryManager->freeGraphicsMemory(graphicsAllocation);

    EXPECT_EQ(0u, hostPtrManager->getFragmentCount());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedFor32BitAllocationThen32BitDrmAllocationIsBeingReturned) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    auto size = 10u;
    memoryManager->setForce32BitAllocations(true);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, size, nullptr, AllocationType::buffer);
    EXPECT_NE(nullptr, allocation);
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_GE(allocation->getUnderlyingBufferSize(), size);

    auto address64bit = allocation->getGpuAddressToPatch();
    EXPECT_LT(address64bit, MemoryConstants::max32BitAddress);
    EXPECT_TRUE(allocation->is32BitAllocation());

    auto gmmHelper = device->getGmmHelper();
    EXPECT_EQ(gmmHelper->canonize(memoryManager->getExternalHeapBaseAddress(allocation->getRootDeviceIndex(), allocation->isAllocatedInLocalMemoryPool())), allocation->getGpuBaseAddress());

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedFor32BitAllocationWhenLimitedAllocationEnabledThen32BitDrmAllocationWithGpuAddrDifferentFromCpuAddrIsBeingReturned) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    auto size = 10u;
    memoryManager->setForce32BitAllocations(true);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, size, nullptr, AllocationType::buffer);
    EXPECT_NE(nullptr, allocation);
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_GE(allocation->getUnderlyingBufferSize(), size);

    EXPECT_NE((uint64_t)allocation->getGpuAddress(), (uint64_t)allocation->getUnderlyingBuffer());
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenMemoryManagerWhenLimitedRangeAllocatorSetThenHeapSizeAndEndAddrCorrectlySetForGivenGpuRange) {
    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    uint64_t sizeBig = 4 * MemoryConstants::megaByte + MemoryConstants::pageSize;
    auto gpuAddressLimitedRange = memoryManager->getGfxPartition(rootDeviceIndex)->heapAllocate(HeapIndex::heapStandard, sizeBig);
    EXPECT_LT(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapStandard), gpuAddressLimitedRange);
    EXPECT_GT(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapStandard), gpuAddressLimitedRange + sizeBig);
    EXPECT_EQ(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapMinimalAddress(HeapIndex::heapStandard), gpuAddressLimitedRange);

    auto gpuInternal32BitAlloc = memoryManager->getGfxPartition(rootDeviceIndex)->heapAllocate(HeapIndex::heapInternalDeviceMemory, sizeBig);
    EXPECT_LT(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapInternalDeviceMemory), gpuInternal32BitAlloc);
    EXPECT_GT(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapInternalDeviceMemory), gpuInternal32BitAlloc + sizeBig);
    EXPECT_EQ(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapMinimalAddress(HeapIndex::heapInternalDeviceMemory), gpuInternal32BitAlloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedForAllocationWithAlignmentAndLimitedRangeAllocatorSetAndAcquireGpuRangeFailsThenNullIsReturned) {
    mock->ioctlExpected.gemUserptr = 0;
    mock->ioctlExpected.gemClose = 0;

    AllocationData allocationData;

    // emulate GPU address space exhaust
    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);
    memoryManager->getGfxPartition(rootDeviceIndex)->heapInit(HeapIndex::heapStandard, 0x0, 0x10000);

    // set size to something bigger than allowed space
    allocationData.size = 0x20000;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    EXPECT_EQ(nullptr, memoryManager->allocateGraphicsMemoryWithAlignment(allocationData));
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedFor32BitAllocationWithHostPtrAndAllocUserptrFailsThenFails) {
    mock->ioctlExpected.gemUserptr = 1;

    this->ioctlResExt = {mock->ioctlCnt.total, -1};
    mock->ioctlResExt = &ioctlResExt;

    auto size = 10u;
    void *hostPtr = reinterpret_cast<void *>(0x1000);
    memoryManager->setForce32BitAllocations(true);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, size, hostPtr, AllocationType::buffer);

    EXPECT_EQ(nullptr, allocation);
    mock->ioctlResExt = &mock->none;
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedFor32BitAllocationAndAllocUserptrFailsThenFails) {
    mock->ioctlExpected.gemUserptr = 1;

    this->ioctlResExt = {mock->ioctlCnt.total, -1};
    mock->ioctlResExt = &ioctlResExt;

    auto size = 10u;
    memoryManager->setForce32BitAllocations(true);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, size, nullptr, AllocationType::buffer);

    EXPECT_EQ(nullptr, allocation);
    mock->ioctlResExt = &mock->none;
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenLimitedRangeAllocatorWhenAskedForInternal32BitAllocationAndAllocUserptrFailsThenFails) {
    mock->ioctlExpected.gemUserptr = 1;

    this->ioctlResExt = {mock->ioctlCnt.total, -1};
    mock->ioctlResExt = &ioctlResExt;

    auto size = 10u;
    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, size, nullptr, AllocationType::internalHeap);

    EXPECT_EQ(nullptr, allocation);
    mock->ioctlResExt = &mock->none;
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenExhaustedInternalHeapWhenAllocate32BitIsCalledThenNullIsReturned) {
    DebugManagerStateRestore dbgStateRestore;
    debugManager.flags.Force32bitAddressing.set(true);
    memoryManager->setForce32BitAllocations(true);

    size_t size = MemoryConstants::pageSize64k;
    auto alloc = memoryManager->getGfxPartition(rootDeviceIndex)->heapAllocate(HeapIndex::heapInternalDeviceMemory, size);
    EXPECT_NE(0llu, alloc);

    size_t allocationSize = 4 * MemoryConstants::gigaByte;
    auto graphicsAllocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, allocationSize, nullptr, AllocationType::internalHeap);
    EXPECT_EQ(nullptr, graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenSetForceUserptrAlignmentWhenGetUserptrAlignmentThenForcedValueIsReturned) {
    DebugManagerStateRestore dbgStateRestore;
    debugManager.flags.ForceUserptrAlignment.set(123456);

    EXPECT_EQ(123456 * MemoryConstants::kiloByte, memoryManager->getUserptrAlignment());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenGetUserptrAlignmentThenDefaultValueIsReturned) {
    EXPECT_EQ(MemoryConstants::allocationAlignment, memoryManager->getUserptrAlignment());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenMemoryManagerWhenAllocateGraphicsMemoryForImageIsCalledThenProperIoctlsAreCalledAndUnmapSizeIsNonZero) {
    mock->ioctlExpected.gemCreate = 1;
    mock->ioctlExpected.gemSetTiling = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::image2D; // tiled
    imgDesc.imageWidth = 512;
    imgDesc.imageHeight = 512;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    imgInfo.size = 4096u;
    imgInfo.rowPitch = 512u;

    AllocationData allocationData;
    allocationData.imgInfo = &imgInfo;
    allocationData.rootDeviceIndex = rootDeviceIndex;

    auto imageGraphicsAllocation = memoryManager->allocateGraphicsMemoryForImage(allocationData);

    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_NE(0u, imageGraphicsAllocation->getGpuAddress());
    EXPECT_EQ(nullptr, imageGraphicsAllocation->getUnderlyingBuffer());

    EXPECT_TRUE(imageGraphicsAllocation->getDefaultGmm()->resourceParams.Usage ==
                GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE);

    EXPECT_EQ(1u, this->mock->createParamsHandle);
    EXPECT_EQ(imgInfo.size, this->mock->createParamsSize);
    auto ioctlHelper = this->mock->getIoctlHelper();
    uint32_t tilingMode = ioctlHelper->getDrmParamValue(DrmParam::tilingY);
    EXPECT_EQ(tilingMode, this->mock->setTilingMode);
    EXPECT_EQ(imgInfo.rowPitch, this->mock->setTilingStride);
    EXPECT_EQ(1u, this->mock->setTilingHandle);

    memoryManager->freeGraphicsMemory(imageGraphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerAndOsHandleWhenCreateIsCalledThenGraphicsAllocationIsReturned) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    this->mock->outputHandle = 2u;
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::sharedBuffer, false, {});

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(this->mock->inputFd, (int)osHandleData.handle);
    EXPECT_EQ(this->mock->setTilingHandle, 0u);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    auto bo = drmAllocation->getBO();
    EXPECT_EQ(bo->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, bo->peekAddress());
    EXPECT_EQ(1u, bo->getRefCount());
    EXPECT_EQ(size, bo->peekSize());

    EXPECT_EQ(osHandleData.handle, graphicsAllocation->peekSharedHandle());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenDrmMemoryManagerWithLocalMemoryWhenCreateGraphicsAllocationFromSharedHandleIsCalledThenAcquireGpuAddressFromExpectedHeap) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    this->mock->outputHandle = 2u;
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::sharedBuffer, false, {});

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(MemoryPool::systemCpuInaccessible, graphicsAllocation->getMemoryPool());
    EXPECT_EQ(this->mock->inputFd, static_cast<int32_t>(osHandleData.handle));

    const bool prefer57bitAddressing = memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapExtended) > 0;
    const auto expectedHeap = prefer57bitAddressing ? HeapIndex::heapExtended : HeapIndex::heapStandard2MB;

    auto gpuAddress = graphicsAllocation->getGpuAddress();
    auto gmmHelper = device->getGmmHelper();

    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(expectedHeap)), gpuAddress);
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(expectedHeap)), gpuAddress);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    auto bo = drmAllocation->getBO();
    EXPECT_EQ(this->mock->outputHandle, static_cast<uint32_t>(bo->peekHandle()));
    EXPECT_EQ(gpuAddress, bo->peekAddress());
    EXPECT_EQ(size, bo->peekSize());

    const auto expectedUnmapSize = prefer57bitAddressing ? alignUp(size, MemoryConstants::pageSize) : alignUp(size, 2 * MemoryConstants::megaByte);
    EXPECT_EQ(expectedUnmapSize, bo->peekUnmapSize());
    EXPECT_EQ(osHandleData.handle, graphicsAllocation->peekSharedHandle());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerAndOsHandleWhenCreateIsCalledAndRootDeviceIndexIsSpecifiedThenGraphicsAllocationIsReturnedWithCorrectRootDeviceIndex) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    this->mock->outputHandle = 2u;
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::sharedBuffer, false, false, 0u);
    ASSERT_TRUE(properties.subDevicesBitfield.none());

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_EQ(rootDeviceIndex, graphicsAllocation->getRootDeviceIndex());
    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(this->mock->inputFd, (int)osHandleData.handle);
    EXPECT_EQ(this->mock->setTilingHandle, 0u);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    auto bo = drmAllocation->getBO();
    EXPECT_EQ(bo->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, bo->peekAddress());
    EXPECT_EQ(1u, bo->getRefCount());
    EXPECT_EQ(size, bo->peekSize());

    EXPECT_EQ(osHandleData.handle, graphicsAllocation->peekSharedHandle());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerAndOsHandleWhenAllocationFailsThenReturnNullPtr) {
    TestedDrmMemoryManager::OsHandleData osHandleData{1u};

    InjectedFunction method = [this, &osHandleData](size_t failureIndex) {
        AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, mockDeviceBitfield);

        auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
        if (MemoryManagement::nonfailingAllocation == failureIndex) {
            EXPECT_NE(nullptr, graphicsAllocation);
            memoryManager->freeGraphicsMemory(graphicsAllocation);
        } else {
            EXPECT_EQ(nullptr, graphicsAllocation);
        }
    };

    injectFailures(method);
    mock->reset();
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerAndThreeOsHandlesWhenReuseCreatesAreCalledThenGraphicsAllocationsAreReturned) {
    mock->ioctlExpected.primeFdToHandle = 3;
    mock->ioctlExpected.gemWait = 3;
    mock->ioctlExpected.gemClose = 2;

    osHandle handles[] = {1u, 2u, 3u};
    size_t size = 4096u;
    GraphicsAllocation *graphicsAllocations[3];
    DrmAllocation *drmAllocation;
    BufferObject *bo;
    unsigned int expectedRefCount;

    this->mock->outputHandle = 2u;

    for (unsigned int i = 0; i < 3; ++i) {
        expectedRefCount = i < 2 ? i + 1 : 1;
        if (i == 2)
            this->mock->outputHandle = 3u;

        AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, mockDeviceBitfield);
        TestedDrmMemoryManager::OsHandleData osHandleData{handles[i]};

        graphicsAllocations[i] = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
        // Clang-tidy false positive WA
        if (graphicsAllocations[i] == nullptr) {
            ASSERT_FALSE(true);
            continue;
        }
        ASSERT_NE(nullptr, graphicsAllocations[i]);

        EXPECT_NE(nullptr, graphicsAllocations[i]->getUnderlyingBuffer());
        EXPECT_EQ(size, graphicsAllocations[i]->getUnderlyingBufferSize());
        EXPECT_EQ(this->mock->inputFd, (int)handles[i]);
        EXPECT_EQ(this->mock->setTilingHandle, 0u);

        drmAllocation = static_cast<DrmAllocation *>(graphicsAllocations[i]);
        bo = drmAllocation->getBO();
        EXPECT_EQ(bo->peekHandle(), (int)this->mock->outputHandle);
        EXPECT_NE(0llu, bo->peekAddress());
        EXPECT_EQ(expectedRefCount, bo->getRefCount());
        EXPECT_EQ(size, bo->peekSize());

        EXPECT_EQ(handles[i], graphicsAllocations[i]->peekSharedHandle());
    }

    for (const auto &it : graphicsAllocations) {
        // Clang-tidy false positive WA
        if (it != nullptr)
            memoryManager->freeGraphicsMemory(it);
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, given32BitAddressingWhenBufferFromSharedHandleAndBitnessRequiredIsCreatedThenItis32BitAllocation) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    SysCalls::lseekCalledCount = 0;
    memoryManager->setForce32BitAllocations(true);
    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    this->mock->outputHandle = 2u;

    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, mockDeviceBitfield);

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, true, false, true, nullptr);
    auto drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    EXPECT_TRUE(graphicsAllocation->is32BitAllocation());
    EXPECT_EQ(1, SysCalls::lseekCalledCount);
    auto gmmHelper = device->getGmmHelper();
    EXPECT_EQ(gmmHelper->canonize(memoryManager->getExternalHeapBaseAddress(graphicsAllocation->getRootDeviceIndex(), drmAllocation->isAllocatedInLocalMemoryPool())), drmAllocation->getGpuBaseAddress());
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, given32BitAddressingWhenBufferFromSharedHandleIsCreatedAndDoesntRequireBitnessThenItIsNot32BitAllocation) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    SysCalls::lseekCalledCount = 0;

    memoryManager->setForce32BitAllocations(true);
    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    this->mock->outputHandle = 2u;
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, mockDeviceBitfield);
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    auto drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);

    EXPECT_FALSE(graphicsAllocation->is32BitAllocation());
    EXPECT_EQ(1, SysCalls::lseekCalledCount);

    EXPECT_EQ(0llu, drmAllocation->getGpuBaseAddress());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenLimitedRangeAllocatorWhenBufferFromSharedHandleIsCreatedThenItIsLimitedRangeAllocation) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    SysCalls::lseekCalledCount = 0;

    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);
    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    this->mock->outputHandle = 2u;
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, mockDeviceBitfield);
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    EXPECT_FALSE(graphicsAllocation->is32BitAllocation());
    auto drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);

    EXPECT_EQ(0llu, drmAllocation->getGpuBaseAddress());
    EXPECT_EQ(1, SysCalls::lseekCalledCount);
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenNon32BitAddressingWhenBufferFromSharedHandleIsCreatedAndDRequireBitnessThenItIsNot32BitAllocation) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    SysCalls::lseekCalledCount = 0;

    memoryManager->setForce32BitAllocations(false);
    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    this->mock->outputHandle = 2u;
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, mockDeviceBitfield);
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, true, false, true, nullptr);
    auto drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    EXPECT_FALSE(graphicsAllocation->is32BitAllocation());
    EXPECT_EQ(1, SysCalls::lseekCalledCount);
    EXPECT_EQ(0llu, drmAllocation->getGpuBaseAddress());
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenSharedHandleWhenAllocationIsCreatedAndIoctlPrimeFdToHandleFailsThenNullPtrIsReturned) {
    mock->ioctlExpected.primeFdToHandle = 1;
    this->ioctlResExt = {mock->ioctlCnt.total, -1};
    mock->ioctlResExt = &this->ioctlResExt;

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    this->mock->outputHandle = 2u;
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, mockDeviceBitfield);
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    EXPECT_EQ(nullptr, graphicsAllocation);
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenTwoGraphicsAllocationsThatShareTheSameBufferObjectWhenTheyAreMadeResidentThenOnlyOneBoIsPassedToExec) {
    auto testedCsr = static_cast<TestedDrmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> *>(device->getDefaultEngine().commandStreamReceiver);
    mock->ioctlExpected.primeFdToHandle = 2;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.gemWait = 2;

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, mockDeviceBitfield);
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    auto graphicsAllocation2 = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);

    auto currentResidentSize = testedCsr->getResidencyAllocations().size();
    testedCsr->makeResident(*graphicsAllocation);
    testedCsr->makeResident(*graphicsAllocation2);

    EXPECT_EQ(currentResidentSize + 2, testedCsr->getResidencyAllocations().size());
    currentResidentSize = testedCsr->getResidencyAllocations().size();

    testedCsr->processResidency(testedCsr->getResidencyAllocations(), 0u);

    EXPECT_EQ(currentResidentSize - 1, testedCsr->residency.size());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
    memoryManager->freeGraphicsMemory(graphicsAllocation2);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenTwoGraphicsAllocationsThatDoesnShareTheSameBufferObjectWhenTheyAreMadeResidentThenTwoBoIsPassedToExec) {
    auto testedCsr = static_cast<TestedDrmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> *>(device->getDefaultEngine().commandStreamReceiver);
    mock->ioctlExpected.primeFdToHandle = 2;
    mock->ioctlExpected.gemClose = 2;
    mock->ioctlExpected.gemWait = 2;

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, {});
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    mock->outputHandle++;
    auto graphicsAllocation2 = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);

    auto currentResidentSize = testedCsr->getResidencyAllocations().size();
    testedCsr->makeResident(*graphicsAllocation);
    testedCsr->makeResident(*graphicsAllocation2);

    EXPECT_EQ(currentResidentSize + 2u, testedCsr->getResidencyAllocations().size());

    testedCsr->processResidency(testedCsr->getResidencyAllocations(), 0u);

    EXPECT_EQ(currentResidentSize + 2u, testedCsr->residency.size());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
    memoryManager->freeGraphicsMemory(graphicsAllocation2);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenLockUnlockIsCalledThenReturnPtr) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemSetDomain = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize});
    ASSERT_NE(nullptr, allocation);

    auto ptr = memoryManager->lockResource(allocation);
    EXPECT_NE(nullptr, ptr);

    memoryManager->unlockResource(allocation);
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenDrmMemoryManagerAndResidentNeededbeforeLockWhenLockIsCalledThenverifyAllocationIsResident) {
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.gemMmapOffset = 1;
    mock->ioctlExpected.gemCreateExt = 1;

    auto mockIoctlHelper = new MockIoctlHelper(*mock);
    mockIoctlHelper->makeResidentBeforeLockNeededResult = true;

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(rootDeviceIndex));
    drm.ioctlHelper.reset(mockIoctlHelper);

    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface.reset(new DrmMemoryOperationsHandlerBind(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex].get(), 0));
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, true, MemoryConstants::pageSize, AllocationType::buffer});
    ASSERT_NE(nullptr, allocation);

    auto ptr = memoryManager->lockResource(allocation);
    EXPECT_NE(nullptr, ptr);

    auto osContext = device->getDefaultEngine().osContext;
    EXPECT_TRUE(allocation->isAlwaysResident(osContext->getContextId()));

    memoryManager->unlockResource(allocation);
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenLockUnlockIsCalledOnAllocationWithCpuPtrThenReturnCpuPtrAndSetCpuDomain) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemSetDomain = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize});
    ASSERT_NE(nullptr, allocation);
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());

    auto ptr = memoryManager->lockResource(allocation);
    EXPECT_EQ(allocation->getUnderlyingBuffer(), ptr);

    // check DRM_IOCTL_I915_GEM_SET_DOMAIN input params
    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    EXPECT_EQ((uint32_t)drmAllocation->getBO()->peekHandle(), mock->setDomainHandle);
    EXPECT_EQ((uint32_t)I915_GEM_DOMAIN_CPU, mock->setDomainReadDomains);
    EXPECT_EQ(0u, mock->setDomainWriteDomain);

    memoryManager->unlockResource(allocation);
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenLockUnlockIsCalledOnAllocationWithoutCpuPtrThenReturnLockedPtrAndSetCpuDomain) {
    mock->ioctlExpected.gemCreate = 1;
    mock->ioctlExpected.gemMmapOffset = 1;
    mock->ioctlExpected.gemSetDomain = 0;
    mock->ioctlExpected.gemSetTiling = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::image2D;
    imgDesc.imageWidth = 512;
    imgDesc.imageHeight = 512;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    imgInfo.size = 4096u;
    imgInfo.rowPitch = 512u;

    AllocationData allocationData;
    allocationData.imgInfo = &imgInfo;
    allocationData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryForImage(allocationData);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(nullptr, allocation->getUnderlyingBuffer());

    auto ptr = memoryManager->lockResource(allocation);
    EXPECT_NE(nullptr, ptr);

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    EXPECT_NE(nullptr, drmAllocation->getBO()->peekLockedAddress());

    // check DRM_IOCTL_I915_GEM_MMAP_OFFSET input params
    EXPECT_EQ((uint32_t)drmAllocation->getBO()->peekHandle(), mock->mmapOffsetHandle);
    EXPECT_EQ(0u, mock->mmapOffsetPad);
    auto expectedMmapOffset = static_cast<uint32_t>(I915_MMAP_OFFSET_WC);
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    if (BufferObject::BOType::nonCoherent != drmAllocation->getBO()->peekBOType() &&
        productHelper.useGemCreateExtInAllocateMemoryByKMD()) {
        expectedMmapOffset = static_cast<uint32_t>(I915_MMAP_OFFSET_WB);
    }
    EXPECT_EQ(expectedMmapOffset, mock->mmapOffsetFlags);

    memoryManager->unlockResource(allocation);
    EXPECT_EQ(nullptr, drmAllocation->getBO()->peekLockedAddress());

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenLockUnlockIsCalledOnNullAllocationThenReturnNullPtr) {
    GraphicsAllocation *allocation = nullptr;

    auto ptr = memoryManager->lockResource(allocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenLockUnlockIsCalledOnAllocationWithoutBufferObjectThenReturnNullPtr) {
    DrmAllocation drmAllocation(rootDeviceIndex, 1u /*num gmms*/, AllocationType::unknown, nullptr, nullptr, 0, static_cast<osHandle>(0u), MemoryPool::memoryNull);
    EXPECT_EQ(nullptr, drmAllocation.getBO());

    auto ptr = memoryManager->lockResource(&drmAllocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(&drmAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenLockUnlockIsCalledButFailsOnIoctlMmapThenReturnNullPtr) {
    mock->ioctlExpected.gemMmapOffset = 1;
    this->ioctlResExt = {mock->ioctlCnt.total, -1};
    mock->ioctlResExt = &ioctlResExt;

    BufferObject bo(rootDeviceIndex, mock, 3, 1, 0, 1);
    DrmAllocation drmAllocation(rootDeviceIndex, 1u /*num gmms*/, AllocationType::unknown, &bo, nullptr, 0u, static_cast<osHandle>(0u), MemoryPool::memoryNull);
    EXPECT_NE(nullptr, drmAllocation.getBO());

    auto ptr = memoryManager->lockResource(&drmAllocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(&drmAllocation);
    mock->ioctlResExt = &mock->none;
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenUnlockResourceIsCalledOnAllocationInLocalMemoryThenRedirectToUnlockResourceInLocalMemory) {
    struct DrmMemoryManagerToTestUnlockResource : public DrmMemoryManager {
        using DrmMemoryManager::unlockResourceImpl;
        DrmMemoryManagerToTestUnlockResource(ExecutionEnvironment &executionEnvironment, bool localMemoryEnabled, size_t lockableLocalMemorySize)
            : DrmMemoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment) {
        }
        void unlockBufferObject(BufferObject *bo) override {
            unlockResourceInLocalMemoryImplParam.bo = bo;
            unlockResourceInLocalMemoryImplParam.called = true;
        }
        struct UnlockResourceInLocalMemoryImplParamType {
            BufferObject *bo = nullptr;
            bool called = false;
        } unlockResourceInLocalMemoryImplParam;
    };

    DrmMemoryManagerToTestUnlockResource drmMemoryManager(*executionEnvironment, true, MemoryConstants::pageSize);

    auto drmMock = DrmMockCustom::create(*executionEnvironment->rootDeviceEnvironments[1]);
    auto bo = new BufferObject(rootDeviceIndex, drmMock.get(), 3, 1, 0, 1);
    auto drmAllocation = new DrmAllocation(rootDeviceIndex, 1u /*num gmms*/, AllocationType::unknown, bo, nullptr, 0u, static_cast<osHandle>(0u), MemoryPool::localMemory);

    drmMemoryManager.unlockResourceImpl(*drmAllocation);
    EXPECT_TRUE(drmMemoryManager.unlockResourceInLocalMemoryImplParam.called);
    EXPECT_EQ(bo, drmMemoryManager.unlockResourceInLocalMemoryImplParam.bo);

    drmMemoryManager.freeGraphicsMemory(drmAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetDomainCpuIsCalledOnAllocationWithoutBufferObjectThenReturnFalse) {
    DrmAllocation drmAllocation(rootDeviceIndex, 1u /*num gmms*/, AllocationType::unknown, nullptr, nullptr, 0, static_cast<osHandle>(0u), MemoryPool::memoryNull);
    EXPECT_EQ(nullptr, drmAllocation.getBO());

    auto success = memoryManager->setDomainCpu(drmAllocation, false);
    EXPECT_FALSE(success);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetDomainCpuIsCalledButFailsOnIoctlSetDomainThenReturnFalse) {
    mock->ioctlExpected.gemSetDomain = 1;
    this->ioctlResExt = {mock->ioctlCnt.total, -1};
    mock->ioctlResExt = &ioctlResExt;

    auto drmMock = DrmMockCustom::create(*executionEnvironment->rootDeviceEnvironments[0]);
    BufferObject bo(0u, drmMock.get(), 3, 1, 0, 1);
    DrmAllocation drmAllocation(rootDeviceIndex, 1u /*num gmms*/, AllocationType::unknown, &bo, nullptr, 0u, static_cast<osHandle>(0u), MemoryPool::memoryNull);
    EXPECT_NE(nullptr, drmAllocation.getBO());

    auto success = memoryManager->setDomainCpu(drmAllocation, false);
    EXPECT_FALSE(success);
    mock->ioctlResExt = &mock->none;
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetDomainCpuIsCalledOnAllocationThenReturnSetWriteDomain) {
    mock->ioctlExpected.gemSetDomain = 1;

    auto drmMock = DrmMockCustom::create(*executionEnvironment->rootDeviceEnvironments[0]);
    BufferObject bo(rootDeviceIndex, drmMock.get(), 3, 1, 0, 1);
    DrmAllocation drmAllocation(rootDeviceIndex, 1u /*num gmms*/, AllocationType::unknown, &bo, nullptr, 0u, static_cast<osHandle>(0u), MemoryPool::memoryNull);
    EXPECT_NE(nullptr, drmAllocation.getBO());

    auto success = memoryManager->setDomainCpu(drmAllocation, true);
    EXPECT_TRUE(success);

    // check DRM_IOCTL_I915_GEM_SET_DOMAIN input params
    EXPECT_EQ((uint32_t)drmAllocation.getBO()->peekHandle(), mock->setDomainHandle);
    EXPECT_EQ((uint32_t)I915_GEM_DOMAIN_CPU, mock->setDomainReadDomains);
    EXPECT_EQ((uint32_t)I915_GEM_DOMAIN_CPU, mock->setDomainWriteDomain);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerAndUnifiedAuxCapableAllocationWhenMappingThenReturnFalse) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = new Gmm(rootDeviceEnvironment->getGmmHelper(), nullptr, 123, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize});
    allocation->setDefaultGmm(gmm);

    auto mockGmmRes = static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    mockGmmRes->setUnifiedAuxTranslationCapable();

    EXPECT_FALSE(memoryManager->mapAuxGpuVA(allocation));

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenSharedAllocationWithSmallerThenRealSizeWhenCreateIsCalledThenRealSizeIsUsed) {
    unsigned int realSize = 64 * 1024;
    VariableBackup<decltype(SysCalls::lseekReturn)> lseekBackup(&SysCalls::lseekReturn, realSize);
    SysCalls::lseekCalledCount = 0;
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, {});

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);

    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(realSize, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(this->mock->inputFd, (int)osHandleData.handle);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    auto bo = drmAllocation->getBO();
    EXPECT_EQ(bo->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, bo->peekAddress());
    EXPECT_EQ(1u, bo->getRefCount());
    EXPECT_EQ(realSize, bo->peekSize());
    EXPECT_EQ(1, SysCalls::lseekCalledCount);
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedForInternalAllocationWithNoPointerThenAllocationFromInternalHeapIsReturned) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    auto bufferSize = MemoryConstants::pageSize;
    void *ptr = nullptr;
    auto drmAllocation = static_cast<DrmAllocation *>(memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, bufferSize, ptr, AllocationType::internalHeap));
    ASSERT_NE(nullptr, drmAllocation);

    EXPECT_NE(nullptr, drmAllocation->getUnderlyingBuffer());
    EXPECT_EQ(bufferSize, drmAllocation->getUnderlyingBufferSize());

    EXPECT_TRUE(drmAllocation->is32BitAllocation());

    auto gpuPtr = drmAllocation->getGpuAddress();
    auto gmmHelper = device->getGmmHelper();
    auto heapBase = gmmHelper->canonize(memoryManager->getInternalHeapBaseAddress(drmAllocation->getRootDeviceIndex(), drmAllocation->isAllocatedInLocalMemoryPool()));
    auto heapSize = 4 * MemoryConstants::gigaByte;

    EXPECT_GE(gpuPtr, heapBase);
    EXPECT_LE(gpuPtr, heapBase + heapSize);

    EXPECT_EQ(drmAllocation->getGpuBaseAddress(), heapBase);

    memoryManager->freeGraphicsMemory(drmAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenLimitedRangeAllocatorWhenAskedForInternalAllocationWithNoPointerThenAllocationFromInternalHeapIsReturned) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    auto bufferSize = MemoryConstants::pageSize;
    void *ptr = nullptr;
    auto drmAllocation = static_cast<DrmAllocation *>(memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, bufferSize, ptr, AllocationType::internalHeap));
    ASSERT_NE(nullptr, drmAllocation);

    EXPECT_NE(nullptr, drmAllocation->getUnderlyingBuffer());
    EXPECT_EQ(bufferSize, drmAllocation->getUnderlyingBufferSize());

    ASSERT_NE(nullptr, drmAllocation->getDriverAllocatedCpuPtr());
    EXPECT_EQ(drmAllocation->getDriverAllocatedCpuPtr(), drmAllocation->getUnderlyingBuffer());

    EXPECT_TRUE(drmAllocation->is32BitAllocation());

    auto gpuPtr = drmAllocation->getGpuAddress();
    auto gmmHelper = device->getGmmHelper();
    auto heapBase = gmmHelper->canonize(memoryManager->getInternalHeapBaseAddress(drmAllocation->getRootDeviceIndex(), drmAllocation->isAllocatedInLocalMemoryPool()));
    auto heapSize = 4 * MemoryConstants::gigaByte;

    EXPECT_GE(gpuPtr, heapBase);
    EXPECT_LE(gpuPtr, heapBase + heapSize);

    EXPECT_EQ(drmAllocation->getGpuBaseAddress(), heapBase);

    memoryManager->freeGraphicsMemory(drmAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenLimitedRangeAllocatorWhenAskedForExternalAllocationWithNoPointerThenAllocationFromInternalHeapIsReturned) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    memoryManager->setForce32BitAllocations(true);
    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    auto bufferSize = MemoryConstants::pageSize;
    void *ptr = nullptr;
    auto drmAllocation = static_cast<DrmAllocation *>(memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, bufferSize, ptr, AllocationType::buffer));
    ASSERT_NE(nullptr, drmAllocation);

    EXPECT_NE(nullptr, drmAllocation->getUnderlyingBuffer());
    EXPECT_TRUE(drmAllocation->is32BitAllocation());

    memoryManager->freeGraphicsMemory(drmAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenLimitedRangeAllocatorWhenAskedForInternalAllocationWithNoPointerAndHugeBufferSizeThenAllocationFromInternalHeapFailed) {
    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    auto bufferSize = 128 * MemoryConstants::megaByte + 4 * MemoryConstants::pageSize;
    void *ptr = nullptr;
    auto drmAllocation = static_cast<DrmAllocation *>(memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, bufferSize, ptr, AllocationType::internalHeap));
    ASSERT_EQ(nullptr, drmAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedForInternalAllocationWithPointerThenAllocationFromInternalHeapIsReturned) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    auto bufferSize = MemoryConstants::pageSize;
    void *ptr = reinterpret_cast<void *>(0x100000);
    auto drmAllocation = static_cast<DrmAllocation *>(memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, bufferSize, ptr, AllocationType::internalHeap));
    ASSERT_NE(nullptr, drmAllocation);

    EXPECT_NE(nullptr, drmAllocation->getUnderlyingBuffer());
    EXPECT_EQ(ptr, drmAllocation->getUnderlyingBuffer());
    EXPECT_EQ(bufferSize, drmAllocation->getUnderlyingBufferSize());

    EXPECT_TRUE(drmAllocation->is32BitAllocation());

    auto gpuPtr = drmAllocation->getGpuAddress();
    auto gmmHelper = device->getGmmHelper();
    auto heapBase = gmmHelper->canonize(memoryManager->getInternalHeapBaseAddress(drmAllocation->getRootDeviceIndex(), drmAllocation->isAllocatedInLocalMemoryPool()));
    auto heapSize = 4 * MemoryConstants::gigaByte;

    EXPECT_GE(gpuPtr, heapBase);
    EXPECT_LE(gpuPtr, heapBase + heapSize);

    EXPECT_EQ(drmAllocation->getGpuBaseAddress(), heapBase);

    memoryManager->freeGraphicsMemory(drmAllocation);
}

using DrmMemoryManagerUSMHostAllocationTests = Test<DrmMemoryManagerFixture>;

HWTEST_TEMPLATED_F(DrmMemoryManagerUSMHostAllocationTests, givenCallToAllocateGraphicsMemoryWithAlignmentWithIsHostUsmAllocationSetToFalseThenNewHostPointerIsUsedAndAllocationIsCreatedSuccesfully) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemClose = 1;

    AllocationData allocationData;
    allocationData.size = 16384;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    auto alloc = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);
    EXPECT_NE(nullptr, alloc);
    memoryManager->freeGraphicsMemoryImpl(alloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerUSMHostAllocationTests, givenCallToAllocateGraphicsMemoryWithAlignmentWithIsHostUsmAllocationSetToTrueThenGpuAddressIsNotFromGfxPartition) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemClose = 1;

    AllocationData allocationData;
    allocationData.size = 16384;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isUSMHostAllocation = true;
    allocationData.type = AllocationType::svmCpu;
    auto alloc = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);

    EXPECT_NE(nullptr, alloc);
    EXPECT_EQ(reinterpret_cast<uint64_t>(alloc->getUnderlyingBuffer()), alloc->getGpuAddress());

    memoryManager->freeGraphicsMemoryImpl(alloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerUSMHostAllocationTests, givenMmapPtrWhenFreeGraphicsMemoryImplThenPtrIsDeallocated) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemClose = 1;

    const size_t size = 16384;
    AllocationData allocationData;
    allocationData.size = size;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    auto alloc = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);
    EXPECT_NE(nullptr, alloc);

    auto ptr = memoryManager->mmapFunction(0, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS, -1, 0);
    static_cast<DrmAllocation *>(alloc)->setMmapPtr(ptr);
    static_cast<DrmAllocation *>(alloc)->setMmapSize(size);

    memoryManager->freeGraphicsMemoryImpl(alloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerUSMHostAllocationTests,
                   givenCallToallocateGraphicsMemoryWithAlignmentWithisHostUSMAllocationSetToTrueThenTheExistingHostPointerIsUsedAndAllocationIsCreatedSuccesfully) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemClose = 1;

    AllocationData allocationData;

    size_t allocSize = 16384;
    void *hostPtr = alignedMalloc(allocSize, 0);

    allocationData.size = allocSize;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isUSMHostAllocation = true;
    allocationData.hostPtr = hostPtr;
    NEO::GraphicsAllocation *alloc = memoryManager->allocateGraphicsMemory(allocationData);
    EXPECT_NE(nullptr, alloc);
    EXPECT_EQ(hostPtr, alloc->getUnderlyingBuffer());

    memoryManager->freeGraphicsMemoryImpl(alloc);
    alignedFree(hostPtr);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDefaultDrmMemoryManagerWhenAskedForAlignedMallocRestrictionsThenNullPtrIsReturned) {
    EXPECT_EQ(nullptr, memoryManager->getAlignedMallocRestrictions());
}

TEST_F(DrmMemoryManagerBasic, givenDefaultMemoryManagerWhenItIsCreatedThenAsyncDeleterEnabledIsTrue) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useGemCloseWorker = true;
    DrmMemoryManager memoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, true, executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    memoryManager.commonCleanup();
}

TEST_F(DrmMemoryManagerBasic, givenDisabledGemCloseWorkerWhenMemoryManagerIsCreatedThenNoGemCloseWorker) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useGemCloseWorker = true;
    DebugManagerStateRestore dbgStateRestore;
    debugManager.flags.EnableGemCloseWorker.set(0u);

    TestedDrmMemoryManager memoryManager(true, true, true, executionEnvironment);

    EXPECT_EQ(memoryManager.peekGemCloseWorker(), nullptr);
}

TEST_F(DrmMemoryManagerBasic, givenEnabledGemCloseWorkerWhenMemoryManagerIsCreatedThenGemCloseWorker) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useGemCloseWorker = true;
    DebugManagerStateRestore dbgStateRestore;
    debugManager.flags.EnableGemCloseWorker.set(1u);

    TestedDrmMemoryManager memoryManager(true, true, true, executionEnvironment);

    EXPECT_NE(memoryManager.peekGemCloseWorker(), nullptr);
}

TEST_F(DrmMemoryManagerBasic, givenDefaultGemCloseWorkerWhenMemoryManagerIsCreatedThenGemCloseWorker) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useGemCloseWorker = true;
    MemoryManagerCreate<DrmMemoryManager> memoryManager(false, false, GemCloseWorkerMode::gemCloseWorkerActive, false, false, executionEnvironment);

    EXPECT_NE(memoryManager.peekGemCloseWorker(), nullptr);
}

TEST_F(DrmMemoryManagerBasic, givenEnabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsFalseAndDeleterIsNullptr) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useGemCloseWorker = true;
    DebugManagerStateRestore dbgStateRestore;
    debugManager.flags.EnableDeferredDeleter.set(true);
    DrmMemoryManager memoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, true, executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    memoryManager.commonCleanup();
}

TEST_F(DrmMemoryManagerBasic, givenDisabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsFalseAndDeleterIsNullptr) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useGemCloseWorker = true;
    DebugManagerStateRestore dbgStateRestore;
    debugManager.flags.EnableDeferredDeleter.set(false);
    DrmMemoryManager memoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, true, executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    memoryManager.commonCleanup();
}

TEST_F(DrmMemoryManagerBasic, givenWorkerToCloseWhenCommonCleanupIsCalledThenClosingIsBlocking) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useGemCloseWorker = true;
    MockDrmMemoryManager memoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, true, executionEnvironment);
    memoryManager.gemCloseWorker.reset(new MockDrmGemCloseWorker(memoryManager));
    auto pWorker = static_cast<MockDrmGemCloseWorker *>(memoryManager.gemCloseWorker.get());

    memoryManager.commonCleanup();
    EXPECT_TRUE(pWorker->wasBlocking);
}

TEST_F(DrmMemoryManagerBasic, givenDefaultDrmMemoryManagerWhenItIsQueriedForInternalHeapBaseThenInternalHeapBaseIsReturned) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    true,
                                                                                                    true,
                                                                                                    executionEnvironment));
    auto heapBase = memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapInternalDeviceMemory);
    EXPECT_EQ(heapBase, memoryManager->getInternalHeapBaseAddress(rootDeviceIndex, true));
}

TEST_F(DrmMemoryManagerBasic, givenMemoryManagerWithEnabledHostMemoryValidationWhenFeatureIsQueriedThenTrueIsReturned) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    executionEnvironment));
    ASSERT_NE(nullptr, memoryManager.get());
    EXPECT_TRUE(memoryManager->isValidateHostMemoryEnabled());
}

TEST_F(DrmMemoryManagerBasic, givenMemoryManagerWithDisabledHostMemoryValidationWhenFeatureIsQueriedThenFalseIsReturned) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    false,
                                                                                                    executionEnvironment));
    ASSERT_NE(nullptr, memoryManager.get());
    EXPECT_FALSE(memoryManager->isValidateHostMemoryEnabled());
}

TEST_F(DrmMemoryManagerBasic, givenEnabledHostMemoryValidationWhenMemoryManagerIsCreatedThenPinBBIsCreated) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    executionEnvironment));
    ASSERT_NE(nullptr, memoryManager.get());
    ASSERT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);
}

TEST_F(DrmMemoryManagerBasic, givenEnabledHostMemoryValidationAndForcePinWhenMemoryManagerIsCreatedThenPinBBIsCreated) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    true,
                                                                                                    true,
                                                                                                    executionEnvironment));
    ASSERT_NE(nullptr, memoryManager.get());
    ASSERT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);
}

TEST_F(DrmMemoryManagerBasic, givenMemoryManagerWhenAllocateGraphicsMemoryIsCalledThenMemoryPoolIsSystem4KBPages) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(
        false,
        false,
        true,
        executionEnvironment));

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, MemoryConstants::pageSize, false));
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::system4KBPages, allocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenMemoryManagerWhenAllocateGraphicsMemoryWithPtrIsCalledThenMemoryPoolIsSystem4KBPages) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    *executionEnvironment));
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = 4096u;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), false, size}, ptr);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::system4KBPages, allocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerBasic, givenMemoryManagerWhenAllocate32BitGraphicsMemoryWithPtrIsCalledThenMemoryPoolIsSystem4KBPagesWith32BitGpuAddressing) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    executionEnvironment));

    memoryManager->setForce32BitAllocations(true);

    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize;

    auto allocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, size, ptr, AllocationType::buffer);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::system4KBPagesWith32BitGpuAddressing, allocation->getMemoryPool());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerBasic, givenMemoryManagerWhenCreateAllocationFromHandleIsCalledThenMemoryPoolIsSystemCpuInaccessible) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    executionEnvironment));
    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, {});
    auto allocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::systemCpuInaccessible, allocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerBasic, givenMemoryManagerWhenCreateAllocationFromHandleIsCalledThenAllocationIsNotRegisteredNorUnregistered) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    executionEnvironment));
    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, {});

    auto allocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    EXPECT_NE(nullptr, allocation);
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(0u, memoryManager->registerSysMemAllocCalled);
    EXPECT_EQ(0u, memoryManager->registerLocalMemAllocCalled);
    EXPECT_EQ(0u, memoryManager->unregisterAllocationCalled);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDisabledForcePinAndEnabledValidateHostMemoryWhenPinBBAllocationFailsThenUnrecoverableIsCalled) {
    this->mock = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());
    this->mock->reset();
    this->mock->ioctlRes = -1;
    this->mock->ioctlExpected.gemUserptr = 1;
    EXPECT_THROW(
        {
            std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false,
                                                                                             false,
                                                                                             true,
                                                                                             *executionEnvironment));
            EXPECT_NE(nullptr, memoryManager.get());
        },
        std::exception);
    this->mock->ioctlRes = 0;
    this->mock->ioctlExpected.contextDestroy = 0;
    this->mock->testIoctls();
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDisabledForcePinAndEnabledValidateHostMemoryWhenPopulateOsHandlesIsCalledThenHostMemoryIsValidated) {

    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager.get());
    ASSERT_NE(nullptr, memoryManager->pinBBs[device->getRootDeviceIndex()]);

    mock->reset();
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.execbuffer2 = 1; // for pinning - host memory validation

    OsHandleStorage handleStorage;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 4096;
    auto result = memoryManager->populateOsHandles(handleStorage, rootDeviceIndex);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, result);

    mock->testIoctls();

    EXPECT_NE(nullptr, handleStorage.fragmentStorageData[0].osHandleStorage);
    handleStorage.fragmentStorageData[0].freeTheFragment = true;

    memoryManager->cleanOsHandles(handleStorage, rootDeviceIndex);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDisabledForcePinAndEnabledValidateHostMemoryWhenPopulateOsHandlesIsCalledWithFirstFragmentAlreadyAllocatedThenNewBosAreValidated) {

    class PinBufferObject : public BufferObject {
      public:
        PinBufferObject(uint32_t rootDeviceIndex, Drm *drm) : BufferObject(rootDeviceIndex, drm, 3, 1, 0, 1) {
        }

        int validateHostPtr(BufferObject *const boToPin[], size_t numberOfBos, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId) override {
            for (size_t i = 0; i < numberOfBos; i++) {
                pinnedBoArray[i] = boToPin[i];
            }
            numberOfBosPinned = numberOfBos;
            return 0;
        }
        BufferObject *pinnedBoArray[5];
        size_t numberOfBosPinned;
    };

    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager.get());
    ASSERT_NE(nullptr, memoryManager->pinBBs[device->getRootDeviceIndex()]);

    PinBufferObject *pinBB = new PinBufferObject(rootDeviceIndex, this->mock);
    memoryManager->injectPinBB(pinBB, rootDeviceIndex);

    mock->reset();
    mock->ioctlExpected.gemUserptr = 2;
    mock->ioctlExpected.execbuffer2 = 0; // pinning for host memory validation is mocked

    OsHandleStorage handleStorage;
    OsHandleLinux handle1;
    handleStorage.fragmentStorageData[0].osHandleStorage = &handle1;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 4096;

    handleStorage.fragmentStorageData[1].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[1].cpuPtr = reinterpret_cast<void *>(0x2000);
    handleStorage.fragmentStorageData[1].fragmentSize = 8192;

    handleStorage.fragmentStorageData[2].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[2].cpuPtr = reinterpret_cast<void *>(0x4000);
    handleStorage.fragmentStorageData[2].fragmentSize = 4096;

    auto result = memoryManager->populateOsHandles(handleStorage, rootDeviceIndex);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, result);

    mock->testIoctls();

    EXPECT_NE(nullptr, handleStorage.fragmentStorageData[0].osHandleStorage);
    EXPECT_NE(nullptr, handleStorage.fragmentStorageData[1].osHandleStorage);
    EXPECT_NE(nullptr, handleStorage.fragmentStorageData[2].osHandleStorage);

    EXPECT_EQ(static_cast<OsHandleLinux *>(handleStorage.fragmentStorageData[1].osHandleStorage)->bo, pinBB->pinnedBoArray[0]);
    EXPECT_EQ(static_cast<OsHandleLinux *>(handleStorage.fragmentStorageData[2].osHandleStorage)->bo, pinBB->pinnedBoArray[1]);

    handleStorage.fragmentStorageData[0].freeTheFragment = false;
    handleStorage.fragmentStorageData[1].freeTheFragment = true;
    handleStorage.fragmentStorageData[2].freeTheFragment = true;

    memoryManager->cleanOsHandles(handleStorage, rootDeviceIndex);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenValidateHostPtrMemoryEnabledWhenHostPtrAllocationIsCreatedWithoutForcingPinThenBufferObjectIsPinned) {
    mock->ioctlExpected.gemUserptr = 2;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 2;

    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, true, true, *executionEnvironment));
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager->pinBBs[device->getRootDeviceIndex()]);

    size_t size = 10 * MemoryConstants::megaByte;
    void *ptr = ::alignedMalloc(size, 4096);
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), false, size}, ptr));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
    ::alignedFree(ptr);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledHostMemoryValidationWhenValidHostPointerIsPassedToPopulateThenSuccessIsReturned) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    *executionEnvironment));

    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    storage.fragmentStorageData[0].fragmentSize = 1;
    auto result = memoryManager->populateOsHandles(storage, rootDeviceIndex);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, result);

    EXPECT_NE(nullptr, storage.fragmentStorageData[0].osHandleStorage);
    storage.fragmentStorageData[0].freeTheFragment = true;
    memoryManager->cleanOsHandles(storage, rootDeviceIndex);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenForcePinAndHostMemoryValidationEnabledWhenSmallAllocationIsCreatedThenBufferObjectIsPinned) {
    mock->ioctlExpected.gemUserptr = 2;  // 1 pinBB, 1 small allocation
    mock->ioctlExpected.execbuffer2 = 1; // pinning
    mock->ioctlExpected.gemWait = 1;     // in freeGraphicsAllocation
    mock->ioctlExpected.gemClose = 2;    // 1 pinBB, 1 small allocation

    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, true, true, *executionEnvironment));
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);

    // one page is too small for early pinning but pinning is used for host memory validation
    allocationData.size = 4 * 1024;
    allocationData.hostPtr = ::alignedMalloc(allocationData.size, 4096);
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
    ::alignedFree(const_cast<void *>(allocationData.hostPtr));
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenForcePinAndHostMemoryValidationEnabledThenPinnedBufferObjectGpuAddressWithinDeviceGpuAddressSpace) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemClose = 1;

    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, true, true, *executionEnvironment));

    auto bo = memoryManager->pinBBs[rootDeviceIndex];

    ASSERT_NE(nullptr, bo);

    EXPECT_LT(bo->peekAddress(), defaultHwInfo->capabilityTable.gpuAddressSpace);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenForcePinAndHostMemoryValidationEnabledThenPinnedBufferObjectWrittenWithMIBBENDAndNOOP) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemClose = 1;

    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, true, true, *executionEnvironment));

    EXPECT_NE(0ul, memoryManager->memoryForPinBBs.size());
    ASSERT_NE(nullptr, memoryManager->memoryForPinBBs[rootDeviceIndex]);

    uint32_t *buffer = reinterpret_cast<uint32_t *>(memoryManager->memoryForPinBBs[rootDeviceIndex]);
    uint32_t bbEnd = 0x05000000;
    EXPECT_EQ(bbEnd, buffer[0]);
    EXPECT_EQ(0ul, buffer[1]);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenForcePinAllowedAndNoPinBBInMemoryManagerWhenAllocationWithForcePinFlagTrueIsCreatedThenAllocationIsNotPinned) {
    mock->ioctlExpected.gemUserptr = 2;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlRes = -1;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, true, false, *executionEnvironment));
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    EXPECT_EQ(nullptr, memoryManager->pinBBs[rootDeviceIndex]);
    mock->ioctlRes = 0;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, MemoryConstants::pageSize, true));
    EXPECT_NE(nullptr, allocation);
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenNullptrOrZeroSizeWhenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledThenAllocationIsNotCreated) {
    allocationData.size = 0;
    allocationData.hostPtr = nullptr;
    EXPECT_FALSE(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));

    allocationData.size = 100;
    allocationData.hostPtr = nullptr;
    EXPECT_FALSE(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));

    allocationData.size = 0;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x12345);
    EXPECT_FALSE(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));
}

TEST_F(DrmMemoryManagerBasic, givenDrmMemoryManagerWhenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledWithNotAlignedPtrIsPassedThenAllocationIsCreated) {
    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    allocationData.size = 13;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x5001);
    allocationData.rootDeviceIndex = rootDeviceIndex;
    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);

    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0x5001u, reinterpret_cast<uint64_t>(allocation->getUnderlyingBuffer()));
    EXPECT_EQ(13u, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(1u, allocation->getAllocationOffset());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerBasic, givenDrmMemoryManagerWhenAllocateGraphicsMemoryForNonSvmHostPtrThenObjectAlignedSizeIsUsedByAllocUserPtrWhenBiggerSizeAllocatedInHeap) {
    AllocationData allocationData;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    allocationData.size = 4 * MemoryConstants::megaByte + 16 * 1024;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x10000000);
    auto allocation0 = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));

    allocationData.hostPtr = reinterpret_cast<const void *>(0x20000000);
    auto allocation1 = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);

    memoryManager->freeGraphicsMemory(allocation0);

    allocationData.size = 4 * MemoryConstants::megaByte + 12 * 1024;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x30000000);
    allocation0 = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));

    EXPECT_EQ(static_cast<uint64_t>(allocation0->getBO()->peekSize()), 4 * MemoryConstants::megaByte + 12 * 1024);

    memoryManager->freeGraphicsMemory(allocation0);
    memoryManager->freeGraphicsMemory(allocation1);
}

TEST_F(DrmMemoryManagerBasic, givenDrmMemoryManagerWithLimitedRangeWhenAllocateGraphicsMemoryForNonSvmHostPtrThenAcquireGpuRangeCalled) {
    AllocationData allocationData;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));
    memoryManager->isLimitedRangeCallBase = false;
    memoryManager->isLimitedRangeResult = true;
    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    allocationData.size = 2 * MemoryConstants::kiloByte;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x1234);
    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));

    EXPECT_EQ(memoryManager->acquireGpuRangeCalledTimes, 1u);
    EXPECT_EQ(memoryManager->acquireGpuRangeWithCustomAlignmenCalledTimes, 0u);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerBasic, givenDrmMemoryManagerWithoutLimitedRangeWhenAllocateGraphicsMemoryForNonSvmHostPtrThenAcquireGpuRangeWithCustomRangeCalled) {
    AllocationData allocationData;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));
    memoryManager->isLimitedRangeCallBase = false;
    memoryManager->isLimitedRangeResult = false;

    allocationData.size = 2 * MemoryConstants::kiloByte;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x1234);
    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));

    EXPECT_EQ(memoryManager->acquireGpuRangeCalledTimes, 0u);
    EXPECT_EQ(memoryManager->acquireGpuRangeWithCustomAlignmenCalledTimes, 1u);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerBasic, givenDrmMemoryManagerWithoutLimitedRangeWhenAllocateGraphicsMemoryForNonSvmHostPtrThen2MBAlignmentUsed) {
    AllocationData allocationData;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));
    memoryManager->isLimitedRangeCallBase = false;
    memoryManager->isLimitedRangeResult = false;

    allocationData.size = 2 * MemoryConstants::kiloByte;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x1234);
    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));

    EXPECT_EQ(memoryManager->acquireGpuRangeWithCustomAlignmenPassedAlignment, MemoryConstants::pageSize2M);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerBasic, givenDrmMemoryManagerWhenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledButAllocationFailedThenNullPtrReturned) {
    AllocationData allocationData;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    allocationData.size = 64 * MemoryConstants::gigaByte;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x100000000000);
    EXPECT_FALSE(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));
}

TEST_F(DrmMemoryManagerBasic, givenDrmMemoryManagerWhenAllocateGraphicsMemoryForNonSvmHostPtrFailsThenNullPtrReturnedAndAllocationIsNotRegistered) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));
    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);
    MockAllocationProperties properties(0u, 64 * MemoryConstants::gigaByte);

    auto ptr = reinterpret_cast<const void *>(0x100000000000);
    EXPECT_FALSE(memoryManager->allocateGraphicsMemoryInPreferredPool(properties, ptr));
    EXPECT_EQ(memoryManager->getSysMemAllocs().size(), 0u);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledWithHostPtrIsPassedAndWhenAllocUserptrFailsThenFails) {
    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    mock->ioctlExpected.gemUserptr = 1;
    this->ioctlResExt = {mock->ioctlCnt.total, -1};
    mock->ioctlResExt = &ioctlResExt;

    allocationData.size = 10;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x1000);
    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);

    EXPECT_EQ(nullptr, allocation);
    mock->ioctlResExt = &mock->none;
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenForcePinNotAllowedAndHostMemoryValidationEnabledWhenAllocationIsCreatedThenBufferObjectIsPinnedOnlyOnce) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    mock->reset();
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.execbuffer2 = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.gemWait = 1;

    AllocationData allocationData;
    allocationData.size = 4 * 1024;
    allocationData.hostPtr = ::alignedMalloc(allocationData.size, 4096);
    allocationData.flags.forcePin = true;
    allocationData.rootDeviceIndex = device->getRootDeviceIndex();
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
    mock->testIoctls();

    ::alignedFree(const_cast<void *>(allocationData.hostPtr));
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenForcePinNotAllowedAndHostMemoryValidationDisabledWhenAllocationIsCreatedThenBufferObjectIsNotPinned) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false, false, false, *executionEnvironment));
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    mock->reset();
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.gemWait = 1;

    AllocationData allocationData;
    allocationData.size = 10 * MemoryConstants::megaByte; // bigger than threshold
    allocationData.hostPtr = ::alignedMalloc(allocationData.size, 4096);
    allocationData.flags.forcePin = true;
    allocationData.rootDeviceIndex = device->getRootDeviceIndex();
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
    mock->testIoctls();

    ::alignedFree(const_cast<void *>(allocationData.hostPtr));
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledValidateHostMemoryWhenReadOnlyPointerCausesPinningFailWithEfaultThenPopulateOsHandlesMarksFragmentsToFree) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager.get());
    ASSERT_NE(nullptr, memoryManager->pinBBs[device->getRootDeviceIndex()]);

    mock->reset();

    DrmMockCustom::IoctlResExt ioctlResExt = {2, -1};
    ioctlResExt.no.push_back(3);
    ioctlResExt.no.push_back(4);
    mock->ioctlResExt = &ioctlResExt;
    mock->errnoValue = EFAULT;
    mock->ioctlExpected.gemUserptr = 2;
    mock->ioctlExpected.execbuffer2 = 3;

    OsHandleStorage handleStorage;
    OsHandleLinux handle1;
    handleStorage.fragmentStorageData[0].osHandleStorage = &handle1;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 4096;

    handleStorage.fragmentStorageData[1].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[1].cpuPtr = reinterpret_cast<void *>(0x2000);
    handleStorage.fragmentStorageData[1].fragmentSize = 8192;

    handleStorage.fragmentStorageData[2].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[2].cpuPtr = reinterpret_cast<void *>(0x4000);
    handleStorage.fragmentStorageData[2].fragmentSize = 4096;

    auto result = memoryManager->populateOsHandles(handleStorage, rootDeviceIndex);
    EXPECT_EQ(MemoryManager::AllocationStatus::InvalidHostPointer, result);

    mock->testIoctls();

    EXPECT_NE(nullptr, handleStorage.fragmentStorageData[0].osHandleStorage);
    EXPECT_NE(nullptr, handleStorage.fragmentStorageData[1].osHandleStorage);
    EXPECT_NE(nullptr, handleStorage.fragmentStorageData[2].osHandleStorage);

    EXPECT_TRUE(handleStorage.fragmentStorageData[1].freeTheFragment);
    EXPECT_TRUE(handleStorage.fragmentStorageData[2].freeTheFragment);

    handleStorage.fragmentStorageData[0].freeTheFragment = false;
    handleStorage.fragmentStorageData[1].freeTheFragment = true;
    handleStorage.fragmentStorageData[2].freeTheFragment = true;

    memoryManager->cleanOsHandles(handleStorage, rootDeviceIndex);
    mock->ioctlResExt = &mock->none;
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledValidateHostMemoryWhenReadOnlyPointerCausesPinningFailWithEfaultThenPopulateOsHandlesDoesNotStoreTheFragments) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager->pinBBs[device->getRootDeviceIndex()]);

    mock->reset();

    DrmMockCustom::IoctlResExt ioctlResExt = {2, -1};
    ioctlResExt.no.push_back(3);
    ioctlResExt.no.push_back(4);
    mock->ioctlResExt = &ioctlResExt;
    mock->errnoValue = EFAULT;
    mock->ioctlExpected.gemUserptr = 2;
    mock->ioctlExpected.execbuffer2 = 3;

    OsHandleStorage handleStorage;
    OsHandleLinux handle1;
    handleStorage.fragmentStorageData[0].osHandleStorage = &handle1;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 4096;

    handleStorage.fragmentStorageData[1].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[1].cpuPtr = reinterpret_cast<void *>(0x2000);
    handleStorage.fragmentStorageData[1].fragmentSize = 8192;

    handleStorage.fragmentStorageData[2].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[2].cpuPtr = reinterpret_cast<void *>(0x4000);
    handleStorage.fragmentStorageData[2].fragmentSize = 4096;

    auto result = memoryManager->populateOsHandles(handleStorage, rootDeviceIndex);
    EXPECT_EQ(MemoryManager::AllocationStatus::InvalidHostPointer, result);

    mock->testIoctls();

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());

    EXPECT_EQ(0u, hostPtrManager->getFragmentCount());
    EXPECT_EQ(nullptr, hostPtrManager->getFragment({handleStorage.fragmentStorageData[1].cpuPtr, rootDeviceIndex}));
    EXPECT_EQ(nullptr, hostPtrManager->getFragment({handleStorage.fragmentStorageData[2].cpuPtr, rootDeviceIndex}));

    handleStorage.fragmentStorageData[0].freeTheFragment = false;
    handleStorage.fragmentStorageData[1].freeTheFragment = true;
    handleStorage.fragmentStorageData[2].freeTheFragment = true;

    memoryManager->cleanOsHandles(handleStorage, rootDeviceIndex);
    mock->ioctlResExt = &mock->none;
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledValidateHostMemoryWhenPopulateOsHandlesSucceedsThenFragmentIsStoredInHostPtrManager) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager->pinBBs[device->getRootDeviceIndex()]);

    mock->reset();
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.execbuffer2 = 1;

    OsHandleStorage handleStorage;
    handleStorage.fragmentStorageData[0].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 4096;

    auto result = memoryManager->populateOsHandles(handleStorage, rootDeviceIndex);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, result);

    mock->testIoctls();

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());
    EXPECT_EQ(1u, hostPtrManager->getFragmentCount());
    EXPECT_NE(nullptr, hostPtrManager->getFragment({handleStorage.fragmentStorageData[0].cpuPtr, device->getRootDeviceIndex()}));

    handleStorage.fragmentStorageData[0].freeTheFragment = true;
    memoryManager->cleanOsHandles(handleStorage, rootDeviceIndex);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerWhenCleanOsHandlesDeletesHandleDataThenOsHandleStorageAndResidencyIsSetToNullptr) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
    ASSERT_NE(nullptr, memoryManager->pinBBs[device->getRootDeviceIndex()]);
    auto maxOsContextCount = 1u;

    OsHandleStorage handleStorage;
    handleStorage.fragmentStorageData[0].osHandleStorage = new OsHandleLinux();
    handleStorage.fragmentStorageData[0].residency = new ResidencyData(maxOsContextCount);
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 4096;

    handleStorage.fragmentStorageData[1].osHandleStorage = new OsHandleLinux();
    handleStorage.fragmentStorageData[1].residency = new ResidencyData(maxOsContextCount);
    handleStorage.fragmentStorageData[1].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[1].fragmentSize = 4096;

    handleStorage.fragmentStorageData[0].freeTheFragment = true;
    handleStorage.fragmentStorageData[1].freeTheFragment = true;

    memoryManager->cleanOsHandles(handleStorage, rootDeviceIndex);

    for (uint32_t i = 0; i < 2; i++) {
        EXPECT_EQ(nullptr, handleStorage.fragmentStorageData[i].osHandleStorage);
        EXPECT_EQ(nullptr, handleStorage.fragmentStorageData[i].residency);
    }
}

TEST_F(DrmMemoryManagerBasic, ifLimitedRangeAllocatorAvailableWhenAskedForAllocationThenLimitedRangePointerIsReturned) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    size_t size = 100u;
    auto ptr = memoryManager->getGfxPartition(rootDeviceIndex)->heapAllocate(HeapIndex::heapStandard, size);
    auto address64bit = ptrDiff(ptr, memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapStandard));

    EXPECT_LT(address64bit, defaultHwInfo->capabilityTable.gpuAddressSpace);

    EXPECT_LT(0u, address64bit);

    memoryManager->getGfxPartition(rootDeviceIndex)->heapFree(HeapIndex::heapStandard, ptr, size);
}

TEST_F(DrmMemoryManagerBasic, givenSpecificAddressSpaceWhenInitializingMemoryManagerThenSetCorrectHeaps) {
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getMutableHardwareInfo()->capabilityTable.gpuAddressSpace = maxNBitValue(48);
    TestedDrmMemoryManager memoryManager(false, false, false, executionEnvironment);

    auto gfxPartition = memoryManager.getGfxPartition(rootDeviceIndex);
    auto limit = gfxPartition->getHeapLimit(HeapIndex::heapSvm);

    EXPECT_EQ(maxNBitValue(48 - 1), limit);
}

TEST_F(DrmMemoryManagerBasic, givenUnalignedHostPtrWithFlushL3RequiredWhenAllocateGraphicsMemoryThenSetCorrectPatIndex) {
    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    memoryManager->forceLimitedRangeAllocator(MemoryConstants::max48BitAddress);

    allocationData.size = 13;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x5001);
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.flushL3 = true;
    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));

    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0x5001u, reinterpret_cast<uint64_t>(allocation->getUnderlyingBuffer()));
    EXPECT_EQ(13u, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(1u, allocation->getAllocationOffset());
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    if (productHelper.isMisalignedUserPtr2WayCoherent()) {
        EXPECT_EQ(MockGmmClientContextBase::MockPatIndex::twoWayCoherent, allocation->getBO()->peekPatIndex());
    } else {
        EXPECT_EQ(MockGmmClientContextBase::MockPatIndex::cached, allocation->getBO()->peekPatIndex());
    }

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerBasic, givenUnalignedHostPtrWithFlushL3NotRequiredWhenAllocateGraphicsMemoryThenSetCorrectPatIndex) {
    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    memoryManager->forceLimitedRangeAllocator(MemoryConstants::max48BitAddress);

    allocationData.size = 13;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x5001);
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.flushL3 = false;
    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));
    EXPECT_NE(nullptr, allocation);

    EXPECT_EQ(MockGmmClientContextBase::MockPatIndex::cached, allocation->getBO()->peekPatIndex());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerBasic, givenAlignedHostPtrWhenAllocateGraphicsMemoryThenSetCorrectPatIndex) {
    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    memoryManager->forceLimitedRangeAllocator(MemoryConstants::max48BitAddress);

    allocationData.size = MemoryConstants::cacheLineSize;
    allocationData.hostPtr = reinterpret_cast<const void *>(MemoryConstants::pageSize);
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.flushL3 = true;
    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));

    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryConstants::pageSize, reinterpret_cast<uint64_t>(allocation->getUnderlyingBuffer()));
    EXPECT_EQ(MemoryConstants::cacheLineSize, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(0u, allocation->getAllocationOffset());

    EXPECT_EQ(MockGmmClientContextBase::MockPatIndex::cached, allocation->getBO()->peekPatIndex());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerBasic, givenImageOrSharedResourceCopyWhenGraphicsAllocationInDevicePoolIsAllocatedThenNullptrIsReturned) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.rootDeviceIndex = rootDeviceIndex;

    AllocationType types[] = {AllocationType::image,
                              AllocationType::sharedResourceCopy};

    for (auto type : types) {
        allocData.type = type;
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
        EXPECT_EQ(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
    }
}

TEST_F(DrmMemoryManagerBasic, givenLocalMemoryDisabledWhenAllocateInDevicePoolIsCalledThenNullptrAndStatusRetryIsReturned) {
    const bool localMemoryEnabled = false;
    TestedDrmMemoryManager memoryManager(localMemoryEnabled, false, false, executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.useSystemMemory = false;
    allocData.flags.allocateMemory = true;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDebugModuleAreaTypeWhenCreatingAllocationThen32BitDrmAllocationWithFrontWindowGpuVaIsReturned) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    const auto size = MemoryConstants::pageSize64k;

    NEO::AllocationProperties properties{device->getRootDeviceIndex(), true, size,
                                         NEO::AllocationType::debugModuleArea,
                                         false,
                                         device->getDeviceBitfield()};

    auto moduleDebugArea = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    EXPECT_NE(nullptr, moduleDebugArea);
    EXPECT_NE(nullptr, moduleDebugArea->getUnderlyingBuffer());
    EXPECT_GE(moduleDebugArea->getUnderlyingBufferSize(), size);

    auto address64bit = moduleDebugArea->getGpuAddressToPatch();
    EXPECT_LT(address64bit, MemoryConstants::max32BitAddress);
    EXPECT_TRUE(moduleDebugArea->is32BitAllocation());

    HeapIndex heap = HeapAssigner::mapInternalWindowIndex(memoryManager->selectInternalHeap(moduleDebugArea->isAllocatedInLocalMemoryPool()));
    EXPECT_TRUE(heap == HeapIndex::heapInternalDeviceFrontWindow || heap == HeapIndex::heapInternalFrontWindow);

    auto gmmHelper = device->getGmmHelper();
    auto frontWindowBase = gmmHelper->canonize(memoryManager->getGfxPartition(moduleDebugArea->getRootDeviceIndex())->getHeapBase(heap));
    EXPECT_EQ(frontWindowBase, moduleDebugArea->getGpuBaseAddress());
    EXPECT_EQ(frontWindowBase, moduleDebugArea->getGpuAddress());

    auto internalHeapBase = gmmHelper->canonize(memoryManager->getGfxPartition(moduleDebugArea->getRootDeviceIndex())->getHeapBase(memoryManager->selectInternalHeap(moduleDebugArea->isAllocatedInLocalMemoryPool())));
    EXPECT_EQ(internalHeapBase, moduleDebugArea->getGpuBaseAddress());

    memoryManager->freeGraphicsMemory(moduleDebugArea);
}

struct DrmAllocationTests : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    }

    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
};

TEST_F(DrmAllocationTests, givenAllocationTypeWhenPassedToDrmAllocationConstructorThenAllocationTypeIsStored) {
    DrmAllocation allocation{0, 1u /*num gmms*/, AllocationType::commandBuffer, nullptr, nullptr, static_cast<size_t>(0), 0u, MemoryPool::memoryNull};
    EXPECT_EQ(AllocationType::commandBuffer, allocation.getAllocationType());

    DrmAllocation allocation2{0, 1u /*num gmms*/, AllocationType::unknown, nullptr, nullptr, 0ULL, static_cast<size_t>(0), MemoryPool::memoryNull};
    EXPECT_EQ(AllocationType::unknown, allocation2.getAllocationType());
}

TEST_F(DrmAllocationTests, givenMemoryPoolWhenPassedToDrmAllocationConstructorThenMemoryPoolIsStored) {
    DrmAllocation allocation{0, 1u /*num gmms*/, AllocationType::commandBuffer, nullptr, nullptr, static_cast<size_t>(0), 0u, MemoryPool::system64KBPages};
    EXPECT_EQ(MemoryPool::system64KBPages, allocation.getMemoryPool());

    DrmAllocation allocation2{0, 1u /*num gmms*/, AllocationType::unknown, nullptr, nullptr, 0ULL, static_cast<size_t>(0), MemoryPool::systemCpuInaccessible};
    EXPECT_EQ(MemoryPool::systemCpuInaccessible, allocation2.getMemoryPool());
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, whenReservingAddressRangeThenExpectProperAddressAndReleaseWhenFreeing) {
    constexpr size_t size = 0x1000;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), size});
    ASSERT_NE(nullptr, allocation);
    void *reserve = memoryManager->reserveCpuAddressRange(size, 0u);
    EXPECT_EQ(nullptr, reserve);
    allocation->setReservedAddressRange(reserve, size);
    EXPECT_EQ(reserve, allocation->getReservedAddressPtr());
    EXPECT_EQ(size, allocation->getReservedAddressSize());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST(DrmMemoryManagerWithExplicitExpectationsTest2, whenObtainFdFromHandleIsCalledThenProperFdHandleIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(4u);
    for (auto i = 0u; i < 4u; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(DrmMockCustom::create(*executionEnvironment->rootDeviceEnvironments[0]));
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);
    for (auto i = 0u; i < 4u; i++) {
        auto mock = executionEnvironment->rootDeviceEnvironments[i]->osInterface->getDriverModel()->as<DrmMockCustom>();

        int boHandle = 3;
        mock->outputFd = 1337;
        mock->ioctlExpected.handleToPrimeFd = 1;
        auto fdHandle = memoryManager->obtainFdFromHandle(boHandle, i);
        EXPECT_EQ(mock->inputHandle, static_cast<uint32_t>(boHandle));
        EXPECT_EQ(mock->inputFlags, DRM_CLOEXEC | DRM_RDWR);
        EXPECT_EQ(1337, fdHandle);
    }
}

TEST(DrmMemoryManagerWithExplicitExpectationsTest2, whenFailingToObtainFdFromHandleThenErrorIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(4u);
    for (auto i = 0u; i < 4u; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(DrmMockCustom::create(*executionEnvironment->rootDeviceEnvironments[0]));
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);
    for (auto i = 0u; i < 4u; i++) {
        auto mock = executionEnvironment->rootDeviceEnvironments[i]->osInterface->getDriverModel()->as<DrmMockCustom>();

        int boHandle = 3;
        mock->outputFd = -1;
        mock->ioctlRes = -1;
        mock->ioctlExpected.handleToPrimeFd = 1;
        auto fdHandle = memoryManager->obtainFdFromHandle(boHandle, i);
        EXPECT_EQ(-1, fdHandle);
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenSvmCpuAllocationWhenSizeAndAlignmentProvidedThenAllocateMemoryAndReserveGpuVa) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    AllocationData allocationData;
    allocationData.size = 2 * MemoryConstants::megaByte;
    allocationData.alignment = 2 * MemoryConstants::megaByte;
    allocationData.type = AllocationType::svmCpu;
    allocationData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithAlignment(allocationData));
    ASSERT_NE(nullptr, allocation);

    EXPECT_EQ(AllocationType::svmCpu, allocation->getAllocationType());

    EXPECT_EQ(allocationData.size, allocation->getUnderlyingBufferSize());
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(allocation->getUnderlyingBuffer(), allocation->getDriverAllocatedCpuPtr());

    EXPECT_NE(0llu, allocation->getGpuAddress());
    EXPECT_NE(reinterpret_cast<uint64_t>(allocation->getUnderlyingBuffer()), allocation->getGpuAddress());

    auto bo = allocation->getBO();
    ASSERT_NE(nullptr, bo);

    EXPECT_NE(0llu, bo->peekAddress());

    auto gmmHelper = device->getGmmHelper();
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapStandard)), bo->peekAddress());
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapStandard)), bo->peekAddress());

    EXPECT_EQ(reinterpret_cast<void *>(allocation->getGpuAddress()), alignUp(allocation->getReservedAddressPtr(), allocationData.alignment));
    EXPECT_EQ(alignUp(allocationData.size, allocationData.alignment) + allocationData.alignment, allocation->getReservedAddressSize());

    EXPECT_GT(allocation->getReservedAddressSize(), bo->peekSize());

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenSvmCpuAllocationWhenSizeAndAlignmentProvidedButFailsToReserveGpuVaThenNullAllocationIsReturned) {
    mock->ioctlExpected.gemUserptr = 0;
    mock->ioctlExpected.gemWait = 0;
    mock->ioctlExpected.gemClose = 0;

    memoryManager->getGfxPartition(rootDeviceIndex)->heapInit(HeapIndex::heapStandard, 0, 0);

    AllocationData allocationData;
    allocationData.size = 2 * MemoryConstants::megaByte;
    allocationData.alignment = 2 * MemoryConstants::megaByte;
    allocationData.type = AllocationType::svmCpu;
    allocationData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);
    EXPECT_EQ(nullptr, allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerAndReleaseGpuRangeIsCalledThenGpuAddressIsDecanonized) {
    constexpr size_t reservedCpuAddressRangeSize = is64bit ? (6 * 4 * MemoryConstants::gigaByte) : 0;
    auto hwInfo = defaultHwInfo.get();
    auto mockGfxPartition = std::make_unique<MockGfxPartition>();
    mockGfxPartition->init(hwInfo->capabilityTable.gpuAddressSpace, reservedCpuAddressRangeSize, 0, 1, false, 0u, hwInfo->capabilityTable.gpuAddressSpace + 1);
    auto size = 2 * MemoryConstants::megaByte;
    auto gpuAddress = mockGfxPartition->heapAllocate(HeapIndex::heapStandard, size);
    auto gmmHelper = device->getGmmHelper();
    auto gpuAddressCanonized = gmmHelper->canonize(gpuAddress);
    EXPECT_LE(gpuAddress, gpuAddressCanonized);

    memoryManager->overrideGfxPartition(mockGfxPartition.release());
    memoryManager->releaseGpuRange(reinterpret_cast<void *>(gpuAddressCanonized), size, 0);

    auto mockGfxPartitionBasic = std::make_unique<MockGfxPartitionBasic>();
    memoryManager->overrideGfxPartition(mockGfxPartitionBasic.release());
}

TEST(DrmMemoryManagerFreeGraphicsMemoryCallSequenceTest, givenDrmMemoryManagerAndFreeGraphicsMemoryIsCalledThenUnreferenceBufferObjectIsCalledFirstWithSynchronousDestroySetToTrue) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();
    TestedDrmMemoryManager memoryManger(executionEnvironment);

    AllocationProperties properties{mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield};
    auto allocation = memoryManger.allocateGraphicsMemoryWithProperties(properties);
    ASSERT_NE(allocation, nullptr);

    memoryManger.freeGraphicsMemory(allocation);

    EXPECT_EQ(EngineLimits::maxHandleCount, memoryManger.unreferenceCalled);
    for (size_t i = 0; i < EngineLimits::maxHandleCount; ++i) {
        EXPECT_TRUE(memoryManger.unreferenceParamsPassed[i].synchronousDestroy);
    }
    EXPECT_EQ(1u, memoryManger.releaseGpuRangeCalled);
    EXPECT_EQ(1u, memoryManger.alignedFreeWrapperCalled);
}

TEST(DrmMemoryManagerFreeGraphicsMemoryUnreferenceTest,
     givenCallToCreateSharedAllocationWithNoReuseSharedAllocationThenAllocationsSuccedAndAddressesAreDifferent) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    const uint32_t rootDeviceIndex = 0u;
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]);
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();
    TestedDrmMemoryManager memoryManger(executionEnvironment);

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, {});
    auto allocation = memoryManger.createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, false, nullptr);
    ASSERT_NE(nullptr, allocation);

    auto allocation2 = memoryManger.createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, false, nullptr);
    ASSERT_NE(nullptr, allocation2);

    EXPECT_NE(allocation->getGpuAddress(), allocation2->getGpuAddress());

    memoryManger.freeGraphicsMemory(allocation2);
    memoryManger.freeGraphicsMemory(allocation);
}

TEST(DrmMemoryManagerFreeGraphicsMemoryUnreferenceTest,
     whenPrintBOCreateDestroyResultFlagIsSetAndCallToCreateSharedAllocationThenExpectedMessageIsPrinted) {
    DebugManagerStateRestore stateRestore;

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    const uint32_t rootDeviceIndex = 0u;
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]);
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();
    TestedDrmMemoryManager memoryManger(executionEnvironment);

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, {});

    debugManager.flags.PrintBOCreateDestroyResult.set(true);
    StreamCapture capture;
    capture.captureStdout();
    auto allocation = memoryManger.createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, false, nullptr);
    ASSERT_NE(nullptr, allocation);

    std::stringstream expectedOutput;
    expectedOutput << "Created BO-0 range: ";
    expectedOutput << std::hex << allocation->getGpuAddress();
    expectedOutput << " - ";
    expectedOutput << std::hex << ptrOffset(allocation->getGpuAddress(), MemoryConstants::pageSize);
    expectedOutput << ", size: 4096 from PRIME_FD_TO_HANDLE\nCalling gem close on handle: BO-0\n";

    memoryManger.freeGraphicsMemory(allocation);

    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(expectedOutput.str(), output);
}

struct DrmMemoryManagerWithHostIpcAllocationParamTest : public DrmMemoryManagerFixture, ::testing::TestWithParam<bool> {
    template <typename GfxFamily>
    void setUpT() {
        DrmMemoryManagerFixture::setUpT<GfxFamily>();
    }

    template <typename GfxFamily>
    void tearDownT() {
        DrmMemoryManagerFixture::tearDownT<GfxFamily>();
    }
};

HWTEST_TEMPLATED_P(DrmMemoryManagerWithHostIpcAllocationParamTest,
                   givenIPCBoHandleAndSharedAllocationReuseEnabledWhenAllocationCreatedThenBoHandleSharingIsNotUsed) {
    const bool reuseSharedAllocation = true;

    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->outputHandle = 88u;
    bool isHostIpcAllocation = GetParam();
    AllocationProperties properties(rootDeviceIndex, false, 4096u, AllocationType::sharedBuffer, false, {});

    TestedDrmMemoryManager::OsHandleData osHandleData{11u};
    auto gfxAllocation1 = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, isHostIpcAllocation, reuseSharedAllocation, nullptr);
    DrmAllocation *drmAllocation1 = static_cast<DrmAllocation *>(gfxAllocation1);
    ASSERT_NE(nullptr, drmAllocation1);

    // BoHandle not registered as shared at all
    auto bo1 = drmAllocation1->getBO();
    EXPECT_NE(bo1, nullptr);
    EXPECT_EQ(static_cast<uint32_t>(bo1->getHandle()), mock->outputHandle);
    EXPECT_FALSE(bo1->isBoHandleShared());
    auto boHandleWrapperIt1 = memoryManager->sharedBoHandles.find(std::make_pair(mock->outputHandle, rootDeviceIndex));
    EXPECT_EQ(boHandleWrapperIt1, std::end(memoryManager->sharedBoHandles));

    memoryManager->freeGraphicsMemory(gfxAllocation1);
}

HWTEST_TEMPLATED_P(DrmMemoryManagerWithHostIpcAllocationParamTest,
                   givenIPCBoHandleAndSharedAllocationReuseEnabledWhenAllocationCreatedFromMultipleHandlesThenBoHandleSharingIsNotUsed) {
    const bool reuseSharedAllocation = true;

    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->outputHandle = 88u;
    bool isHostIpcAllocation = GetParam();
    AllocationProperties properties(rootDeviceIndex, true, 4096u, AllocationType::sharedBuffer, false, {0b0010});

    std::vector<osHandle> handles = {11u};
    auto gfxAllocation1 = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, isHostIpcAllocation, reuseSharedAllocation, nullptr);
    DrmAllocation *drmAllocation1 = static_cast<DrmAllocation *>(gfxAllocation1);
    ASSERT_NE(nullptr, drmAllocation1);

    // BoHandle not registered as shared at all
    auto bo1 = drmAllocation1->getBO();
    EXPECT_NE(bo1, nullptr);
    EXPECT_EQ(static_cast<uint32_t>(bo1->getHandle()), mock->outputHandle);
    EXPECT_FALSE(bo1->isBoHandleShared());
    auto boHandleWrapperIt1 = memoryManager->sharedBoHandles.find(std::make_pair(mock->outputHandle, rootDeviceIndex));
    EXPECT_EQ(boHandleWrapperIt1, std::end(memoryManager->sharedBoHandles));

    memoryManager->freeGraphicsMemory(gfxAllocation1);
}

HWTEST_TEMPLATED_P(DrmMemoryManagerWithHostIpcAllocationParamTest,
                   givenIPCBoHandleAndSharedAllocationReuseDisabledWhenMultipleAllocationsCreatedFromSingleProcessThenBoHandleIsClosedOnlyOnce) {
    const bool reuseSharedAllocation = false;

    mock->ioctlExpected.primeFdToHandle = 2;
    mock->ioctlExpected.gemWait = 2;
    mock->ioctlExpected.gemClose = 1;
    mock->outputHandle = 88u;
    bool isHostIpcAllocation = GetParam();
    AllocationProperties properties(rootDeviceIndex, false, 4096u, AllocationType::sharedBuffer, false, {});

    TestedDrmMemoryManager::OsHandleData osHandleData1{11u};
    auto gfxAllocation1 = memoryManager->createGraphicsAllocationFromSharedHandle(&osHandleData1, properties, false, isHostIpcAllocation, reuseSharedAllocation, nullptr);
    DrmAllocation *drmAllocation1 = static_cast<DrmAllocation *>(gfxAllocation1);
    ASSERT_NE(nullptr, drmAllocation1);

    // BoHandle registered as shared but with WEAK ownership - GEM_CLOSE can be called on it
    auto bo1 = drmAllocation1->getBO();
    EXPECT_NE(bo1, nullptr);
    EXPECT_EQ(static_cast<uint32_t>(bo1->getHandle()), mock->outputHandle);
    EXPECT_TRUE(bo1->isBoHandleShared());
    auto boHandleWrapperIt1 = memoryManager->sharedBoHandles.find(std::make_pair(mock->outputHandle, rootDeviceIndex));
    EXPECT_NE(boHandleWrapperIt1, std::end(memoryManager->sharedBoHandles));
    EXPECT_TRUE(boHandleWrapperIt1->second.canCloseBoHandle());

    TestedDrmMemoryManager::OsHandleData osHandleData2{12u};
    auto gfxAllocation2 = memoryManager->createGraphicsAllocationFromSharedHandle(&osHandleData2, properties, false, isHostIpcAllocation, false, nullptr);
    DrmAllocation *drmAllocation2 = static_cast<DrmAllocation *>(gfxAllocation2);
    ASSERT_NE(nullptr, drmAllocation2);
    // BoHandle registered as shared with SHARED ownership - GEM_CLOSE cannot be called on it
    auto bo2 = drmAllocation2->getBO();
    EXPECT_NE(bo2, nullptr);
    EXPECT_EQ(static_cast<uint32_t>(bo2->getHandle()), mock->outputHandle);
    EXPECT_TRUE(bo2->isBoHandleShared());
    auto boHandleWrapperIt2 = memoryManager->sharedBoHandles.find(std::make_pair(mock->outputHandle, rootDeviceIndex));
    EXPECT_EQ(boHandleWrapperIt2, boHandleWrapperIt1);
    EXPECT_FALSE(boHandleWrapperIt2->second.canCloseBoHandle());

    memoryManager->freeGraphicsMemory(gfxAllocation2);
    // GEM_CLOSE can be called on BoHandle again
    EXPECT_TRUE(boHandleWrapperIt2->second.canCloseBoHandle());
    memoryManager->freeGraphicsMemory(gfxAllocation1);
}

HWTEST_TEMPLATED_P(DrmMemoryManagerWithHostIpcAllocationParamTest,
                   givenIPCBoHandleAndSharedAllocationReuseDisabledWhenMultipleAllocationsCreatedFromDifferentDevicesThenBoHandlesAreClosedSeparately) {
    const bool reuseSharedAllocation = false;

    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->outputHandle = 88u;
    bool isHostIpcAllocation = GetParam();
    AllocationProperties properties(rootDeviceIndex, false, 4096u, AllocationType::sharedBuffer, false, {});

    TestedDrmMemoryManager::OsHandleData osHandleData1{11u};
    auto gfxAllocation1 = memoryManager->createGraphicsAllocationFromSharedHandle(&osHandleData1, properties, false, isHostIpcAllocation, reuseSharedAllocation, nullptr);
    DrmAllocation *drmAllocation1 = static_cast<DrmAllocation *>(gfxAllocation1);
    ASSERT_NE(nullptr, drmAllocation1);

    // BoHandle registered as shared but with WEAK ownership - GEM_CLOSE can be called on it
    auto bo1 = drmAllocation1->getBO();
    EXPECT_NE(bo1, nullptr);
    EXPECT_EQ(static_cast<uint32_t>(bo1->getHandle()), mock->outputHandle);
    EXPECT_TRUE(bo1->isBoHandleShared());
    auto boHandleWrapperIt1 = memoryManager->sharedBoHandles.find(std::make_pair(mock->outputHandle, rootDeviceIndex));
    EXPECT_NE(boHandleWrapperIt1, std::end(memoryManager->sharedBoHandles));
    EXPECT_TRUE(boHandleWrapperIt1->second.canCloseBoHandle());

    TestedDrmMemoryManager::OsHandleData osHandleData2{12u};
    // two devices in the system, rootDeviceIndex is set to 1, so set rootDeviceInext to 0
    auto rootDeviceIndex2 = 0;
    auto rootDeviceEnvironment2 = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex2].get();
    auto mock2 = static_cast<DrmMockCustom *>(rootDeviceEnvironment2->osInterface->getDriverModel()->as<Drm>());
    mock2->ioctlExpected.primeFdToHandle = 1;
    mock2->ioctlExpected.gemWait = 1;
    mock2->ioctlExpected.gemClose = 1;
    mock2->outputHandle = 88u;
    AllocationProperties properties2(rootDeviceIndex2, false, 4096u, AllocationType::sharedBuffer, false, {});
    auto gfxAllocation2 = memoryManager->createGraphicsAllocationFromSharedHandle(&osHandleData2, properties2, false, isHostIpcAllocation, false, nullptr);
    DrmAllocation *drmAllocation2 = static_cast<DrmAllocation *>(gfxAllocation2);
    ASSERT_NE(nullptr, drmAllocation2);
    // BoHandle registered as shared with WEAK ownership because it's from different root devices - GEM_CLOSE can be called on it
    auto bo2 = drmAllocation2->getBO();
    EXPECT_NE(bo2, nullptr);
    EXPECT_EQ(static_cast<uint32_t>(bo2->getHandle()), mock->outputHandle);
    EXPECT_TRUE(bo2->isBoHandleShared());
    auto boHandleWrapperIt2 = memoryManager->sharedBoHandles.find(std::make_pair(mock->outputHandle, rootDeviceIndex2));
    EXPECT_NE(boHandleWrapperIt2, boHandleWrapperIt1);
    EXPECT_TRUE(boHandleWrapperIt2->second.canCloseBoHandle());

    memoryManager->freeGraphicsMemory(gfxAllocation2);
    memoryManager->freeGraphicsMemory(gfxAllocation1);
    mock2->testIoctls();
    mock2->reset();
}

HWTEST_TEMPLATED_P(DrmMemoryManagerWithHostIpcAllocationParamTest,
                   givenIPCBoHandleAndSharedAllocationReuseDisabledWhenMultipleAllocationsCreatedFromMultipleSharedHandlesFromSingleProcessThenBoHandleIsClosedOnlyOnce) {
    const bool reuseSharedAllocation = false;

    mock->ioctlExpected.primeFdToHandle = 2;
    mock->ioctlExpected.gemWait = 2;
    mock->ioctlExpected.gemClose = 1;
    mock->outputHandle = 88u;
    bool isHostIpcAllocation = GetParam();
    AllocationProperties properties(rootDeviceIndex, true, 4096u, AllocationType::sharedBuffer, false, {});

    std::vector<osHandle> handles1 = {11u};
    auto gfxAllocation1 = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles1, properties, false, isHostIpcAllocation, reuseSharedAllocation, nullptr);
    DrmAllocation *drmAllocation1 = static_cast<DrmAllocation *>(gfxAllocation1);
    ASSERT_NE(nullptr, drmAllocation1);

    // BoHandle registered as shared but with WEAK ownership - GEM_CLOSE can be called on it
    auto bo1 = drmAllocation1->getBO();
    EXPECT_NE(bo1, nullptr);
    EXPECT_EQ(static_cast<uint32_t>(bo1->getHandle()), mock->outputHandle);
    EXPECT_TRUE(bo1->isBoHandleShared());
    auto boHandleWrapperIt1 = memoryManager->sharedBoHandles.find(std::make_pair(mock->outputHandle, rootDeviceIndex));
    EXPECT_NE(boHandleWrapperIt1, std::end(memoryManager->sharedBoHandles));
    EXPECT_TRUE(boHandleWrapperIt1->second.canCloseBoHandle());

    std::vector<osHandle> handles2 = {12u};
    auto gfxAllocation2 = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles2, properties, false, isHostIpcAllocation, false, nullptr);
    DrmAllocation *drmAllocation2 = static_cast<DrmAllocation *>(gfxAllocation2);
    ASSERT_NE(nullptr, drmAllocation2);
    // BoHandle registered as shared with SHARED ownership - GEM_CLOSE cannot be called on it
    auto bo2 = drmAllocation2->getBO();
    EXPECT_NE(bo2, nullptr);
    EXPECT_EQ(static_cast<uint32_t>(bo2->getHandle()), mock->outputHandle);
    EXPECT_TRUE(bo2->isBoHandleShared());
    auto boHandleWrapperIt2 = memoryManager->sharedBoHandles.find(std::make_pair(mock->outputHandle, rootDeviceIndex));
    EXPECT_EQ(boHandleWrapperIt2, boHandleWrapperIt1);
    EXPECT_FALSE(boHandleWrapperIt2->second.canCloseBoHandle());

    memoryManager->freeGraphicsMemory(gfxAllocation2);
    // GEM_CLOSE can be called on BoHandle again
    EXPECT_TRUE(boHandleWrapperIt2->second.canCloseBoHandle());
    memoryManager->freeGraphicsMemory(gfxAllocation1);
}

INSTANTIATE_TEST_SUITE_P(HostIpcAllocationFlag,
                         DrmMemoryManagerWithHostIpcAllocationParamTest,
                         ::testing::Values(false, true));

TEST(DrmMemoryManagerFreeGraphicsMemoryUnreferenceTest,
     givenCallToCreateSharedAllocationWithReuseSharedAllocationThenAllocationsSuccedAndAddressesAreTheSame) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    const uint32_t rootDeviceIndex = 0u;
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]);
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();
    TestedDrmMemoryManager memoryManger(executionEnvironment);

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, {});
    auto allocation = memoryManger.createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, allocation);

    auto allocation2 = memoryManger.createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, allocation2);

    EXPECT_EQ(allocation->getGpuAddress(), allocation2->getGpuAddress());

    memoryManger.freeGraphicsMemory(allocation2);
    memoryManger.freeGraphicsMemory(allocation);
}

TEST(DrmMemoryManagerFreeGraphicsMemoryUnreferenceTest, givenDrmMemoryManagerAndFreeGraphicsMemoryIsCalledForSharedAllocationThenUnreferenceBufferObjectIsCalledWithSynchronousDestroySetToFalse) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    const uint32_t rootDeviceIndex = 0u;
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]);
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();
    TestedDrmMemoryManager memoryManger(executionEnvironment);

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, {});
    auto allocation = memoryManger.createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, allocation);

    memoryManger.freeGraphicsMemory(allocation);

    EXPECT_EQ(1 + EngineLimits::maxHandleCount - 1, memoryManger.unreferenceCalled);
    EXPECT_FALSE(memoryManger.unreferenceParamsPassed[0].synchronousDestroy);
    for (size_t i = 1; i < EngineLimits::maxHandleCount - 1; ++i) {
        EXPECT_TRUE(memoryManger.unreferenceParamsPassed[i].synchronousDestroy);
    }
}

struct MockIoctlHelperPrelimResourceRegistration : public IoctlHelperPrelim20 {
  public:
    using IoctlHelperPrelim20::classHandles;
    using IoctlHelperPrelim20::IoctlHelperPrelim20;
};

TEST_F(DrmAllocationTests, givenResourceRegistrationEnabledWhenAllocationTypeShouldBeRegisteredThenBoHasBindExtHandleAdded) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(drm);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::maxSize); i++) {
        ioctlHelper->classHandles.push_back(i);
    }
    drm.ioctlHelper.reset(ioctlHelper.release());

    {
        MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugContextSaveArea, MemoryPool::system4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);
        EXPECT_EQ(DrmResourceClass::contextSaveArea, drm.registeredClass);
    }
    drm.registeredClass = DrmResourceClass::maxSize;

    {
        MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugSbaTrackingBuffer, MemoryPool::system4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);
        EXPECT_EQ(DrmResourceClass::sbaTrackingBuffer, drm.registeredClass);
    }
    drm.registeredClass = DrmResourceClass::maxSize;

    {
        MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::kernelIsa, MemoryPool::system4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);
        EXPECT_EQ(DrmResourceClass::isa, drm.registeredClass);
    }
    drm.registeredClass = DrmResourceClass::maxSize;

    {
        MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugModuleArea, MemoryPool::system4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);
        EXPECT_EQ(DrmResourceClass::moduleHeapDebugArea, drm.registeredClass);
    }

    drm.registeredClass = DrmResourceClass::maxSize;

    {
        MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::bufferHostMemory, MemoryPool::system4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(0u, bo.bindExtHandles.size());
        EXPECT_EQ(DrmResourceClass::maxSize, drm.registeredClass);
    }
}

TEST_F(DrmAllocationTests, givenResourceRegistrationEnabledWhenAllocationTypeShouldNotBeRegisteredThenNoBindHandleCreated) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    drm.registeredClass = DrmResourceClass::maxSize;
    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(drm);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::maxSize); i++) {
        ioctlHelper->classHandles.push_back(i);
    }
    drm.ioctlHelper.reset(ioctlHelper.release());

    {
        MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::kernelIsaInternal, MemoryPool::system4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(0u, bo.bindExtHandles.size());
    }
    EXPECT_EQ(DrmResourceClass::maxSize, drm.registeredClass);
}

TEST_F(DrmAllocationTests, givenResourceRegistrationNotEnabledWhenRegisteringBindExtHandleThenHandleIsNotAddedToBo) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(drm);
    EXPECT_EQ(0u, ioctlHelper->classHandles.size());
    drm.ioctlHelper.reset(ioctlHelper.release());

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugContextSaveArea, MemoryPool::system4KBPages);
    allocation.bufferObjects[0] = &bo;
    allocation.registerBOBindExtHandle(&drm);
    EXPECT_EQ(0u, bo.bindExtHandles.size());
    EXPECT_EQ(DrmResourceClass::maxSize, drm.registeredClass);
}

TEST(DrmMemoryManager, givenTrackedAllocationTypeAndDebuggingDisabledWhenAllocatingThenRegisterBoBindExtHandleIsNotCalled) {
    const uint32_t rootDeviceIndex = 0u;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->initGmm();
    executionEnvironment->setDebuggingMode(DebuggingMode::disabled);

    auto mockDrm = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);

    EXPECT_FALSE(mockDrm->resourceRegistrationEnabled());

    mockDrm->registeredDataSize = 0;

    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugContextSaveArea, MemoryPool::system4KBPages);

    memoryManager->registerAllocationInOs(&allocation);

    EXPECT_FALSE(allocation.registerBOBindExtHandleCalled);
    EXPECT_EQ(DrmResourceClass::maxSize, mockDrm->registeredClass);
}

TEST(DrmMemoryManager, givenResourceRegistrationEnabledAndAllocTypeToCaptureWhenRegisteringAllocationInOsThenItIsMarkedForCapture) {
    const uint32_t rootDeviceIndex = 0u;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->setDebuggingMode(DebuggingMode::online);

    auto mockDrm = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);

    // mock resource registration enabling by storing class handles
    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(*mockDrm);
    ioctlHelper->classHandles.push_back(1);
    mockDrm->ioctlHelper.reset(ioctlHelper.release());

    MockBufferObject bo(rootDeviceIndex, mockDrm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::scratchSurface, MemoryPool::system4KBPages);
    allocation.bufferObjects[0] = &bo;
    memoryManager->registerAllocationInOs(&allocation);

    EXPECT_TRUE(allocation.markedForCapture);

    MockDrmAllocation allocation2(rootDeviceIndex, AllocationType::buffer, MemoryPool::system4KBPages);
    allocation2.bufferObjects[0] = &bo;
    memoryManager->registerAllocationInOs(&allocation2);

    EXPECT_FALSE(allocation2.markedForCapture);
}

TEST(DrmMemoryManager, givenTrackedAllocationTypeWhenAllocatingThenAllocationIsRegistered) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->setDebuggingMode(DebuggingMode::online);

    auto mockDrm = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);

    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(*mockDrm);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::maxSize); i++) {
        ioctlHelper->classHandles.push_back(i);
    }
    mockDrm->ioctlHelper.reset(ioctlHelper.release());

    EXPECT_TRUE(mockDrm->resourceRegistrationEnabled());

    NEO::AllocationProperties properties{0, true, MemoryConstants::pageSize,
                                         NEO::AllocationType::debugSbaTrackingBuffer,
                                         false, 1};

    properties.gpuAddress = 0x20000;
    auto sbaAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);
    EXPECT_EQ(DrmResourceClass::sbaTrackingBuffer, mockDrm->registeredClass);

    EXPECT_EQ(sizeof(uint64_t), mockDrm->registeredDataSize);
    uint64_t *data = reinterpret_cast<uint64_t *>(mockDrm->registeredData);
    EXPECT_EQ(properties.gpuAddress, *data);

    memoryManager->freeGraphicsMemory(sbaAllocation);
}

TEST(DrmMemoryManager, givenTrackedAllocationTypeWhenFreeingThenRegisteredHandlesAreUnregistered) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->setDebuggingMode(DebuggingMode::online);

    auto mockDrm = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);

    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(*mockDrm);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::maxSize); i++) {
        ioctlHelper->classHandles.push_back(i);
    }
    mockDrm->ioctlHelper.reset(ioctlHelper.release());

    EXPECT_TRUE(mockDrm->resourceRegistrationEnabled());

    NEO::AllocationProperties properties{0, true, MemoryConstants::pageSize,
                                         NEO::AllocationType::debugSbaTrackingBuffer,
                                         false, 1};

    properties.gpuAddress = 0x20000;
    auto sbaAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    EXPECT_EQ(0u, mockDrm->unregisterCalledCount);

    memoryManager->freeGraphicsMemory(sbaAllocation);

    EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, mockDrm->unregisteredHandle);
    EXPECT_EQ(1u, mockDrm->unregisterCalledCount);
}

TEST(DrmMemoryManager, givenEnabledResourceRegistrationWhenSshIsAllocatedThenItIsMarkedForCapture) {
    auto executionEnvironment = new MockExecutionEnvironment();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->setDebuggingMode(DebuggingMode::online);

    auto mockDrm = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mockDrm, 0u, false);
    executionEnvironment->memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);

    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(*mockDrm);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::maxSize); i++) {
        ioctlHelper->classHandles.push_back(i);
    }
    mockDrm->ioctlHelper.reset(ioctlHelper.release());
    EXPECT_TRUE(mockDrm->resourceRegistrationEnabled());

    auto device = std::unique_ptr<MockDevice>(MockDevice::create<MockDevice>(executionEnvironment, 0));

    CommandContainer cmdContainer;
    cmdContainer.initialize(device.get(), nullptr, HeapSize::defaultHeapSize, true, false);

    auto *ssh = cmdContainer.getIndirectHeap(NEO::HeapType::surfaceState);
    auto bo = static_cast<DrmAllocation *>(ssh->getGraphicsAllocation())->getBO();

    ASSERT_NE(nullptr, bo);
    EXPECT_TRUE(bo->isMarkedForCapture());
}

TEST(DrmMemoryManager, givenNullBoWhenRegisteringBindExtHandleThenEarlyReturn) {
    const uint32_t rootDeviceIndex = 0u;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto mockDrm = std::make_unique<DrmMockResources>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(*mockDrm);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::maxSize); i++) {
        ioctlHelper->classHandles.push_back(i);
    }
    mockDrm->ioctlHelper.reset(ioctlHelper.release());

    EXPECT_TRUE(mockDrm->resourceRegistrationEnabled());

    MockDrmAllocation gfxAllocation(rootDeviceIndex, AllocationType::debugSbaTrackingBuffer, MemoryPool::memoryNull);

    gfxAllocation.registerBOBindExtHandle(mockDrm.get());
    EXPECT_EQ(1u, gfxAllocation.registeredBoBindHandles.size());
    gfxAllocation.freeRegisteredBOBindExtHandles(mockDrm.get());
}

TEST_F(DrmAllocationTests, givenResourceRegistrationEnabledWhenAllocationIsRegisteredThenBosAreMarkedForCaptureAndRequireImmediateBinding) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    // mock resource registration enabling by storing class handles
    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(drm);

    ioctlHelper->classHandles.push_back(1);
    drm.ioctlHelper.reset(ioctlHelper.release());

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugContextSaveArea, MemoryPool::system4KBPages);
    allocation.bufferObjects[0] = &bo;
    allocation.registerBOBindExtHandle(&drm);

    EXPECT_TRUE(bo.isMarkedForCapture());
    EXPECT_TRUE(bo.isImmediateBindingRequired());
}

TEST_F(DrmAllocationTests, givenResourceRegistrationEnabledAndTileInstancedIsaWhenIsaIsRegisteredThenCookieIsAddedToBoHandle) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(drm);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::maxSize); i++) {
        ioctlHelper->classHandles.push_back(i);
    }
    drm.ioctlHelper.reset(ioctlHelper.release());
    drm.registeredClass = DrmResourceClass::maxSize;

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::kernelIsa, MemoryPool::localMemory);
    allocation.storageInfo.tileInstanced = true;
    allocation.bufferObjects[0] = &bo;
    allocation.registerBOBindExtHandle(&drm);
    EXPECT_EQ(2u, bo.bindExtHandles.size());

    EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);
    EXPECT_EQ(drm.currentCookie - 1, bo.bindExtHandles[1]);

    allocation.freeRegisteredBOBindExtHandles(&drm);
    EXPECT_EQ(2u, drm.unregisterCalledCount);
}

TEST_F(DrmAllocationTests, givenResourceRegistrationEnabledAndSingleInstanceIsaWhenIsaIsRegisteredThenCookieIsNotAddedToBoHandle) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(drm);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::maxSize); i++) {
        ioctlHelper->classHandles.push_back(i);
    }
    drm.ioctlHelper.reset(ioctlHelper.release());
    drm.registeredClass = DrmResourceClass::maxSize;

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::kernelIsa, MemoryPool::system4KBPages);
    allocation.bufferObjects[0] = &bo;
    allocation.registerBOBindExtHandle(&drm);
    EXPECT_EQ(1u, bo.bindExtHandles.size());

    EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);

    allocation.freeRegisteredBOBindExtHandles(&drm);
    EXPECT_EQ(1u, drm.unregisterCalledCount);
}

TEST_F(DrmAllocationTests, givenResourceRegistrationEnabledWhenIsaIsRegisteredThenDeviceBitfieldIsPassedAsPayload) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(drm);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::maxSize); i++) {
        ioctlHelper->classHandles.push_back(i);
    }
    drm.ioctlHelper.reset(ioctlHelper.release());
    drm.registeredClass = DrmResourceClass::maxSize;

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::kernelIsa, MemoryPool::localMemory);
    allocation.storageInfo.subDeviceBitfield = 1 << 3;
    allocation.bufferObjects[0] = &bo;
    allocation.registerBOBindExtHandle(&drm);
    EXPECT_EQ(1u, bo.bindExtHandles.size());

    EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);

    EXPECT_EQ(sizeof(uint32_t), drm.registeredDataSize);
    uint32_t *data = reinterpret_cast<uint32_t *>(drm.registeredData);
    EXPECT_EQ(static_cast<uint32_t>(allocation.storageInfo.subDeviceBitfield.to_ulong()), *data);
}

TEST_F(DrmAllocationTests, givenResourceRegistrationEnabledAndSubDeviceBitfieldSetWhenSbaTrackingBufferIsRegisteredThenDeviceContextIdIsPassedAsPayload) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(drm);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::maxSize); i++) {
        ioctlHelper->classHandles.push_back(i);
    }
    drm.ioctlHelper.reset(ioctlHelper.release());
    drm.registeredClass = DrmResourceClass::maxSize;

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugSbaTrackingBuffer, MemoryPool::localMemory);
    allocation.storageInfo.tileInstanced = false;
    allocation.storageInfo.subDeviceBitfield = 0b0010;
    allocation.bufferObjects[0] = &bo;

    MockOsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    allocation.setOsContext(&osContext);

    osContext.drmContextIds.clear();
    osContext.drmContextIds.push_back(3u);
    osContext.drmContextIds.push_back(5u);

    const auto processId = 0xABCEDF;
    uint64_t offlineDumpContextId = static_cast<uint64_t>(processId) << 32 | static_cast<uint64_t>(5u);

    allocation.registerBOBindExtHandle(&drm);
    EXPECT_EQ(2u, bo.bindExtHandles.size());

    EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);

    uint64_t *data = reinterpret_cast<uint64_t *>(drm.registeredData);
    EXPECT_EQ(offlineDumpContextId, *data);

    allocation.freeRegisteredBOBindExtHandles(&drm);
    EXPECT_EQ(2u, drm.unregisterCalledCount);
}

TEST_F(DrmAllocationTests, givenResourceRegistrationEnabledAndSubDeviceBitfieldNotSetWhenSbaTrackingBufferIsRegisteredThenContextIdIsTakenFromDevice0AndPassedAsPayload) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(drm);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::maxSize); i++) {
        ioctlHelper->classHandles.push_back(i);
    }
    drm.ioctlHelper.reset(ioctlHelper.release());
    drm.registeredClass = DrmResourceClass::maxSize;

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugSbaTrackingBuffer, MemoryPool::localMemory);
    allocation.storageInfo.tileInstanced = false;
    allocation.bufferObjects[0] = &bo;

    MockOsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    allocation.setOsContext(&osContext);

    osContext.drmContextIds.clear();
    osContext.drmContextIds.push_back(3u);
    osContext.drmContextIds.push_back(5u);

    const auto processId = 0xABCEDF;
    uint64_t offlineDumpContextId = static_cast<uint64_t>(processId) << 32 | static_cast<uint64_t>(3u);

    allocation.registerBOBindExtHandle(&drm);
    EXPECT_EQ(2u, bo.bindExtHandles.size());

    EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);

    uint64_t *data = reinterpret_cast<uint64_t *>(drm.registeredData);
    EXPECT_EQ(offlineDumpContextId, *data);

    allocation.freeRegisteredBOBindExtHandles(&drm);
    EXPECT_EQ(2u, drm.unregisterCalledCount);
}

TEST_F(DrmAllocationTests, givenTwoBufferObjectsAndTileInstancedSbaAndSubDeviceBitfieldWhenSbaTrackingBufferIsRegisteredThenContextIdIsTakenFromBufferObjectIndexAndPassedAsPayload) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(drm);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::maxSize); i++) {
        ioctlHelper->classHandles.push_back(i);
    }
    drm.ioctlHelper.reset(ioctlHelper.release());
    drm.registeredClass = DrmResourceClass::maxSize;

    MockBufferObject bo0(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockBufferObject bo1(rootDeviceIndex, &drm, 3, 0, 0, 1);

    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugSbaTrackingBuffer, MemoryPool::localMemory);
    allocation.storageInfo.subDeviceBitfield = 0b0011;
    allocation.storageInfo.tileInstanced = true;
    allocation.bufferObjects[0] = &bo0;
    allocation.bufferObjects[1] = &bo1;

    MockOsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    allocation.setOsContext(&osContext);

    osContext.drmContextIds.clear();
    osContext.drmContextIds.push_back(3u);
    osContext.drmContextIds.push_back(5u);

    const auto processId = 0xABCEDF;
    uint64_t offlineDumpContextIdBo1 = static_cast<uint64_t>(processId) << 32 | static_cast<uint64_t>(5u);

    allocation.registerBOBindExtHandle(&drm);
    EXPECT_EQ(2u, bo0.bindExtHandles.size());
    EXPECT_EQ(2u, bo1.bindExtHandles.size());

    EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo0.bindExtHandles[0]);
    EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo1.bindExtHandles[0]);

    uint64_t *dataBo1 = reinterpret_cast<uint64_t *>(drm.registeredData);
    EXPECT_EQ(offlineDumpContextIdBo1, *dataBo1);

    allocation.freeRegisteredBOBindExtHandles(&drm);
    EXPECT_EQ(3u, drm.unregisterCalledCount);
}

TEST_F(DrmAllocationTests, givenResourceRegistrationEnabledWhenSbaTrackingBufferIsRegisteredWithoutOsContextThenHandleIsNotAddedToBO) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(drm);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::maxSize); i++) {
        ioctlHelper->classHandles.push_back(i);
    }
    drm.ioctlHelper.reset(ioctlHelper.release());
    drm.registeredClass = DrmResourceClass::maxSize;

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugSbaTrackingBuffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    allocation.registerBOBindExtHandle(&drm);
    EXPECT_EQ(1u, bo.bindExtHandles.size());

    EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);

    allocation.freeRegisteredBOBindExtHandles(&drm);
    EXPECT_EQ(1u, drm.unregisterCalledCount);
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenSetCacheRegionIsCalledForDefaultRegionThenReturnTrue) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);

    EXPECT_TRUE(allocation.setCacheRegion(&drm, CacheRegion::defaultRegion));
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenCacheInfoIsNotAvailableThenCacheRegionIsNotSet) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);

    EXPECT_FALSE(allocation.setCacheRegion(&drm, CacheRegion::region1));
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenDefaultCacheInfoIsAvailableThenCacheRegionIsNotSet) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    drm.setupCacheInfo(*defaultHwInfo.get());

    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);

    EXPECT_FALSE(allocation.setCacheRegion(&drm, CacheRegion::region1));
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenCacheRegionIsNotSetThenReturnFalse) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    CacheReservationParameters l2CacheParameters{};
    CacheReservationParameters l3CacheParameters{};
    l3CacheParameters.maxSize = 32 * MemoryConstants::kiloByte;
    l3CacheParameters.maxNumRegions = 2;
    l3CacheParameters.maxNumWays = 32;
    drm.cacheInfo.reset(new MockCacheInfo(*drm.getIoctlHelper(), l2CacheParameters, l3CacheParameters));

    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);

    EXPECT_FALSE(allocation.setCacheAdvice(&drm, 1024, CacheRegion::none, false));
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenCacheRegionIsSetSuccessfullyThenReturnTrue) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    drm.queryAndSetVmBindPatIndexProgrammingSupport();

    CacheReservationParameters l2CacheParameters{};
    CacheReservationParameters l3CacheParameters{};
    l3CacheParameters.maxSize = 32 * MemoryConstants::kiloByte;
    l3CacheParameters.maxNumRegions = 2;
    l3CacheParameters.maxNumWays = 32;
    drm.cacheInfo.reset(new MockCacheInfo(*drm.getIoctlHelper(), l2CacheParameters, l3CacheParameters));

    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    if ((productHelper.getNumCacheRegions() == 0) &&
        productHelper.isVmBindPatIndexProgrammingSupported()) {
        EXPECT_ANY_THROW(allocation.setCacheAdvice(&drm, 1024, CacheRegion::region1, false));
    } else {
        EXPECT_TRUE(allocation.setCacheAdvice(&drm, 1024, CacheRegion::region1, false));
    }
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenCacheRegionIsSetSuccessfullyThenSetRegionInBufferObject) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    drm.queryAndSetVmBindPatIndexProgrammingSupport();

    CacheReservationParameters l2CacheParameters{};
    CacheReservationParameters l3CacheParameters{};
    l3CacheParameters.maxSize = 32 * MemoryConstants::kiloByte;
    l3CacheParameters.maxNumRegions = 2;
    l3CacheParameters.maxNumWays = 32;
    drm.cacheInfo.reset(new MockCacheInfo(*drm.getIoctlHelper(), l2CacheParameters, l3CacheParameters));

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    if ((productHelper.getNumCacheRegions() == 0) &&
        productHelper.isVmBindPatIndexProgrammingSupported()) {
        EXPECT_ANY_THROW(allocation.setCacheAdvice(&drm, 1024, CacheRegion::region1, false));
    } else {
        EXPECT_TRUE(allocation.setCacheAdvice(&drm, 1024, CacheRegion::region1, false));

        for (auto bo : allocation.bufferObjects) {
            if (bo != nullptr) {
                EXPECT_EQ(CacheRegion::region1, bo->peekCacheRegion());
            }
        }
    }
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenBufferObjectIsCreatedThenApplyDefaultCachePolicy) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    for (auto bo : allocation.bufferObjects) {
        if (bo != nullptr) {
            EXPECT_EQ(CachePolicy::writeBack, bo->peekCachePolicy());
        }
    }
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenSetCachePolicyIsCalledThenUpdatePolicyInBufferObject) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    allocation.setCachePolicy(CachePolicy::uncached);

    for (auto bo : allocation.bufferObjects) {
        if (bo != nullptr) {
            EXPECT_EQ(CachePolicy::uncached, bo->peekCachePolicy());
        }
    }
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenSetMemAdviseWithCachePolicyIsCalledThenUpdatePolicyInBufferObject) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    EXPECT_EQ(CachePolicy::writeBack, bo.peekCachePolicy());

    MemAdviseFlags memAdviseFlags{};
    EXPECT_TRUE(memAdviseFlags.cachedMemory);

    for (auto cached : {true, false, true}) {
        memAdviseFlags.cachedMemory = cached;

        EXPECT_TRUE(allocation.setMemAdvise(&drm, memAdviseFlags));

        EXPECT_EQ(cached ? CachePolicy::writeBack : CachePolicy::uncached, bo.peekCachePolicy());

        EXPECT_EQ(memAdviseFlags.allFlags, allocation.enabledMemAdviseFlags.allFlags);
    }
}

TEST_F(DrmAllocationTests, givenBoWhenMarkingForCaptureThenBosAreMarked) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::scratchSurface, MemoryPool::system4KBPages);
    allocation.markForCapture();

    allocation.bufferObjects[0] = &bo;
    allocation.markForCapture();

    EXPECT_TRUE(bo.isMarkedForCapture());
}

TEST_F(DrmAllocationTests, givenUncachedTypeWhenForceOverridePatIndexForUncachedTypesThenForcedByFlagPatIndexIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceAllResourcesUncached.set(1);
    uint64_t patIndex = 8u;
    debugManager.flags.OverridePatIndexForUncachedTypes.set(static_cast<int32_t>(patIndex));

    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);

    EXPECT_EQ(patIndex, drm.getPatIndex(allocation.getDefaultGmm(), allocation.getAllocationType(), CacheRegion::defaultRegion, CachePolicy::writeBack, false, false));
}

TEST_F(DrmAllocationTests, givenCachedTypeWhenForceOverridePatIndexForUncachedTypesThenPatIndexIsNotOverrideByFlag) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceAllResourcesUncached.set(0);
    uint64_t patIndex = 8u;
    debugManager.flags.OverridePatIndexForUncachedTypes.set(static_cast<int32_t>(patIndex));

    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);

    EXPECT_NE(patIndex, drm.getPatIndex(allocation.getDefaultGmm(), allocation.getAllocationType(), CacheRegion::defaultRegion, CachePolicy::writeBack, false, false));
}

TEST_F(DrmAllocationTests, givenUncachedTypeWhenForceOverridePatIndexForCachedTypesThenPatIndexIsNotOverrideByFlag) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceAllResourcesUncached.set(1);
    uint64_t patIndex = 8u;
    debugManager.flags.OverridePatIndexForCachedTypes.set(static_cast<int32_t>(patIndex));

    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);

    EXPECT_NE(patIndex, drm.getPatIndex(allocation.getDefaultGmm(), allocation.getAllocationType(), CacheRegion::defaultRegion, CachePolicy::writeBack, false, false));
}

TEST_F(DrmAllocationTests, givenCachedTypeWhenForceOverridePatIndexForCachedTypesThenForcedByFlagPatIndexIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceAllResourcesUncached.set(0);
    uint64_t patIndex = 8u;
    debugManager.flags.OverridePatIndexForCachedTypes.set(static_cast<int32_t>(patIndex));

    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);

    EXPECT_EQ(patIndex, drm.getPatIndex(allocation.getDefaultGmm(), allocation.getAllocationType(), CacheRegion::defaultRegion, CachePolicy::writeBack, false, false));
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmAllocationWithHostPtrWhenItIsCreatedWithCacheRegionThenSetRegionInBufferObject) {

    auto &productHelper = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getHelper<ProductHelper>();
    if (productHelper.getNumCacheRegions() == 0) {
        GTEST_SKIP();
    }
    mock->ioctlExpected.total = -1;
    auto drm = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->as<Drm>());

    CacheReservationParameters l2CacheParameters{};
    CacheReservationParameters l3CacheParameters{};
    l3CacheParameters.maxSize = 32 * MemoryConstants::kiloByte;
    l3CacheParameters.maxNumRegions = 2;
    l3CacheParameters.maxNumWays = 32;
    drm->cacheInfo.reset(new MockCacheInfo(*drm->getIoctlHelper(), l2CacheParameters, l3CacheParameters));

    auto ptr = reinterpret_cast<void *>(0x1000);
    auto size = MemoryConstants::pageSize;

    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = ptr;
    storage.fragmentStorageData[0].fragmentSize = 1;
    storage.fragmentCount = 1;

    memoryManager->populateOsHandles(storage, rootDeviceIndex);

    auto allocation = std::make_unique<DrmAllocation>(rootDeviceIndex, 1u /*num gmms*/, AllocationType::bufferHostMemory,
                                                      nullptr, ptr, castToUint64(ptr), size, MemoryPool::system4KBPages);
    allocation->fragmentsStorage = storage;

    allocation->setCacheAdvice(drm, 1024, CacheRegion::region1, false);

    for (uint32_t i = 0; i < storage.fragmentCount; i++) {
        auto bo = static_cast<OsHandleLinux *>(allocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage)->bo;
        EXPECT_EQ(CacheRegion::region1, bo->peekCacheRegion());
    }

    storage.fragmentStorageData[0].freeTheFragment = true;
    memoryManager->cleanOsHandles(storage, rootDeviceIndex);
}

HWTEST2_TEMPLATED_F(DrmMemoryManagerTest, givenDrmAllocationWithHostPtrWhenItIsCreatedWithIncorrectCacheRegionThenReturnNull, IsClosUnsupported) {
    mock->ioctlExpected.total = -1;
    auto drm = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->as<Drm>());
    drm->setupCacheInfo(*defaultHwInfo.get());

    auto ptr = reinterpret_cast<void *>(0x1000);
    auto size = MemoryConstants::pageSize;

    allocationData.size = size;
    allocationData.hostPtr = ptr;
    allocationData.cacheRegion = 0xFFFF;

    auto allocation = std::unique_ptr<GraphicsAllocation>(memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData));
    EXPECT_EQ(allocation, nullptr);
}

HWTEST2_TEMPLATED_F(DrmMemoryManagerTest, givenDrmAllocationWithWithAlignmentFromUserptrWhenItIsCreatedWithIncorrectCacheRegionThenReturnNull, IsClosUnsupported) {
    mock->ioctlExpected.total = -1;
    auto drm = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->as<Drm>());
    drm->setupCacheInfo(*defaultHwInfo.get());

    auto size = MemoryConstants::pageSize;
    allocationData.size = size;
    allocationData.cacheRegion = 0xFFFF;

    auto allocation = static_cast<DrmAllocation *>(memoryManager->createAllocWithAlignmentFromUserptr(allocationData, size, 0, 0, 0x1000));
    EXPECT_EQ(allocation, nullptr);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenAllocateGraphicsMemoryWithPropertiesCalledWithDebugSurfaceTypeThenDebugSurfaceIsCreated) {
    AllocationProperties debugSurfaceProperties{0, true, MemoryConstants::pageSize, NEO::AllocationType::debugContextSaveArea, false, false, 0b1011};
    auto debugSurface = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(debugSurfaceProperties));

    EXPECT_NE(nullptr, debugSurface);

    auto mem = debugSurface->getUnderlyingBuffer();
    ASSERT_NE(nullptr, mem);

    EXPECT_EQ(3u, debugSurface->getNumGmms());

    auto &bos = debugSurface->getBOs();

    EXPECT_NE(nullptr, bos[0]);
    EXPECT_NE(nullptr, bos[1]);
    EXPECT_NE(nullptr, bos[3]);

    EXPECT_EQ(debugSurface->getGpuAddress(), bos[0]->peekAddress());
    EXPECT_EQ(debugSurface->getGpuAddress(), bos[1]->peekAddress());
    EXPECT_EQ(debugSurface->getGpuAddress(), bos[3]->peekAddress());

    auto sipType = SipKernel::getSipKernelType(*device);
    SipKernel::initSipKernel(sipType, *device);

    auto &stateSaveAreaHeader = NEO::SipKernel::getSipKernel(*device, nullptr).getStateSaveAreaHeader();
    mem = ptrOffset(mem, stateSaveAreaHeader.size());
    auto size = debugSurface->getUnderlyingBufferSize() - stateSaveAreaHeader.size();

    EXPECT_TRUE(memoryZeroed(mem, size));

    SipKernel::freeSipKernels(&device->getRootDeviceEnvironmentRef(), device->getMemoryManager());
    memoryManager->freeGraphicsMemory(debugSurface);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenAffinityMaskDeviceWithBitfieldIndex1SetWhenAllocatingDebugSurfaceThenSingleAllocationWithOneBoIsCreated) {
    NEO::debugManager.flags.CreateMultipleSubDevices.set(2);

    AffinityMaskHelper affinityMask;
    affinityMask.enableGenericSubDevice(1);

    executionEnvironment->rootDeviceEnvironments[0]->deviceAffinityMask = affinityMask;
    AllocationProperties debugSurfaceProperties{0, true, MemoryConstants::pageSize, NEO::AllocationType::debugContextSaveArea, false, false, 0b0010};
    auto debugSurface = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(debugSurfaceProperties));

    EXPECT_NE(nullptr, debugSurface);

    auto mem = debugSurface->getUnderlyingBuffer();
    ASSERT_NE(nullptr, mem);

    EXPECT_EQ(1u, debugSurface->getNumGmms());

    auto &bos = debugSurface->getBOs();
    auto bo = debugSurface->getBO();

    EXPECT_NE(nullptr, bos[0]);
    EXPECT_EQ(nullptr, bos[1]);
    EXPECT_EQ(bos[0], bo);

    EXPECT_EQ(debugSurface->getGpuAddress(), bos[0]->peekAddress());

    auto sipType = SipKernel::getSipKernelType(*device);
    SipKernel::initSipKernel(sipType, *device);

    auto &stateSaveAreaHeader = NEO::SipKernel::getSipKernel(*device, nullptr).getStateSaveAreaHeader();
    mem = ptrOffset(mem, stateSaveAreaHeader.size());
    auto size = debugSurface->getUnderlyingBufferSize() - stateSaveAreaHeader.size();

    EXPECT_TRUE(memoryZeroed(mem, size));

    SipKernel::freeSipKernels(&device->getRootDeviceEnvironmentRef(), device->getMemoryManager());
    memoryManager->freeGraphicsMemory(debugSurface);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenWddmMemoryManagerIsCreatedThenAlignmentSelectorHasExpectedAlignments) {
    std::vector<AlignmentSelector::CandidateAlignment> expectedAlignments = {
        {MemoryConstants::pageSize2M, false, AlignmentSelector::anyWastage, HeapIndex::heapStandard2MB},
        {MemoryConstants::pageSize64k, true, AlignmentSelector::anyWastage, HeapIndex::heapStandard64KB},
    };

    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    EXPECT_EQ(expectedAlignments, memoryManager.alignmentSelector.peekCandidateAlignments());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenDebugFlagToNotFreeResourcesIsSpecifiedThenFreeIsNotDoingAnything) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DoNotFreeResources.set(true);
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    size_t sizeIn = 1024llu;
    uint64_t gpuAddress = 0x1337llu;
    auto drmAllocation = std::make_unique<DrmAllocation>(0u, 1u /*num gmms*/, AllocationType::buffer, nullptr, nullptr, gpuAddress, sizeIn, MemoryPool::system64KBPages);
    memoryManager.freeGraphicsMemoryImpl(drmAllocation.get());

    EXPECT_EQ(drmAllocation->getNumGmms(), 1u);
    EXPECT_EQ(drmAllocation->getRootDeviceIndex(), 0u);
    EXPECT_EQ(drmAllocation->getBOs().size(), EngineLimits::maxHandleCount);
    EXPECT_EQ(drmAllocation->getAllocationType(), AllocationType::buffer);
    EXPECT_EQ(drmAllocation->getGpuAddress(), gpuAddress);
    EXPECT_EQ(drmAllocation->getUnderlyingBufferSize(), sizeIn);
    EXPECT_EQ(drmAllocation->getMemoryPool(), MemoryPool::system64KBPages);
    EXPECT_EQ(drmAllocation->getUnderlyingBuffer(), nullptr);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, given2MbPagesDisabledWhenWddmMemoryManagerIsCreatedThenAlignmentSelectorHasExpectedAlignments) {
    DebugManagerStateRestore restore{};
    debugManager.flags.AlignLocalMemoryVaTo2MB.set(0);

    std::vector<AlignmentSelector::CandidateAlignment> expectedAlignments = {
        {MemoryConstants::pageSize64k, true, AlignmentSelector::anyWastage, HeapIndex::heapStandard64KB},
    };

    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    EXPECT_EQ(expectedAlignments, memoryManager.alignmentSelector.peekCandidateAlignments());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenCustomAlignmentWhenWddmMemoryManagerIsCreatedThenAlignmentSelectorHasExpectedAlignments) {
    DebugManagerStateRestore restore{};

    {
        debugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(MemoryConstants::megaByte);
        std::vector<AlignmentSelector::CandidateAlignment> expectedAlignments = {
            {MemoryConstants::pageSize2M, false, AlignmentSelector::anyWastage, HeapIndex::heapStandard2MB},
            {MemoryConstants::megaByte, true, AlignmentSelector::anyWastage, HeapIndex::heapStandard64KB},
            {MemoryConstants::pageSize64k, true, AlignmentSelector::anyWastage, HeapIndex::heapStandard64KB},
        };
        TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
        EXPECT_EQ(expectedAlignments, memoryManager.alignmentSelector.peekCandidateAlignments());
    }

    {
        debugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(2 * MemoryConstants::pageSize2M);
        std::vector<AlignmentSelector::CandidateAlignment> expectedAlignments = {
            {2 * MemoryConstants::pageSize2M, true, AlignmentSelector::anyWastage, HeapIndex::heapStandard2MB},
            {MemoryConstants::pageSize2M, false, AlignmentSelector::anyWastage, HeapIndex::heapStandard2MB},
            {MemoryConstants::pageSize64k, true, AlignmentSelector::anyWastage, HeapIndex::heapStandard64KB},
        };
        TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
        EXPECT_EQ(expectedAlignments, memoryManager.alignmentSelector.peekCandidateAlignments());
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmManagerWithLocalMemoryWhenGettingGlobalMemoryPercentThenCorrectValueIsReturned) {
    TestedDrmMemoryManager memoryManager(true, false, false, *executionEnvironment);
    uint32_t rootDeviceIndex = 0u;
    EXPECT_EQ(memoryManager.getPercentOfGlobalMemoryAvailable(rootDeviceIndex), 0.95);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmManagerWithoutLocalMemoryWhenGettingGlobalMemoryPercentThenCorrectValueIsReturned) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    uint32_t rootDeviceIndex = 0u;
    EXPECT_EQ(memoryManager.getPercentOfGlobalMemoryAvailable(rootDeviceIndex), 0.94);
}

struct DrmMemoryManagerToTestLockInLocalMemory : public TestedDrmMemoryManager {
    using TestedDrmMemoryManager::lockResourceImpl;
    DrmMemoryManagerToTestLockInLocalMemory(ExecutionEnvironment &executionEnvironment)
        : TestedDrmMemoryManager(true, false, false, executionEnvironment) {}

    void *lockBufferObject(BufferObject *bo) override {
        lockedLocalMemory.reset(new uint8_t[bo->peekSize()]);
        return lockedLocalMemory.get();
    }
    std::unique_ptr<uint8_t[]> lockedLocalMemory;
};

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmManagerWithLocalMemoryWhenLockResourceIsCalledOnWriteCombinedAllocationThenReturnPtrAlignedTo64Kb) {
    DrmMemoryManagerToTestLockInLocalMemory memoryManager(*executionEnvironment);
    BufferObject bo(rootDeviceIndex, mock, 3, 1, 1024, 0);

    DrmAllocation drmAllocation(0, 1u /*num gmms*/, AllocationType::writeCombined, &bo, nullptr, 0u, 0u, MemoryPool::localMemory);
    EXPECT_EQ(&bo, drmAllocation.getBO());

    auto ptr = memoryManager.lockResourceImpl(drmAllocation);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(ptr, bo.peekLockedAddress());
    EXPECT_TRUE(isAligned<MemoryConstants::pageSize64k>(ptr));

    memoryManager.unlockBufferObject(&bo);
    EXPECT_EQ(nullptr, bo.peekLockedAddress());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWithoutLocalMemoryWhenCopyMemoryToAllocationThenAllocationIsFilledWithCorrectData) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    std::vector<uint8_t> dataToCopy(MemoryConstants::pageSize, 1u);

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties({rootDeviceIndex, dataToCopy.size(), AllocationType::buffer, device->getDeviceBitfield()});
    ASSERT_NE(nullptr, allocation);

    auto ret = memoryManager.copyMemoryToAllocation(allocation, 0, dataToCopy.data(), dataToCopy.size());
    EXPECT_TRUE(ret);

    EXPECT_EQ(0, memcmp(allocation->getUnderlyingBuffer(), dataToCopy.data(), dataToCopy.size()));

    memoryManager.freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWithoutLocalMemoryAndCpuPtrWhenCopyMemoryToAllocationThenReturnFalse) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    std::vector<uint8_t> dataToCopy(MemoryConstants::pageSize, 1u);

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties({rootDeviceIndex, dataToCopy.size(), AllocationType::buffer, device->getDeviceBitfield()});
    ASSERT_NE(nullptr, allocation);
    allocation->setCpuPtrAndGpuAddress(nullptr, 0u);
    auto ret = memoryManager.copyMemoryToAllocation(allocation, 0, dataToCopy.data(), dataToCopy.size());
    EXPECT_FALSE(ret);

    memoryManager.freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenNullDefaultAllocWhenCreateGraphicsAllocationFromExistingStorageThenDoNotImportHandle) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    mock->ioctlExpected.primeFdToHandle = 0;

    MockAllocationProperties properties(0u, 1u);
    MultiGraphicsAllocation allocation(0u);
    auto alloc = memoryManager.createGraphicsAllocationFromExistingStorage(properties, nullptr, allocation);

    EXPECT_NE(alloc, nullptr);

    memoryManager.freeGraphicsMemory(alloc);
}

TEST(DrmMemoryManagerSimpleTest, givenDrmMemoryManagerWhenAllocateInDevicePoolIsCalledThenNullptrAndStatusRetryIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    TestedDrmMemoryManager memoryManager(executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.useSystemMemory = true;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

TEST(DrmMemoryManagerSimpleTest, givenDrmMemoryManagerWhenDeviceHeapIsDepletedThenNullptrReturnedAndStatusIsError) {
    constexpr size_t reservedCpuAddressRangeSize = is64bit ? (6 * 4 * MemoryConstants::gigaByte) : 0;

    auto hwInfo = defaultHwInfo.get();
    auto executionEnvironment = MockExecutionEnvironment{hwInfo};
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);

    auto mockGfxPartition = std::make_unique<MockGfxPartition>();
    mockGfxPartition->init(hwInfo->capabilityTable.gpuAddressSpace, reservedCpuAddressRangeSize, 0, 1, false, 0u, hwInfo->capabilityTable.gpuAddressSpace + 1);

    auto status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.size = MemoryConstants::pageSize + mockGfxPartition->heapGetLimit(NEO::HeapIndex::heapInternalDeviceMemory);
    allocData.flags.useSystemMemory = false;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::kernelIsa;

    auto memoryManager = TestedDrmMemoryManager{true, false, false, executionEnvironment};
    memoryManager.overrideGfxPartition(mockGfxPartition.release());
    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);

    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
}

TEST(DrmMemoryManagerSimpleTest, givenDrmMemoryManagerWhenAllocateInLocalDeviceMemoryIsCalledThenNullptrAndStatusRetryIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    TestedDrmMemoryManager memoryManager(executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.useSystemMemory = true;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager.allocatePhysicalLocalDeviceMemory(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
}

TEST(DrmMemoryManagerSimpleTest, givenDrmMemoryManagerWhenLockResourceIsCalledOnNullBufferObjectThenReturnNullPtr) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    TestedDrmMemoryManager memoryManager(executionEnvironment);
    DrmAllocation drmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, nullptr, 0u, 0u, MemoryPool::localMemory);

    auto ptr = memoryManager.lockBufferObject(drmAllocation.getBO());
    EXPECT_EQ(nullptr, ptr);

    memoryManager.unlockBufferObject(drmAllocation.getBO());
}

TEST(DrmMemoryManagerSimpleTest, givenDrmMemoryManagerWhenFreeGraphicsMemoryIsCalledOnAllocationWithNullBufferObjectThenEarlyReturn) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    TestedDrmMemoryManager memoryManager(executionEnvironment);

    auto drmAllocation = new DrmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, nullptr, 0u, 0u, MemoryPool::localMemory);
    EXPECT_NE(nullptr, drmAllocation);

    memoryManager.freeGraphicsMemoryImpl(drmAllocation);
}

TEST(DrmMemoryManagerSimpleTest, WhenDrmIsCreatedThenQueryPageFaultSupportIsCalled) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = std::unique_ptr<Drm>(Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[0]));

    EXPECT_TRUE(static_cast<DrmMock *>(drm.get())->queryPageFaultSupportCalled);
}

using DrmMemoryManagerWithLocalMemoryTest = Test<DrmMemoryManagerWithLocalMemoryFixture>;

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenDrmMemoryManagerWithoutLocalMemoryWhenPreferCompressedIsSetThenLocalOnlyRequriedDeterminedByReleaseHelper) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);

    AllocationProperties properties{1, true, 4096, AllocationType::unknown, false, 0b10};
    properties.flags.preferCompressed = true;
    auto storageInfo = memoryManager.createStorageInfoFromProperties(properties);

    const auto *releaseHelper{executionEnvironment->rootDeviceEnvironments[0]->getReleaseHelper()};
    EXPECT_EQ(storageInfo.localOnlyRequired, (!releaseHelper || releaseHelper->isLocalOnlyAllowed()));
}

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenDrmMemoryManagerWithLocalMemoryWhenLockResourceIsCalledOnAllocationInLocalMemoryThenReturnNullPtr) {
    DrmAllocation drmAllocation(rootDeviceIndex, 1u /*num gmms*/, AllocationType::unknown, nullptr, nullptr, 0u, 0u, MemoryPool::localMemory);

    auto ptr = memoryManager->lockResource(&drmAllocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(&drmAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenSingleLocalMemoryWhenParticularSubdeviceIndicatedThenCorrectBankIsSelected) {
    auto *memoryInfo = static_cast<MockMemoryInfo *>(mock->memoryInfo.get());
    auto &localMemoryRegions = memoryInfo->localMemoryRegions;
    localMemoryRegions.resize(1U);
    localMemoryRegions[0].tilesMask = 0b11;

    AllocationProperties properties{1, true, 4096, AllocationType::buffer, false, {}};
    properties.subDevicesBitfield = 0b10;

    memoryManager->computeStorageInfoMemoryBanksCalled = 0U;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);

    constexpr auto expectedMemoryBanks = 0b01;
    EXPECT_EQ(storageInfo.memoryBanks, expectedMemoryBanks);
    EXPECT_EQ(memoryManager->computeStorageInfoMemoryBanksCalled, 2UL);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenSingleLocalMemoryAndNonTileInstancedAllocationWhenAllTilesIndicatedThenCorrectBankIsSelected) {
    auto *memoryInfo = static_cast<MockMemoryInfo *>(mock->memoryInfo.get());
    auto &localMemoryRegions = memoryInfo->localMemoryRegions;
    localMemoryRegions.resize(1U);
    localMemoryRegions[0].tilesMask = 0b11;

    AllocationProperties properties{1, true, 4096, AllocationType::buffer, false, {}};
    properties.subDevicesBitfield = 0b11;

    memoryManager->computeStorageInfoMemoryBanksCalled = 0U;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);

    constexpr auto expectedMemoryBanks = 0b01;
    EXPECT_EQ(storageInfo.memoryBanks, expectedMemoryBanks);
    EXPECT_EQ(memoryManager->computeStorageInfoMemoryBanksCalled, 2UL);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenSingleLocalMemoryWhenTileInstancedAllocationCreatedThenMemoryBanksSetToAllTilesRegardlessOfSubDevicesIndicatedInProperties) {
    auto *memoryInfo = static_cast<MockMemoryInfo *>(mock->memoryInfo.get());
    auto &localMemoryRegions = memoryInfo->localMemoryRegions;
    debugManager.flags.CreateMultipleSubDevices.set(2);
    localMemoryRegions.resize(1U);
    localMemoryRegions[0].tilesMask = 0b11;

    const auto expectedMemoryBanks = 0b11;
    for (auto subDevicesMask = 1U; subDevicesMask < 4; ++subDevicesMask) {
        AllocationProperties properties{1, 4096, AllocationType::workPartitionSurface, subDevicesMask};

        memoryManager->computeStorageInfoMemoryBanksCalled = 0U;
        auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);

        EXPECT_TRUE(storageInfo.tileInstanced);
        EXPECT_EQ(storageInfo.memoryBanks, expectedMemoryBanks);
        EXPECT_EQ(memoryManager->computeStorageInfoMemoryBanksCalled, 2UL);
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenSingleLocalMemoryAndTileInstancedAllocationTypeEvenWhenSubsetOfTilesIndicatedThenCorrectBankIsSelected) {
    auto *memoryInfo = static_cast<MockMemoryInfo *>(mock->memoryInfo.get());
    auto &localMemoryRegions = memoryInfo->localMemoryRegions;
    debugManager.flags.CreateMultipleSubDevices.set(2);

    localMemoryRegions.resize(1U);
    localMemoryRegions[0].tilesMask = 0b11;

    AllocationProperties properties{1, true, 4096, AllocationType::workPartitionSurface, false, 0b10};
    const auto expectedMemoryBanks = 0b11;

    memoryManager->computeStorageInfoMemoryBanksCalled = 0U;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);

    EXPECT_EQ(storageInfo.memoryBanks, expectedMemoryBanks);
    EXPECT_EQ(memoryManager->computeStorageInfoMemoryBanksCalled, 2UL);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenSingleLocalMemoryWhenTileInstancedAllocationCreatedThenItHasCorrectBOs) {
    auto *memoryInfo = static_cast<MockMemoryInfo *>(mock->memoryInfo.get());
    auto &localMemoryRegions = memoryInfo->localMemoryRegions;
    localMemoryRegions.resize(1U);
    localMemoryRegions[0].tilesMask = 0b11;

    const DeviceBitfield subDeviceBitfield{0b11};
    const auto expectedNumHandles{subDeviceBitfield.count()};
    EXPECT_NE(expectedNumHandles, 0UL);

    AllocationProperties properties{1, 4096, AllocationType::workPartitionSurface, subDeviceBitfield};
    auto *allocation{memoryManager->allocateGraphicsMemoryInPreferredPool(properties, nullptr)};
    EXPECT_EQ(allocation->getNumHandles(), expectedNumHandles);

    const auto &bos{static_cast<DrmAllocation *>(allocation)->getBOs()};
    const auto commonBoAddress{bos[0U]->peekAddress()};
    auto numberOfValidBos{0U};
    for (const auto bo : bos) {
        if (bo == nullptr) {
            continue;
        }
        ++numberOfValidBos;
        EXPECT_EQ(bo->peekAddress(), commonBoAddress);
    }
    EXPECT_EQ(numberOfValidBos, expectedNumHandles);
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenMultipleLocalMemoryRegionsWhenParticularSubdeviceIndicatedThenItIsSelected) {
    auto *memoryInfo = static_cast<MockMemoryInfo *>(mock->memoryInfo.get());
    auto &localMemoryRegions = memoryInfo->localMemoryRegions;
    constexpr auto leastOccupiedBankBitPosition = 1u;
    debugManager.flags.OverrideLeastOccupiedBank.set(leastOccupiedBankBitPosition);

    localMemoryRegions.resize(2U);
    localMemoryRegions[0].tilesMask = 0b01;
    localMemoryRegions[1].tilesMask = 0b10;

    AllocationProperties properties{1, true, 4096, AllocationType::buffer, false, {}};
    properties.subDevicesBitfield = 0b10;
    const auto expectedMemoryBanks = properties.subDevicesBitfield;

    memoryManager->computeStorageInfoMemoryBanksCalled = 0U;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);

    EXPECT_EQ(storageInfo.memoryBanks, expectedMemoryBanks);
    EXPECT_EQ(memoryManager->computeStorageInfoMemoryBanksCalled, 2UL);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenMultipleLocalMemoryRegionsWhenAllSubdevicesIndicatedThenTheLeastOccuppiedBankIsSelected) {
    auto *memoryInfo = static_cast<MockMemoryInfo *>(mock->memoryInfo.get());
    auto &localMemoryRegions = memoryInfo->localMemoryRegions;
    constexpr auto leastOccupiedBankBitPosition = 1u;
    debugManager.flags.OverrideLeastOccupiedBank.set(leastOccupiedBankBitPosition);

    localMemoryRegions.resize(2U);
    localMemoryRegions[0].tilesMask = 0b01;
    localMemoryRegions[1].tilesMask = 0b10;

    AllocationProperties properties{1, true, 4096, AllocationType::buffer, false, {}};
    properties.subDevicesBitfield = 0b11;

    const auto expectedMemoryBanks = memoryManager->createStorageInfoFromPropertiesGeneric(properties).memoryBanks;

    memoryManager->computeStorageInfoMemoryBanksCalled = 0U;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);

    EXPECT_EQ(storageInfo.memoryBanks, expectedMemoryBanks);
    EXPECT_EQ(memoryManager->computeStorageInfoMemoryBanksCalled, 2UL);
}

using DrmMemoryManagerTest = Test<DrmMemoryManagerFixture>;

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenCopyMemoryToAllocationThenAllocationIsFilledWithCorrectData) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    std::vector<uint8_t> dataToCopy(MemoryConstants::pageSize, 1u);

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties({rootDeviceIndex, dataToCopy.size(), AllocationType::buffer, device->getDeviceBitfield()});
    ASSERT_NE(nullptr, allocation);

    auto ret = memoryManager->copyMemoryToAllocation(allocation, 0, dataToCopy.data(), dataToCopy.size());
    EXPECT_TRUE(ret);

    EXPECT_EQ(0, memcmp(allocation->getUnderlyingBuffer(), dataToCopy.data(), dataToCopy.size()));

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenFreeingImportedMemoryThenCloseSharedHandleIsNotCalled) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    std::vector<uint8_t> dataToCopy(MemoryConstants::pageSize, 1u);

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties({rootDeviceIndex, dataToCopy.size(), AllocationType::buffer, device->getDeviceBitfield()});
    ASSERT_NE(nullptr, allocation);

    memoryManager->freeGraphicsMemory(allocation, true);

    EXPECT_EQ(memoryManager->callsToCloseSharedHandle, 0u);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenFreeingNonImportedMemoryThenCloseSharedHandleIsCalled) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    std::vector<uint8_t> dataToCopy(MemoryConstants::pageSize, 1u);

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties({rootDeviceIndex, dataToCopy.size(), AllocationType::buffer, device->getDeviceBitfield()});
    ASSERT_NE(nullptr, allocation);

    memoryManager->freeGraphicsMemory(allocation);

    EXPECT_EQ(memoryManager->callsToCloseSharedHandle, 1u);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenGetLocalMemoryIsCalledThenSizeOfLocalMemoryIsReturned) {
    EXPECT_EQ(0 * MemoryConstants::gigaByte, memoryManager->getLocalMemorySize(rootDeviceIndex, 0xF));
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetMemAdviseIsCalledThenUpdateCachePolicyInBufferObject) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    BufferObject bo(rootDeviceIndex, mock, 3, 1, 1024, 0);

    DrmAllocation drmAllocation(rootDeviceIndex, 1u /*num gmms*/, AllocationType::unifiedSharedMemory, &bo, nullptr, 0u, 0u, MemoryPool::localMemory);
    EXPECT_EQ(&bo, drmAllocation.getBO());

    for (auto isCached : {false, true}) {
        MemAdviseFlags flags{};
        flags.cachedMemory = isCached;

        EXPECT_TRUE(memoryManager.setMemAdvise(&drmAllocation, flags, rootDeviceIndex));
        EXPECT_EQ(isCached ? CachePolicy::writeBack : CachePolicy::uncached, bo.peekCachePolicy());
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetSharedSystemMemAdviseIsCalledThenAdviceSentToIoctlHelper) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);

    class MyMockIoctlHelper : public MockIoctlHelper {
        using MockIoctlHelper::MockIoctlHelper;

      public:
        bool setVmSharedSystemMemAdvise(uint64_t handle, const size_t size, const uint32_t attribute, const uint64_t param, const std::vector<uint32_t> &vmIds) override {
            setVmSharedSystemMemAdviseCalled++;
            return true;
        }
        uint32_t setVmSharedSystemMemAdviseCalled = 0;
    };

    auto mockIoctlHelper = new MyMockIoctlHelper(*mock);

    auto &drm = static_cast<DrmMockCustom &>(memoryManager.getDrm(mockRootDeviceIndex));
    drm.ioctlHelper.reset(mockIoctlHelper);

    auto subDeviceIds = NEO::SubDeviceIdsVec{0};
    MemAdvise memAdviseOp = MemAdvise::setPreferredLocation;
    EXPECT_TRUE(memoryManager.setSharedSystemMemAdvise(nullptr, 0u, memAdviseOp, subDeviceIds, 0u));
    EXPECT_EQ(1u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);

    memAdviseOp = MemAdvise::clearPreferredLocation;
    EXPECT_TRUE(memoryManager.setSharedSystemMemAdvise(nullptr, 0u, memAdviseOp, subDeviceIds, 0u));
    EXPECT_EQ(2u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);

    memAdviseOp = MemAdvise::setSystemMemoryPreferredLocation;
    EXPECT_TRUE(memoryManager.setSharedSystemMemAdvise(nullptr, 0u, memAdviseOp, subDeviceIds, 0u));
    EXPECT_EQ(3u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);

    memAdviseOp = MemAdvise::clearSystemMemoryPreferredLocation;
    EXPECT_TRUE(memoryManager.setSharedSystemMemAdvise(nullptr, 0u, memAdviseOp, subDeviceIds, 0u));
    EXPECT_EQ(4u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);

    memAdviseOp = MemAdvise::invalidAdvise;
    EXPECT_FALSE(memoryManager.setSharedSystemMemAdvise(nullptr, 0u, memAdviseOp, subDeviceIds, 0u));
    EXPECT_EQ(4u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetSharedSystemAtomicAccessIsCalledThenAdviceSentToIoctlHelper) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);

    class MyMockIoctlHelper : public MockIoctlHelper {
        using MockIoctlHelper::MockIoctlHelper;

      public:
        bool setVmSharedSystemMemAdvise(uint64_t handle, const size_t size, const uint32_t attribute, const uint64_t param, const std::vector<uint32_t> &vmIds) override {
            setVmSharedSystemMemAdviseCalled++;
            return true;
        }
        uint32_t setVmSharedSystemMemAdviseCalled = 0;
    };

    auto mockIoctlHelper = new MyMockIoctlHelper(*mock);

    auto &drm = static_cast<DrmMockCustom &>(memoryManager.getDrm(mockRootDeviceIndex));
    drm.ioctlHelper.reset(mockIoctlHelper);

    auto subDeviceIds = NEO::SubDeviceIdsVec{0};
    AtomicAccessMode mode = AtomicAccessMode::device;
    EXPECT_TRUE(memoryManager.setSharedSystemAtomicAccess(nullptr, 0u, mode, subDeviceIds, 0u));
    EXPECT_EQ(1u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);

    mode = AtomicAccessMode::system;
    EXPECT_TRUE(memoryManager.setSharedSystemAtomicAccess(nullptr, 0u, mode, subDeviceIds, 0u));
    EXPECT_EQ(2u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);

    mode = AtomicAccessMode::host;
    EXPECT_TRUE(memoryManager.setSharedSystemAtomicAccess(nullptr, 0u, mode, subDeviceIds, 0u));
    EXPECT_EQ(3u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);

    mode = AtomicAccessMode::none;
    EXPECT_TRUE(memoryManager.setSharedSystemAtomicAccess(nullptr, 0u, mode, subDeviceIds, 0u));
    EXPECT_EQ(4u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);

    mode = AtomicAccessMode::invalid;
    EXPECT_FALSE(memoryManager.setSharedSystemAtomicAccess(nullptr, 0u, mode, subDeviceIds, 0u));
    EXPECT_EQ(4u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetAtomicAccessIsCalledThenTrueReturned) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    BufferObject bo(rootDeviceIndex, mock, 3, 1, 1024, 0);

    DrmAllocation drmAllocation(rootDeviceIndex, 1u /*num gmms*/, AllocationType::unifiedSharedMemory, &bo, nullptr, 0u, 0u, MemoryPool::localMemory);
    EXPECT_EQ(&bo, drmAllocation.getBO());

    size_t size = 16;
    AtomicAccessMode mode = AtomicAccessMode::none;
    EXPECT_TRUE(memoryManager.setAtomicAccess(&drmAllocation, size, mode, rootDeviceIndex));

    mode = AtomicAccessMode::device;
    EXPECT_TRUE(memoryManager.setAtomicAccess(&drmAllocation, size, mode, rootDeviceIndex));

    mode = AtomicAccessMode::system;
    EXPECT_TRUE(memoryManager.setAtomicAccess(&drmAllocation, size, mode, rootDeviceIndex));

    mode = AtomicAccessMode::host;
    EXPECT_TRUE(memoryManager.setAtomicAccess(&drmAllocation, size, mode, rootDeviceIndex));
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenKmdMigratedSharedAllocationWithMultipleBOsWhenSetMemPrefetchIsCalledWithSubDevicesThenPrefetchBOsToTheseSubDevices) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    SubDeviceIdsVec subDeviceIds{0, 1};
    BufferObject bo0(rootDeviceIndex, mock, 3, 1, 1024, 0);
    BufferObject bo1(rootDeviceIndex, mock, 3, 2, 1024, 0);
    BufferObjects bos{&bo0, &bo1};

    MockDrmAllocation drmAllocation(AllocationType::unifiedSharedMemory, MemoryPool::localMemory, bos);
    drmAllocation.storageInfo.memoryBanks = 0x3;
    drmAllocation.setNumHandles(2);

    memoryManager.allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager.getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }

    EXPECT_TRUE(memoryManager.setMemPrefetch(&drmAllocation, subDeviceIds, rootDeviceIndex));

    EXPECT_TRUE(drmAllocation.bindBOsCalled);
    EXPECT_TRUE(drmAllocation.prefetchBOCalled);

    ASSERT_EQ(4u, drmAllocation.vmHandleIdsReceived.size());
    ASSERT_EQ(4u, drmAllocation.subDeviceIdsReceived.size());

    EXPECT_EQ(0u, drmAllocation.vmHandleIdsReceived[0]);
    EXPECT_EQ(0u, drmAllocation.subDeviceIdsReceived[0]);

    EXPECT_EQ(1u, drmAllocation.vmHandleIdsReceived[1]);
    EXPECT_EQ(0u, drmAllocation.subDeviceIdsReceived[1]);

    EXPECT_EQ(0u, drmAllocation.vmHandleIdsReceived[2]);
    EXPECT_EQ(1u, drmAllocation.subDeviceIdsReceived[2]);

    EXPECT_EQ(1u, drmAllocation.vmHandleIdsReceived[3]);
    EXPECT_EQ(1u, drmAllocation.subDeviceIdsReceived[3]);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenKmdMigratedSharedAllocationWithMultipleBOsWhenSetMemPrefetchIsCalledWithASubDeviceThenPrefetchBOsToTheSubDevices) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    SubDeviceIdsVec subDeviceIds{0};
    BufferObject bo0(rootDeviceIndex, mock, 3, 1, 1024, 0);
    BufferObject bo1(rootDeviceIndex, mock, 3, 2, 1024, 0);
    BufferObjects bos{&bo0, &bo1};

    MockDrmAllocation drmAllocation(AllocationType::unifiedSharedMemory, MemoryPool::localMemory, bos);
    drmAllocation.storageInfo.memoryBanks = 0x3;
    drmAllocation.setNumHandles(2);

    memoryManager.allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager.getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }

    EXPECT_TRUE(memoryManager.setMemPrefetch(&drmAllocation, subDeviceIds, rootDeviceIndex));

    EXPECT_TRUE(drmAllocation.bindBOsCalled);
    EXPECT_TRUE(drmAllocation.prefetchBOCalled);

    ASSERT_EQ(2u, drmAllocation.vmHandleIdsReceived.size());
    ASSERT_EQ(2u, drmAllocation.subDeviceIdsReceived.size());

    EXPECT_EQ(0u, drmAllocation.vmHandleIdsReceived[0]);
    EXPECT_EQ(0u, drmAllocation.subDeviceIdsReceived[0]);

    EXPECT_EQ(0u, drmAllocation.vmHandleIdsReceived[1]);
    EXPECT_EQ(1u, drmAllocation.subDeviceIdsReceived[1]);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenKMDSupportForCrossTileMigrationPolicyAndKmdMigratedSharedAllocationWithMultipleBOsWhenSetMemPrefetchIsCalledWithASubDeviceThenPrefetchBOsToThisSubDevice) {
    DebugManagerStateRestore restore;
    debugManager.flags.KMDSupportForCrossTileMigrationPolicy.set(1);

    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    SubDeviceIdsVec subDeviceIds{0};
    BufferObject bo0(rootDeviceIndex, mock, 3, 1, 1024, 0);
    BufferObject bo1(rootDeviceIndex, mock, 3, 2, 1024, 0);
    BufferObjects bos{&bo0, &bo1};

    MockDrmAllocation drmAllocation(AllocationType::unifiedSharedMemory, MemoryPool::localMemory, bos);
    drmAllocation.storageInfo.memoryBanks = 0x3;
    drmAllocation.setNumHandles(2);

    memoryManager.allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager.getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }

    EXPECT_TRUE(memoryManager.setMemPrefetch(&drmAllocation, subDeviceIds, rootDeviceIndex));

    EXPECT_TRUE(drmAllocation.bindBOsCalled);
    EXPECT_TRUE(drmAllocation.prefetchBOCalled);

    ASSERT_EQ(2u, drmAllocation.vmHandleIdsReceived.size());
    ASSERT_EQ(2u, drmAllocation.subDeviceIdsReceived.size());

    EXPECT_EQ(0u, drmAllocation.vmHandleIdsReceived[0]);
    EXPECT_EQ(0u, drmAllocation.subDeviceIdsReceived[0]);

    EXPECT_EQ(0u, drmAllocation.vmHandleIdsReceived[1]);
    EXPECT_EQ(0u, drmAllocation.subDeviceIdsReceived[1]);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenKMDSupportForCrossTileMigrationPolicyAndKmdMigratedSharedAllocationWithMultipleBOsWhenSetMemPrefetchIsCalledWithSubDevicesThenPrefetchBOsToTheseSubDevices) {
    DebugManagerStateRestore restore;
    debugManager.flags.KMDSupportForCrossTileMigrationPolicy.set(1);

    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    SubDeviceIdsVec subDeviceIds{0, 1};
    BufferObject bo0(rootDeviceIndex, mock, 3, 1, 1024, 0);
    BufferObject bo1(rootDeviceIndex, mock, 3, 2, 1024, 0);
    BufferObjects bos{&bo0, &bo1};

    MockDrmAllocation drmAllocation(AllocationType::unifiedSharedMemory, MemoryPool::localMemory, bos);
    drmAllocation.storageInfo.memoryBanks = 0x3;
    drmAllocation.setNumHandles(2);

    memoryManager.allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager.getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }

    EXPECT_TRUE(memoryManager.setMemPrefetch(&drmAllocation, subDeviceIds, rootDeviceIndex));

    EXPECT_TRUE(drmAllocation.bindBOsCalled);
    EXPECT_TRUE(drmAllocation.prefetchBOCalled);

    ASSERT_EQ(4u, drmAllocation.vmHandleIdsReceived.size());
    ASSERT_EQ(4u, drmAllocation.subDeviceIdsReceived.size());

    EXPECT_EQ(0u, drmAllocation.vmHandleIdsReceived[0]);
    EXPECT_EQ(0u, drmAllocation.subDeviceIdsReceived[0]);

    EXPECT_EQ(1u, drmAllocation.vmHandleIdsReceived[1]);
    EXPECT_EQ(0u, drmAllocation.subDeviceIdsReceived[1]);

    EXPECT_EQ(0u, drmAllocation.vmHandleIdsReceived[2]);
    EXPECT_EQ(1u, drmAllocation.subDeviceIdsReceived[2]);

    EXPECT_EQ(1u, drmAllocation.vmHandleIdsReceived[3]);
    EXPECT_EQ(1u, drmAllocation.subDeviceIdsReceived[3]);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenKmdMigratedSharedAllocationWithSingleBOWhenSetMemPrefetchIsCalledWithASubDeviceThenPrefetchBOToThisSubdevice) {
    const uint32_t rootDeviceIndex = this->device->getRootDeviceIndex();
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    SubDeviceIdsVec subDeviceIds{0};
    BufferObject bo(rootDeviceIndex, mock, 3, 1, 1024, 0);

    MockDrmAllocation drmAllocation(rootDeviceIndex, AllocationType::unifiedSharedMemory, MemoryPool::localMemory);
    drmAllocation.bufferObjects[0] = &bo;
    drmAllocation.setNumHandles(1);

    memoryManager.allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager.getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }

    EXPECT_TRUE(memoryManager.setMemPrefetch(&drmAllocation, subDeviceIds, rootDeviceIndex));

    EXPECT_TRUE(drmAllocation.bindBOsCalled);
    EXPECT_TRUE(drmAllocation.prefetchBOCalled);

    ASSERT_EQ(1u, drmAllocation.vmHandleIdsReceived.size());
    ASSERT_EQ(1u, drmAllocation.subDeviceIdsReceived.size());

    EXPECT_EQ(0u, drmAllocation.vmHandleIdsReceived[0]);
    EXPECT_EQ(0u, drmAllocation.subDeviceIdsReceived[0]);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetMemPrefetchFailsToBindBufferObjectThenReturnFalse) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    SubDeviceIdsVec subDeviceIds{0};
    BufferObject bo(rootDeviceIndex, mock, 3, 1, 1024, 0);

    MockDrmAllocation drmAllocation(rootDeviceIndex, AllocationType::unifiedSharedMemory, MemoryPool::localMemory);
    drmAllocation.bufferObjects[0] = &bo;

    memoryManager.allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager.getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }

    drmAllocation.bindBOsRetValue = -1;

    EXPECT_FALSE(memoryManager.setMemPrefetch(&drmAllocation, subDeviceIds, rootDeviceIndex));

    EXPECT_TRUE(drmAllocation.bindBOsCalled);
    EXPECT_FALSE(drmAllocation.prefetchBOCalled);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenPrefetchSharedSystemAllocIsCalledThenReturnTrue) {
    SubDeviceIdsVec subDeviceIds{0};
    class MyMockIoctlHelper : public MockIoctlHelper {
      public:
        using MockIoctlHelper::MockIoctlHelper;

        bool setVmPrefetch(uint64_t start, uint64_t length, uint32_t region, uint32_t vmId) override {
            return true;
        }
    };
    auto mockIoctlHelper = new MyMockIoctlHelper(*mock);

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    drm.ioctlHelper.reset(mockIoctlHelper);

    auto ptr = malloc(1024);

    EXPECT_TRUE(memoryManager->prefetchSharedSystemAlloc(ptr, 1024, subDeviceIds, rootDeviceIndex));

    free(ptr);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenPrefetchSharedSystemAllocIsCalledThenReturnFalse) {
    SubDeviceIdsVec subDeviceIds{0};
    class MyMockIoctlHelper : public MockIoctlHelper {
      public:
        using MockIoctlHelper::MockIoctlHelper;

        bool setVmPrefetch(uint64_t start, uint64_t length, uint32_t region, uint32_t vmId) override {
            return false;
        }
    };
    auto mockIoctlHelper = new MyMockIoctlHelper(*mock);

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    drm.ioctlHelper.reset(mockIoctlHelper);

    auto ptr = malloc(1024);

    EXPECT_TRUE(memoryManager->prefetchSharedSystemAlloc(ptr, 1024, subDeviceIds, rootDeviceIndex));

    free(ptr);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenPageFaultIsUnSupportedWhenCallingBindBoOnBufferAllocationThenAllocationShouldNotPageFaultAndExplicitResidencyIsNotRequired) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    EXPECT_FALSE(drm.pageFaultSupported);

    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);
    uint32_t vmHandleId = 0;

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    std::vector<BufferObject *> bufferObjects;
    allocation.bindBO(&bo, &osContext, vmHandleId, &bufferObjects, true, false);

    EXPECT_FALSE(allocation.shouldAllocationPageFault(&drm));
    EXPECT_FALSE(bo.isExplicitResidencyRequired());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenPageFaultIsSupportedAndKmdMigrationEnabledForBuffersWhenCallingBindBoOnBufferAllocationThenAllocationShouldPageFaultAndExplicitResidencyIsNotRequired) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableRecoverablePageFaults.set(true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    EXPECT_FALSE(drm.pageFaultSupported);

    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);
    uint32_t vmHandleId = 0;

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    for (auto useKmdMigrationForBuffers : {-1, 0, 1}) {
        debugManager.flags.UseKmdMigrationForBuffers.set(useKmdMigrationForBuffers);

        std::vector<BufferObject *> bufferObjects;
        allocation.bindBO(&bo, &osContext, vmHandleId, &bufferObjects, true, false);

        if (useKmdMigrationForBuffers > 0) {
            EXPECT_TRUE(allocation.shouldAllocationPageFault(&drm));
            EXPECT_FALSE(bo.isExplicitResidencyRequired());
        } else {
            EXPECT_FALSE(allocation.shouldAllocationPageFault(&drm));
            EXPECT_TRUE(bo.isExplicitResidencyRequired());
        }
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenPageFaultIsSupportedWhenCallingBindBoOnAllocationThatShouldPageFaultThenExplicitResidencyIsNotRequired) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    drm.pageFaultSupported = true;

    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);
    uint32_t vmHandleId = 0;

    struct MockDrmAllocationToTestPageFault : MockDrmAllocation {
        MockDrmAllocationToTestPageFault() : MockDrmAllocation(0u, AllocationType::buffer, MemoryPool::localMemory){};
        bool shouldAllocationPageFault(const Drm *drm) override {
            return shouldPageFault;
        }
        bool shouldPageFault = false;
    };

    for (auto shouldAllocationPageFault : {false, true}) {
        MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
        MockDrmAllocationToTestPageFault allocation;
        allocation.bufferObjects[0] = &bo;
        allocation.shouldPageFault = shouldAllocationPageFault;

        std::vector<BufferObject *> bufferObjects;
        allocation.bindBO(&bo, &osContext, vmHandleId, &bufferObjects, true, false);

        EXPECT_EQ(shouldAllocationPageFault, allocation.shouldAllocationPageFault(&drm));
        EXPECT_EQ(!shouldAllocationPageFault, bo.isExplicitResidencyRequired());
    }
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenGetSizeOfChunkFor2ChunksThenActualValueReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.NumberOfBOChunks.set(2);
    size_t allocSize = 2097152;
    size_t expectedSize = 1048576;
    size_t chunkSize = memoryManager->getSizeOfChunk(allocSize);
    EXPECT_EQ(expectedSize, chunkSize);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenGetSizeOfChunkFor3ChunksThenCorrectedValueReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.NumberOfBOChunks.set(3);
    size_t allocSize = 2097152;
    size_t expectedSize = 1048576;
    size_t chunkSize = memoryManager->getSizeOfChunk(allocSize);
    EXPECT_EQ(expectedSize, chunkSize);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenGetSizeOfChunkFor6ChunksThenCorrectedValueReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.NumberOfBOChunks.set(6);
    size_t allocSize = 2097152;
    size_t expectedSize = 524288;
    size_t chunkSize = memoryManager->getSizeOfChunk(allocSize);
    EXPECT_EQ(expectedSize, chunkSize);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenGetSizeOfChunkForUnevenChunksThenCorrectedValueReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.NumberOfBOChunks.set(2);
    size_t allocSize = 2162688;
    size_t expectedSize = 720896;
    size_t chunkSize = memoryManager->getSizeOfChunk(allocSize);
    EXPECT_EQ(expectedSize, chunkSize);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenGetSizeOfChunkFor1ChunkThenDefaultMinimumChunkSizeReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.NumberOfBOChunks.set(1);
    size_t allocSize = 2097152;
    size_t expectedSize = 65536;
    size_t chunkSize = memoryManager->getSizeOfChunk(allocSize);
    EXPECT_EQ(expectedSize, chunkSize);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenGetSizeOfChunkForTooManyChunksThenDefaultMinimumChunkSizeReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.NumberOfBOChunks.set(10000);
    size_t allocSize = 2097152;
    size_t expectedSize = 65536;
    size_t chunkSize = memoryManager->getSizeOfChunk(allocSize);
    EXPECT_EQ(expectedSize, chunkSize);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenSetChunkSizeThenSameSizeReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetBOChunkingSize.set(65536);
    size_t allocSize = 2097152;
    size_t expectedSize = 65536;
    size_t chunkSize = memoryManager->getSizeOfChunk(allocSize);
    EXPECT_EQ(expectedSize, chunkSize);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenSetChunkSizeThenCorrectedSizeReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetBOChunkingSize.set(100000);
    size_t allocSize = 2097152;
    size_t expectedSize = 65536;
    size_t chunkSize = memoryManager->getSizeOfChunk(allocSize);
    EXPECT_EQ(expectedSize, chunkSize);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenSetChunkSizeTooLargeThenCorrectedSizeReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetBOChunkingSize.set(4000000);
    size_t allocSize = 2097152;
    size_t expectedSize = 1048576;
    size_t chunkSize = memoryManager->getSizeOfChunk(allocSize);
    EXPECT_EQ(expectedSize, chunkSize);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenSetChunkSizeTooSmallThenCorrectedSizeReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetBOChunkingSize.set(4000);
    size_t allocSize = 2097152;
    size_t expectedSize = 65536;
    size_t chunkSize = memoryManager->getSizeOfChunk(allocSize);
    EXPECT_EQ(expectedSize, chunkSize);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenCheckAllocationForChunkingReturnTrue) {
    size_t allocSize = 2097152;
    size_t minSize = 2097152;
    bool subDeviceEnabled = true;
    bool debugDisabled = true;
    bool modeEnabled = true;
    bool bufferEnabled = true;
    EXPECT_TRUE(memoryManager->checkAllocationForChunking(allocSize, minSize, subDeviceEnabled, debugDisabled, modeEnabled, bufferEnabled));
    minSize = 1048576;
    EXPECT_TRUE(memoryManager->checkAllocationForChunking(allocSize, minSize, subDeviceEnabled, debugDisabled, modeEnabled, bufferEnabled));
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenCheckAllocationForChunkingWithImproperAllocSizeReturnFalse) {
    size_t allocSize = 2098000;
    size_t minSize = 2097152;
    bool subDeviceEnabled = true;
    bool debugDisabled = true;
    bool modeEnabled = true;
    bool bufferEnabled = true;
    EXPECT_FALSE(memoryManager->checkAllocationForChunking(allocSize, minSize, subDeviceEnabled, debugDisabled, modeEnabled, bufferEnabled));
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenCheckAllocationForChunkingWithUnevenNumberChunksReturnsFalse) {
    size_t allocSize = 2162688;
    size_t minSize = 2097152;
    bool subDeviceEnabled = true;
    bool debugDisabled = true;
    bool modeEnabled = true;
    bool bufferEnabled = true;
    EXPECT_FALSE(memoryManager->checkAllocationForChunking(allocSize, minSize, subDeviceEnabled, debugDisabled, modeEnabled, bufferEnabled));
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenCheckAllocationForChunkingWithAllocationLessThanMinSizeReturnsFalse) {
    size_t allocSize = 100000;
    size_t minSize = 2097152;
    bool subDeviceEnabled = true;
    bool debugDisabled = true;
    bool modeEnabled = true;
    bool bufferEnabled = true;
    EXPECT_FALSE(memoryManager->checkAllocationForChunking(allocSize, minSize, subDeviceEnabled, debugDisabled, modeEnabled, bufferEnabled));
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenCheckAllocationForChunkingWithBooleanInputFalseReturnsFalse) {
    size_t allocSize = 2097152;
    size_t minSize = 2097152;
    bool subDeviceEnabled = true;
    bool debugDisabled = true;
    bool modeEnabled = true;
    bool bufferEnabled = true;
    EXPECT_TRUE(memoryManager->checkAllocationForChunking(allocSize, minSize, subDeviceEnabled, debugDisabled, modeEnabled, bufferEnabled));
    subDeviceEnabled = false;
    EXPECT_FALSE(memoryManager->checkAllocationForChunking(allocSize, minSize, subDeviceEnabled, debugDisabled, modeEnabled, bufferEnabled));
    subDeviceEnabled = true;
    debugDisabled = false;
    EXPECT_FALSE(memoryManager->checkAllocationForChunking(allocSize, minSize, subDeviceEnabled, debugDisabled, modeEnabled, bufferEnabled));
    debugDisabled = true;
    modeEnabled = false;
    EXPECT_FALSE(memoryManager->checkAllocationForChunking(allocSize, minSize, subDeviceEnabled, debugDisabled, modeEnabled, bufferEnabled));
    modeEnabled = true;
    bufferEnabled = false;
    EXPECT_FALSE(memoryManager->checkAllocationForChunking(allocSize, minSize, subDeviceEnabled, debugDisabled, modeEnabled, bufferEnabled));
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenCreateBufferObjectInMemoryRegionIsCalledWithoutMemoryInfoThenNullBufferObjectIsReturned) {
    mock->memoryInfo.reset(nullptr);

    auto gpuAddress = 0x1234u;
    auto size = MemoryConstants::pageSize;

    auto bo = std::unique_ptr<BufferObject>(memoryManager->createBufferObjectInMemoryRegion(rootDeviceIndex, nullptr, AllocationType::buffer, gpuAddress, size, MemoryBanks::mainBank, 1, -1, false, false));
    EXPECT_EQ(nullptr, bo);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenCreateBufferObjectInMemoryRegionIsCalledWithZeroSizeThenNullBufferObjectIsReturned) {
    auto gpuAddress = 0x1234u;
    auto size = 0u;

    auto bo = std::unique_ptr<BufferObject>(memoryManager->createBufferObjectInMemoryRegion(rootDeviceIndex, nullptr, AllocationType::buffer, gpuAddress, size, MemoryBanks::mainBank, 1, -1, false, false));
    EXPECT_EQ(nullptr, bo);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenUseKmdMigrationForBuffersWhenGraphicsAllocationInDevicePoolIsAllocatedForBufferWithSeveralMemoryBanksThenCreateGemObjectWithMultipleRegions) {
    debugManager.flags.UseKmdMigrationForBuffers.set(1);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_EQ(allocData.storageInfo.memoryBanks, static_cast<MockedMemoryInfo *>(mock->getMemoryInfo())->banks);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenUseKmdMigrationForBuffersWhenGraphicsAllocationInPhysicalLocalMemoryIsAllocatedForBufferWithSeveralMemoryBanksThenCreateGemObjectWithMultipleRegions) {
    debugManager.flags.UseKmdMigrationForBuffers.set(1);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;

    auto allocation = memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(0u, allocation->getGpuAddress());

    EXPECT_EQ(allocData.storageInfo.memoryBanks, static_cast<MockedMemoryInfo *>(mock->getMemoryInfo())->banks);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenUAllocationInPhysicalLocalMemoryIsAllocatedWithNoCacheRegionThenFailureReturned) {
    debugManager.flags.UseKmdMigrationForBuffers.set(1);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;
    allocData.cacheRegion = 0xFFFF;

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(0));
    drm.cacheInfo.reset(nullptr);

    auto allocation = memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenMemoryAllocationWithNoSetPairAndOneHandleAndCommandBufferTypeThenNoPairHandleIsPassed) {
    VariableBackup<bool> backupSetPairCallParent{&mock->getSetPairAvailableCall.callParent, false};
    VariableBackup<bool> backupSetPairReturnValue{&mock->getSetPairAvailableCall.returnValue, true};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::commandBuffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b01;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_EQ(-1, static_cast<MockedMemoryInfo *>(mock->getMemoryInfo())->pairHandlePassed);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenPhysicalLocalMemoryAllocationThenSuccessAndNoVirtualMemoryAssigned) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::commandBuffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b01;

    auto allocation = memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_EQ(0u, allocation->getGpuAddress());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenMemoryAllocationWithSetPairAndOneHandleThenThenNoPairHandleIsPassed) {
    VariableBackup<bool> backupSetPairCallParent{&mock->getSetPairAvailableCall.callParent, false};
    VariableBackup<bool> backupSetPairReturnValue{&mock->getSetPairAvailableCall.returnValue, true};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b01;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_EQ(-1, static_cast<MockedMemoryInfo *>(mock->getMemoryInfo())->pairHandlePassed);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenMemoryAllocationWithSetPairAndTwoHandlesThenPairHandleIsPassed) {
    VariableBackup<bool> backupSetPairCallParent{&mock->getSetPairAvailableCall.callParent, false};
    VariableBackup<bool> backupSetPairReturnValue{&mock->getSetPairAvailableCall.returnValue, true};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_NE(-1, static_cast<MockedMemoryInfo *>(mock->getMemoryInfo())->pairHandlePassed);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest,
       givenMemoryAllocationWithLessThanChunkingSizeAllowedThenChunkingIsNotUsed) {
    VariableBackup<bool> backupChunkingCallParent{&mock->getChunkingAvailableCall.callParent, false};
    VariableBackup<bool> backupChunkingReturnValue{&mock->getChunkingAvailableCall.returnValue, true};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::chunkThreshold / 4;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_EQ(false, static_cast<MockedMemoryInfo *>(mock->getMemoryInfo())->isChunkedUsed);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest,
       givenMemoryAllocationWithMoreThanChunkingSizeAllowedThenChunkingIsUsed) {
    VariableBackup<bool> backupChunkingCallParent{&mock->getChunkingAvailableCall.callParent, false};
    VariableBackup<bool> backupChunkingReturnValue{&mock->getChunkingAvailableCall.returnValue, true};
    VariableBackup<bool> backupChunkingModeCallParent{&mock->getChunkingModeCall.callParent, false};
    VariableBackup<uint32_t> backupChunkingModeReturnValue{&mock->getChunkingModeCall.returnValue, 0x3};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize2M;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;
    allocData.storageInfo.subDeviceBitfield = 0b11;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_EQ(true, static_cast<MockedMemoryInfo *>(mock->getMemoryInfo())->isChunkedUsed);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest,
       givenDeviceMemoryAllocationWithChunkingModeSetToSharedThenNoChunkingIsNotUsed) {
    VariableBackup<bool> backupChunkingCallParent{&mock->getChunkingAvailableCall.callParent, false};
    VariableBackup<bool> backupChunkingReturnValue{&mock->getChunkingAvailableCall.returnValue, true};
    VariableBackup<bool> backupChunkingModeCallParent{&mock->getChunkingModeCall.callParent, false};
    VariableBackup<uint32_t> backupChunkingModeReturnValue{&mock->getChunkingModeCall.returnValue, 0x1};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize2M;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_FALSE(allocation->storageInfo.isChunked);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest,
       givenDeviceMemoryAllocationWithChunkingModeSetToDeviceAndOnlyOneTileThenChunkingIsNotUsed) {
    VariableBackup<bool> backupChunkingCallParent{&mock->getChunkingAvailableCall.callParent, false};
    VariableBackup<bool> backupChunkingReturnValue{&mock->getChunkingAvailableCall.returnValue, true};
    VariableBackup<bool> backupChunkingModeCallParent{&mock->getChunkingModeCall.callParent, false};
    VariableBackup<uint32_t> backupChunkingModeReturnValue{&mock->getChunkingModeCall.returnValue, 0x2};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize2M;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;
    allocData.storageInfo.subDeviceBitfield = 0b01;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_FALSE(allocation->storageInfo.isChunked);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest,
       givenDeviceMemoryAllocationWithChunkingModeSetToDeviceThenChunkingIsUsed) {
    VariableBackup<bool> backupChunkingCallParent{&mock->getChunkingAvailableCall.callParent, false};
    VariableBackup<bool> backupChunkingReturnValue{&mock->getChunkingAvailableCall.returnValue, true};
    VariableBackup<bool> backupChunkingModeCallParent{&mock->getChunkingModeCall.callParent, false};
    VariableBackup<uint32_t> backupChunkingModeReturnValue{&mock->getChunkingModeCall.returnValue, 0x2};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize2M;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;
    allocData.storageInfo.subDeviceBitfield = 0b11;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_TRUE(allocation->storageInfo.isChunked);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest,
       givenDeviceMemoryAllocationWithChunkingModeSetToDeviceAndChunkingSizeNotAlignedThenChunkingIsNotUsed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.NumberOfBOChunks.set(64);

    VariableBackup<bool> backupChunkingCallParent{&mock->getChunkingAvailableCall.callParent, false};
    VariableBackup<bool> backupChunkingReturnValue{&mock->getChunkingAvailableCall.returnValue, true};
    VariableBackup<bool> backupChunkingModeCallParent{&mock->getChunkingModeCall.callParent, false};
    VariableBackup<uint32_t> backupChunkingModeReturnValue{&mock->getChunkingModeCall.returnValue, 0x2};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize2M;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_FALSE(allocation->storageInfo.isChunked);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest,
       givenDeviceMemoryAllocationWithChunkingModeSetToDeviceAndSizeLessThanMinimalThenChunkingIsNotUsed) {
    VariableBackup<bool> backupChunkingCallParent{&mock->getChunkingAvailableCall.callParent, false};
    VariableBackup<bool> backupChunkingReturnValue{&mock->getChunkingAvailableCall.returnValue, true};
    VariableBackup<bool> backupChunkingModeCallParent{&mock->getChunkingModeCall.callParent, false};
    VariableBackup<uint32_t> backupChunkingModeReturnValue{&mock->getChunkingModeCall.returnValue, 0x2};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_FALSE(allocation->storageInfo.isChunked);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest,
       givenMemoryAllocationWithMoreThanChunkingSizeAllowedWithDebuggingEnabledThenChunkingIsNotUsed) {
    VariableBackup<bool> backupChunkingCallParent{&mock->getChunkingAvailableCall.callParent, false};
    VariableBackup<bool> backupChunkingReturnValue{&mock->getChunkingAvailableCall.returnValue, true};
    VariableBackup<bool> backupChunkingModeCallParent{&mock->getChunkingModeCall.callParent, false};
    VariableBackup<uint32_t> backupChunkingModeReturnValue{&mock->getChunkingModeCall.returnValue, 0x3};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize2M;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;

    executionEnvironment->setDebuggingMode(DebuggingMode::offline);

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_EQ(false, static_cast<MockedMemoryInfo *>(mock->getMemoryInfo())->isChunkedUsed);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest,
       givenMemoryAllocationWithMoreThanChunkingSizeAllowedAndFailGemCreateExtThenNullptrIsReturned) {
    VariableBackup<bool> backupChunkingCallParent{&mock->getChunkingAvailableCall.callParent, false};
    VariableBackup<bool> backupChunkingReturnValue{&mock->getChunkingAvailableCall.returnValue, true};
    VariableBackup<bool> backupChunkingModeCallParent{&mock->getChunkingModeCall.callParent, false};
    VariableBackup<uint32_t> backupChunkingModeReturnValue{&mock->getChunkingModeCall.returnValue, 0x3};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize2M;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;
    allocData.storageInfo.subDeviceBitfield = 0b11;
    static_cast<MockedMemoryInfo *>(mock->getMemoryInfo())->failOnCreateGemExtWithMultipleRegions = true;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenMemoryAllocationWithNoSetPairAndTwoHandlesThenPairHandleIsPassed) {
    VariableBackup<bool> backupSetPairCallParent{&mock->getSetPairAvailableCall.callParent, false};
    VariableBackup<bool> backupSetPairReturnValue{&mock->getSetPairAvailableCall.returnValue, false};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_EQ(-1, static_cast<MockedMemoryInfo *>(mock->getMemoryInfo())->pairHandlePassed);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenUseSystemMemoryFlagWhenGraphicsAllocationInDevicePoolIsAllocatedThenNullptrIsReturned) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.useSystemMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenSvmGpuAllocationWhenHostPtrProvidedThenUseHostPtrAsGpuVa) {
    size_t size = 2 * MemoryConstants::megaByte;
    AllocationProperties properties{rootDeviceIndex, false, size, AllocationType::svmGpu, false, mockDeviceBitfield};
    properties.alignment = size;
    void *svmPtr = reinterpret_cast<void *>(2 * size);

    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(properties, svmPtr));
    ASSERT_NE(nullptr, allocation);

    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(nullptr, allocation->getDriverAllocatedCpuPtr());

    EXPECT_EQ(svmPtr, reinterpret_cast<void *>(allocation->getGpuAddress()));

    EXPECT_EQ(0u, allocation->getReservedAddressSize());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenAllowed32BitAndForce32BitWhenGraphicsAllocationInDevicePoolIsAllocatedThenNullptrIsReturned) {
    memoryManager->setForce32BitAllocations(true);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allow32Bit = true;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenAllowed32BitWhen32BitIsNotForcedThenGraphicsAllocationInDevicePoolReturnsLocalMemoryAllocation) {
    memoryManager->setForce32BitAllocations(false);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allow32Bit = true;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenAllocationWithKernelIsaWhenAllocatingInDevicePoolOnAllMemoryBanksThenCreateFourBufferObjectsWithSameGpuVirtualAddressAndSize) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 3 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;

    AllocationType isaTypes[] = {AllocationType::kernelIsa, AllocationType::kernelIsaInternal};
    for (uint32_t i = 0; i < 2; i++) {
        allocData.type = isaTypes[i];
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
        EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
        EXPECT_NE(0u, allocation->getGpuAddress());
        EXPECT_EQ(EngineLimits::maxHandleCount, allocation->getNumGmms());

        auto drmAllocation = static_cast<DrmAllocation *>(allocation);
        auto &bos = drmAllocation->getBOs();
        auto boAddress = drmAllocation->getGpuAddress();
        for (auto handleId = 0u; handleId < EngineLimits::maxHandleCount; handleId++) {
            auto bo = bos[handleId];
            ASSERT_NE(nullptr, bo);
            auto boSize = allocation->getGmm(handleId)->gmmResourceInfo->getSizeAllocation();
            EXPECT_EQ(boAddress, bo->peekAddress());
            EXPECT_EQ(boSize, bo->peekSize());
            EXPECT_EQ(boSize, 3 * MemoryConstants::pageSize64k);
        }

        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenAllocationWithLargeBufferWhenAllocatingInLocalPhysicalMemoryOnAllMemoryBanksThenCreateFourBufferObjectsWithDifferentGpuVirtualAddressesAndPartialSizes) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    uint64_t gpuAddress = 0x1234;
    allocData.allFlags = 0;
    allocData.size = 18 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));
    allocData.storageInfo.multiStorage = true;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_EQ(0u, allocation->getGpuAddress());
    EXPECT_EQ(EngineLimits::maxHandleCount, allocation->getNumGmms());
    memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(allocation, gpuAddress, allocData.size);
    EXPECT_EQ(gpuAddress, allocation->getGpuAddress());

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto &bos = drmAllocation->getBOs();
    auto boAddress = drmAllocation->getGpuAddress();
    for (auto handleId = 0u; handleId < EngineLimits::maxHandleCount; handleId++) {
        auto bo = bos[handleId];
        ASSERT_NE(nullptr, bo);
        auto boSize = allocation->getGmm(handleId)->gmmResourceInfo->getSizeAllocation();
        EXPECT_EQ(boAddress, bo->peekAddress());
        EXPECT_EQ(boSize, bo->peekSize());
        EXPECT_EQ(boSize, handleId == 0 || handleId == 1 ? 5 * MemoryConstants::pageSize64k : 4 * MemoryConstants::pageSize64k);
        boAddress += boSize;
    }
    auto osContext = device->getDefaultEngine().osContext;
    memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(allocation, gpuAddress, allocData.size, osContext, 0u);
    EXPECT_EQ(0u, allocation->getGpuAddress());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenAllocationWithKernelIsaWhenAllocationInLocalPhysicalMemoryAndDeviceBitfieldWithHolesThenCorrectAllocationCreated) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::kernelIsa;
    allocData.storageInfo.memoryBanks = 0b1011;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    uint64_t gpuAddress = 0x1234;

    auto kernelIsaAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status));

    EXPECT_NE(nullptr, kernelIsaAllocation);

    memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(kernelIsaAllocation, gpuAddress, allocData.size);

    auto gpuAddressReserved = kernelIsaAllocation->getGpuAddress();
    auto &bos = kernelIsaAllocation->getBOs();

    EXPECT_NE(nullptr, bos[0]);
    EXPECT_EQ(gpuAddressReserved, bos[0]->peekAddress());
    EXPECT_NE(nullptr, bos[1]);
    EXPECT_EQ(gpuAddressReserved, bos[1]->peekAddress());

    EXPECT_EQ(nullptr, bos[2]);

    EXPECT_NE(nullptr, bos[3]);
    EXPECT_EQ(gpuAddressReserved, bos[3]->peekAddress());

    auto &storageInfo = kernelIsaAllocation->storageInfo;
    EXPECT_EQ(0b1011u, storageInfo.memoryBanks.to_ulong());

    auto osContext = device->getDefaultEngine().osContext;
    memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(kernelIsaAllocation, gpuAddress, allocData.size, osContext, 0u);

    memoryManager->freeGraphicsMemory(kernelIsaAllocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenAllocationWithLargeBufferThenAfterUnMapBindInfoisReset) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    uint64_t gpuAddress = 0x1234;
    allocData.allFlags = 0;
    allocData.size = 18 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));
    allocData.storageInfo.multiStorage = true;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_EQ(0u, allocation->getGpuAddress());
    EXPECT_EQ(EngineLimits::maxHandleCount, allocation->getNumGmms());
    memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(allocation, gpuAddress, allocData.size);
    EXPECT_EQ(gpuAddress, allocation->getGpuAddress());

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto &bos = drmAllocation->getBOs();
    auto boAddress = drmAllocation->getGpuAddress();
    for (auto handleId = 0u; handleId < EngineLimits::maxHandleCount; handleId++) {
        auto bo = bos[handleId];
        ASSERT_NE(nullptr, bo);
        auto boSize = allocation->getGmm(handleId)->gmmResourceInfo->getSizeAllocation();
        EXPECT_EQ(boAddress, bo->peekAddress());
        EXPECT_EQ(boSize, bo->peekSize());
        EXPECT_EQ(boSize, handleId == 0 || handleId == 1 ? 5 * MemoryConstants::pageSize64k : 4 * MemoryConstants::pageSize64k);
        boAddress += boSize;
    }
    auto osContext = device->getDefaultEngine().osContext;
    memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(allocation, gpuAddress, allocData.size, osContext, 0u);
    for (auto handleId = 0u; handleId < EngineLimits::maxHandleCount; handleId++) {
        auto bo = bos[handleId];
        auto contextId = bo->getOsContextId(osContext);
        ASSERT_NE(nullptr, bo);
        EXPECT_FALSE(bo->getBindInfo()[contextId][handleId]);
    }
    EXPECT_EQ(0u, allocation->getGpuAddress());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenAllocationWithLargeBufferWhenAllocatingInDevicePoolOnAllMemoryBanksThenCreateFourBufferObjectsWithDifferentGpuVirtualAddressesAndPartialSizes) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 18 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));
    allocData.storageInfo.multiStorage = true;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_NE(0u, allocation->getGpuAddress());
    EXPECT_EQ(EngineLimits::maxHandleCount, allocation->getNumGmms());

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto &bos = drmAllocation->getBOs();
    auto boAddress = drmAllocation->getGpuAddress();
    for (auto handleId = 0u; handleId < EngineLimits::maxHandleCount; handleId++) {
        auto bo = bos[handleId];
        ASSERT_NE(nullptr, bo);
        auto boSize = allocation->getGmm(handleId)->gmmResourceInfo->getSizeAllocation();
        EXPECT_EQ(boAddress, bo->peekAddress());
        EXPECT_EQ(boSize, bo->peekSize());
        EXPECT_EQ(boSize, handleId == 0 || handleId == 1 ? 5 * MemoryConstants::pageSize64k : 4 * MemoryConstants::pageSize64k);
        boAddress += boSize;
    }
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenMultipleHandlesUpdateSubAllocationInformation) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 18 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));
    allocData.storageInfo.multiStorage = true;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_NE(0u, allocation->getGpuAddress());
    EXPECT_EQ(EngineLimits::maxHandleCount, allocation->getNumGmms());

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto &bos = drmAllocation->getBOs();
    auto boAddress = drmAllocation->getGpuAddress();
    for (auto handleId = 0u; handleId < EngineLimits::maxHandleCount; handleId++) {
        auto bo = bos[handleId];
        ASSERT_NE(nullptr, bo);
        auto boSize = allocation->getGmm(handleId)->gmmResourceInfo->getSizeAllocation();
        EXPECT_EQ(boAddress, allocation->getHandleAddressBase(handleId));
        EXPECT_EQ(boSize, allocation->getHandleSize(handleId));
        boAddress += boSize;
    }
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenAllocationWithKernelIsaWhenAllocationInDevicePoolAndDeviceBitfieldWithHolesThenCorrectAllocationCreated) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::kernelIsa;
    allocData.storageInfo.memoryBanks = 0b1011;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto kernelIsaAllocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status));

    EXPECT_NE(nullptr, kernelIsaAllocation);

    auto gpuAddress = kernelIsaAllocation->getGpuAddress();
    auto &bos = kernelIsaAllocation->getBOs();

    EXPECT_NE(nullptr, bos[0]);
    EXPECT_EQ(gpuAddress, bos[0]->peekAddress());
    EXPECT_NE(nullptr, bos[1]);
    EXPECT_EQ(gpuAddress, bos[1]->peekAddress());

    EXPECT_EQ(nullptr, bos[2]);

    EXPECT_NE(nullptr, bos[3]);
    EXPECT_EQ(gpuAddress, bos[3]->peekAddress());

    auto &storageInfo = kernelIsaAllocation->storageInfo;
    EXPECT_EQ(0b1011u, storageInfo.memoryBanks.to_ulong());

    memoryManager->freeGraphicsMemory(kernelIsaAllocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDeviceBitfieldWithHolesWhenMappingPhysicalHostMemoryThenMappingIsSuccessful) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.isUSMHostAllocation = true;
    allocData.type = AllocationType::bufferHostMemory;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    uint64_t gpuAddress = 0x1234;

    auto physicalAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalHostMemory(allocData, status));
    EXPECT_NE(nullptr, physicalAllocation);

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    rootDeviceIndices.pushUnique(1);
    rootDeviceIndices.pushUnique(2);
    MultiGraphicsAllocation multiGraphicsAllocation{static_cast<uint32_t>(rootDeviceIndices.size())};
    EXPECT_TRUE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, allocData.size));
    for (uint32_t i = 0; i < static_cast<uint32_t>(rootDeviceIndices.size()); i++) {
        EXPECT_NE(nullptr, multiGraphicsAllocation.getGraphicsAllocation(i));
    }

    memoryManager->unMapPhysicalHostMemoryFromVirtualMemory(multiGraphicsAllocation, physicalAllocation, gpuAddress, allocData.size);
    memoryManager->freeGraphicsMemory(physicalAllocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDeviceBitfieldWithHolesAndPrimeHandleToFdFailsWhenMappingPhysicalHostMemoryThenMappingFails) {
    mock->failOnPrimeHandleToFd = true;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.isUSMHostAllocation = true;
    allocData.type = AllocationType::bufferHostMemory;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    uint64_t gpuAddress = 0x1234;

    auto physicalAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalHostMemory(allocData, status));
    EXPECT_NE(nullptr, physicalAllocation);

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    rootDeviceIndices.pushUnique(1);
    rootDeviceIndices.pushUnique(2);
    MultiGraphicsAllocation multiGraphicsAllocation{static_cast<uint32_t>(rootDeviceIndices.size())};
    EXPECT_FALSE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, allocData.size));

    for (uint32_t i = 0; i < static_cast<uint32_t>(rootDeviceIndices.size()); i++) {
        EXPECT_EQ(nullptr, multiGraphicsAllocation.getGraphicsAllocation(i));
    }

    memoryManager->freeGraphicsMemory(physicalAllocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDeviceBitfieldWithHolesAndPrimeFdToHandleFailsWhenMappingPhysicalHostMemoryThenMappingFails) {
    mock->failOnPrimeFdToHandle = true;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.isUSMHostAllocation = true;
    allocData.type = AllocationType::bufferHostMemory;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    uint64_t gpuAddress = 0x1234;

    auto physicalAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalHostMemory(allocData, status));
    EXPECT_NE(nullptr, physicalAllocation);

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    rootDeviceIndices.pushUnique(1);
    rootDeviceIndices.pushUnique(2);
    MultiGraphicsAllocation multiGraphicsAllocation{static_cast<uint32_t>(rootDeviceIndices.size())};
    EXPECT_FALSE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, allocData.size));

    for (uint32_t i = 0; i < static_cast<uint32_t>(rootDeviceIndices.size()); i++) {
        EXPECT_EQ(nullptr, multiGraphicsAllocation.getGraphicsAllocation(i));
    }

    memoryManager->freeGraphicsMemory(physicalAllocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDeviceBitfieldWithHolesAndSecondPrimeFdToHandleFailsWhenMappingPhysicalHostMemoryThenMappingFails) {
    mock->failOnSecondPrimeFdToHandle = true;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.isUSMHostAllocation = true;
    allocData.type = AllocationType::bufferHostMemory;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    uint64_t gpuAddress = 0x1234;

    auto physicalAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalHostMemory(allocData, status));
    EXPECT_NE(nullptr, physicalAllocation);

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    rootDeviceIndices.pushUnique(1);
    rootDeviceIndices.pushUnique(2);
    MultiGraphicsAllocation multiGraphicsAllocation{static_cast<uint32_t>(rootDeviceIndices.size())};
    EXPECT_FALSE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, allocData.size));

    for (uint32_t i = 0; i < static_cast<uint32_t>(rootDeviceIndices.size()); i++) {
        EXPECT_EQ(nullptr, multiGraphicsAllocation.getGraphicsAllocation(i));
    }

    memoryManager->freeGraphicsMemory(physicalAllocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenSingleRootDeviceWhenMappingPhysicalHostMemoryThenMappingIsSuccessful) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.isUSMHostAllocation = true;
    allocData.type = AllocationType::bufferHostMemory;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    uint64_t gpuAddress = 0x1234;

    auto physicalAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalHostMemory(allocData, status));
    EXPECT_NE(nullptr, physicalAllocation);

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(rootDeviceIndex);
    MultiGraphicsAllocation multiGraphicsAllocation{1u};
    EXPECT_TRUE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, allocData.size));
    EXPECT_NE(nullptr, multiGraphicsAllocation.getDefaultGraphicsAllocation());
    EXPECT_NE(nullptr, multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));

    memoryManager->unMapPhysicalHostMemoryFromVirtualMemory(multiGraphicsAllocation, physicalAllocation, gpuAddress, allocData.size);
    memoryManager->freeGraphicsMemory(physicalAllocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenSingleRootDeviceWhenMappingLockedPhysicalHostMemoryThenMappingIsSuccessful) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.isUSMHostAllocation = true;
    allocData.type = AllocationType::bufferHostMemory;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    uint64_t gpuAddress = 0x1234;

    auto physicalAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalHostMemory(allocData, status));
    EXPECT_NE(nullptr, physicalAllocation);
    physicalAllocation->lock(addrToPtr(gpuAddress));

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(rootDeviceIndex);
    MultiGraphicsAllocation multiGraphicsAllocation{1u};
    EXPECT_TRUE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, allocData.size));
    EXPECT_NE(nullptr, multiGraphicsAllocation.getDefaultGraphicsAllocation());
    EXPECT_NE(nullptr, multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));
    multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex)->lock(addrToPtr(gpuAddress));

    memoryManager->unMapPhysicalHostMemoryFromVirtualMemory(multiGraphicsAllocation, physicalAllocation, gpuAddress, allocData.size);
    memoryManager->freeGraphicsMemory(physicalAllocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenSingleRootDeviceAndPrimeHandleToFdFailsWhenMappingPhysicalHostMemoryThenMappingIsSuccessful) {
    mock->failOnPrimeHandleToFd = true;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.isUSMHostAllocation = true;
    allocData.type = AllocationType::bufferHostMemory;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    uint64_t gpuAddress = 0x1234;

    auto physicalAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalHostMemory(allocData, status));
    EXPECT_NE(nullptr, physicalAllocation);

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(rootDeviceIndex);
    MultiGraphicsAllocation multiGraphicsAllocation{1u};
    EXPECT_TRUE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, allocData.size));
    EXPECT_NE(nullptr, multiGraphicsAllocation.getDefaultGraphicsAllocation());
    EXPECT_NE(nullptr, multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));

    memoryManager->unMapPhysicalHostMemoryFromVirtualMemory(multiGraphicsAllocation, physicalAllocation, gpuAddress, allocData.size);
    memoryManager->freeGraphicsMemory(physicalAllocation);
}

struct DrmMemoryManagerLocalMemoryAlignmentTest : DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest {
    std::unique_ptr<TestedDrmMemoryManager> createMemoryManager() {
        return std::make_unique<TestedDrmMemoryManager>(true, false, false, *executionEnvironment);
    }

    bool isAllocationWithinHeap(MemoryManager &memoryManager, const GraphicsAllocation &allocation, HeapIndex heap) {
        const auto allocationStart = allocation.getGpuAddress();
        const auto allocationEnd = allocationStart + allocation.getUnderlyingBufferSize();
        const auto gmmHelper = device->getGmmHelper();
        const auto heapStart = gmmHelper->canonize(memoryManager.getGfxPartition(rootDeviceIndex)->getHeapBase(heap));
        const auto heapEnd = gmmHelper->canonize(memoryManager.getGfxPartition(rootDeviceIndex)->getHeapLimit(heap));
        return heapStart <= allocationStart && allocationEnd <= heapEnd;
    }

    const uint32_t rootDeviceIndex = 1u;
    DebugManagerStateRestore restore{};
};

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, given2MbAlignmentAllowedWhenAllocatingAllocationLessThen2MbThenUse64kbHeap) {
    AllocationData allocationData;
    allocationData.allFlags = 0;
    allocationData.size = MemoryConstants::pageSize;
    allocationData.flags.allocateMemory = true;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.type = AllocationType::buffer;
    allocationData.flags.resource48Bit = true;
    MemoryManager::AllocationStatus allocationStatus;

    {
        debugManager.flags.AlignLocalMemoryVaTo2MB.set(-1);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard64KB));
        memoryManager->freeGraphicsMemory(allocation);
    }
    {
        debugManager.flags.AlignLocalMemoryVaTo2MB.set(0);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard64KB));
        memoryManager->freeGraphicsMemory(allocation);
    }
    {
        debugManager.flags.AlignLocalMemoryVaTo2MB.set(1);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard64KB));
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, given2MbAlignmentAllowedWhenAllocatingAllocationBiggerThan2MbThenUse2MbHeap) {
    AllocationData allocationData;
    allocationData.allFlags = 0;
    allocationData.size = 2 * MemoryConstants::megaByte;
    allocationData.flags.allocateMemory = true;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.type = AllocationType::buffer;
    allocationData.flags.resource48Bit = true;
    MemoryManager::AllocationStatus allocationStatus;

    {
        debugManager.flags.AlignLocalMemoryVaTo2MB.set(-1);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard2MB));
        memoryManager->freeGraphicsMemory(allocation);
    }
    {
        debugManager.flags.AlignLocalMemoryVaTo2MB.set(0);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard64KB));
        memoryManager->freeGraphicsMemory(allocation);
    }
    {
        debugManager.flags.AlignLocalMemoryVaTo2MB.set(1);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard2MB));
        EXPECT_TRUE(isAligned(allocation->getGpuAddress(), MemoryConstants::pageSize2M));
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, givenExtendedHeapPreferredAnd2MbAlignmentAllowedWhenAllocatingAllocationBiggerThenUseExtendedHeap) {
    if (memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapExtended) == 0) {
        GTEST_SKIP();
    }

    AllocationData allocationData;
    allocationData.allFlags = 0;
    allocationData.size = 2 * MemoryConstants::megaByte;
    allocationData.flags.allocateMemory = true;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.type = AllocationType::buffer;
    allocationData.flags.resource48Bit = false;
    MemoryManager::AllocationStatus allocationStatus;

    {
        debugManager.flags.AlignLocalMemoryVaTo2MB.set(-1);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAligned(allocation->getGpuAddress(), MemoryConstants::pageSize2M));
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapExtended));
        memoryManager->freeGraphicsMemory(allocation);
    }
    {
        debugManager.flags.AlignLocalMemoryVaTo2MB.set(0);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapExtended));
        memoryManager->freeGraphicsMemory(allocation);
    }
    {
        debugManager.flags.AlignLocalMemoryVaTo2MB.set(1);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapExtended));
        EXPECT_TRUE(isAligned(allocation->getGpuAddress(), MemoryConstants::pageSize2M));
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, givenCustomAlignmentWhenAllocatingAllocationBiggerThanTheAlignmentThenAlignProperly) {
    AllocationData allocationData;
    allocationData.allFlags = 0;
    allocationData.flags.allocateMemory = true;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.type = AllocationType::buffer;
    allocationData.flags.resource48Bit = true;
    MemoryManager::AllocationStatus allocationStatus;

    {
        // size==2MB, use 2MB heap
        debugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(2 * MemoryConstants::megaByte);
        allocationData.size = 2 * MemoryConstants::megaByte;
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard2MB));
        EXPECT_TRUE(isAligned(allocation->getGpuAddress(), 2 * MemoryConstants::megaByte));
        memoryManager->freeGraphicsMemory(allocation);
    }

    {
        // size > 2MB, use 2MB heap
        debugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(16 * MemoryConstants::megaByte);
        allocationData.size = 16 * MemoryConstants::megaByte;
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard2MB));
        EXPECT_TRUE(isAligned(allocation->getGpuAddress(), 16 * MemoryConstants::megaByte));
        memoryManager->freeGraphicsMemory(allocation);
    }

    {
        // size < 2MB, use 64KB heap
        debugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(8 * MemoryConstants::pageSize64k);
        allocationData.size = 8 * MemoryConstants::pageSize64k;
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard64KB));
        EXPECT_TRUE(isAligned(allocation->getGpuAddress(), 8 * MemoryConstants::pageSize64k));
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, givenCustomAlignmentWhenAllocatingAllocationLessThanTheAlignmentThenIgnoreCustomAlignment) {
    AllocationData allocationData;
    allocationData.allFlags = 0;
    allocationData.size = 3 * MemoryConstants::megaByte;
    allocationData.flags.allocateMemory = true;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.type = AllocationType::buffer;
    allocationData.flags.resource48Bit = true;
    MemoryManager::AllocationStatus allocationStatus;

    {
        // Too small allocation, fallback to 2MB heap
        debugManager.flags.AlignLocalMemoryVaTo2MB.set(0);
        debugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(32 * MemoryConstants::megaByte);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard2MB));
        memoryManager->freeGraphicsMemory(allocation);
    }

    {
        // Too small allocation, fallback to 2MB heap
        debugManager.flags.AlignLocalMemoryVaTo2MB.set(1);
        debugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(32 * MemoryConstants::megaByte);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard2MB));
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, givenForced2MBSizeAlignmentWhenAllocatingAllocationThenUseProperAlignment) {
    debugManager.flags.ExperimentalAlignLocalMemorySizeTo2MB.set(true);

    AllocationData allocationData;
    allocationData.allFlags = 0;
    allocationData.flags.allocateMemory = true;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.type = AllocationType::buffer;
    allocationData.flags.resource48Bit = true;
    MemoryManager::AllocationStatus allocationStatus;

    {
        allocationData.size = 1;
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard2MB));
        EXPECT_EQ(2 * MemoryConstants::megaByte, allocation->getUnderlyingBufferSize());
        EXPECT_EQ(2 * MemoryConstants::megaByte, allocation->getReservedAddressSize());
        memoryManager->freeGraphicsMemory(allocation);
    }

    {
        allocationData.size = 2 * MemoryConstants::megaByte + 1;
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard2MB));
        EXPECT_EQ(4 * MemoryConstants::megaByte, allocation->getUnderlyingBufferSize());
        EXPECT_EQ(4 * MemoryConstants::megaByte, allocation->getReservedAddressSize());
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, givenEnabled2MBSizeAlignmentWhenAllocatingAllocationThenUseProperAlignment) {
    auto mockProductHelper = new MockProductHelper;
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper.reset(mockProductHelper);
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = true;

    ASSERT_TRUE(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper->is2MBLocalMemAlignmentEnabled());

    AllocationData allocationData;
    allocationData.allFlags = 0;
    allocationData.flags.allocateMemory = true;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.type = AllocationType::buffer;
    allocationData.flags.resource48Bit = true;
    MemoryManager::AllocationStatus allocationStatus;

    allocationData.size = 2 * MemoryConstants::megaByte + 1;
    auto memoryManager = createMemoryManager();
    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
    EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard2MB));
    EXPECT_EQ(4 * MemoryConstants::megaByte, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(4 * MemoryConstants::megaByte, allocation->getReservedAddressSize());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, Given2MBLocalMemAlignmentEnabledWhenAllocating2MBPageTypeInDevicePoolThenAllocationIs2MBAligned) {
    auto mockProductHelper = new MockProductHelper;
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper.reset(mockProductHelper);
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = true;

    ASSERT_TRUE(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper->is2MBLocalMemAlignmentEnabled());

    for (uint32_t i = 0; i < static_cast<uint32_t>(AllocationType::count); i++) {
        auto allocType = static_cast<AllocationType>(i);

        if (!GraphicsAllocation::is2MBPageAllocationType(allocType)) {
            continue;
        }

        const auto requestedSize = MemoryConstants::pageSize;
        const auto expectedAlignedSize = alignUp(requestedSize, MemoryConstants::pageSize2M);

        AllocationData allocationData;
        allocationData.allFlags = 0;
        allocationData.flags.allocateMemory = true;
        allocationData.rootDeviceIndex = rootDeviceIndex;
        allocationData.type = allocType;
        allocationData.flags.resource48Bit = true;
        allocationData.size = requestedSize;

        MemoryManager::AllocationStatus allocationStatus;
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard2MB));
        EXPECT_EQ(expectedAlignedSize, allocation->getUnderlyingBufferSize());
        EXPECT_EQ(expectedAlignedSize, allocation->getReservedAddressSize());
        EXPECT_TRUE(isAligned<MemoryConstants::pageSize2M>(allocation->getGpuAddress()));
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenNotSetUseSystemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedForBufferThenLocalMemoryAllocationIsReturnedFromStandard64KbHeap) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    auto sizeAligned = alignUp(allocData.size, MemoryConstants::pageSize64k);

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());

    auto gmm = allocation->getDefaultGmm();
    EXPECT_NE(nullptr, gmm);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.NonLocalOnly);

    EXPECT_EQ(RESOURCE_BUFFER, gmm->resourceParams.Type);
    EXPECT_EQ(sizeAligned, gmm->resourceParams.BaseWidth64);
    EXPECT_NE(nullptr, gmm->gmmResourceInfo->peekHandle());
    EXPECT_NE(0u, gmm->gmmResourceInfo->getHAlign());

    auto gpuAddress = allocation->getGpuAddress();
    EXPECT_NE(0u, gpuAddress);

    auto heap = HeapIndex::heapStandard64KB;
    if (memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapExtended)) {
        heap = HeapIndex::heapExtended;
    }

    auto gmmHelper = device->getGmmHelper();
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(heap)), gpuAddress);
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(heap)), gpuAddress);
    EXPECT_EQ(0u, allocation->getGpuBaseAddress());
    EXPECT_EQ(sizeAligned, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(gpuAddress, reinterpret_cast<uint64_t>(allocation->getReservedAddressPtr()));
    EXPECT_EQ(sizeAligned, allocation->getReservedAddressSize());

    EXPECT_EQ(allocData.storageInfo.memoryBanks, allocation->storageInfo.memoryBanks);
    EXPECT_EQ(allocData.storageInfo.pageTablesVisibility, allocation->storageInfo.pageTablesVisibility);
    EXPECT_EQ(allocData.storageInfo.cloningOfPageTables, allocation->storageInfo.cloningOfPageTables);
    EXPECT_EQ(allocData.storageInfo.tileInstanced, allocation->storageInfo.tileInstanced);
    EXPECT_EQ(allocData.storageInfo.multiStorage, allocation->storageInfo.multiStorage);
    EXPECT_EQ(allocData.flags.flushL3, allocation->isFlushL3Required());

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto bo = drmAllocation->getBO();
    EXPECT_NE(nullptr, bo);
    EXPECT_EQ(gpuAddress, bo->peekAddress());
    EXPECT_EQ(sizeAligned, bo->peekSize());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenPatIndexProgrammingAllowedWhenCreatingAllocationThenSetValidPatIndex) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    ASSERT_NE(nullptr, drmAllocation->getBO());
    auto &productHelper = this->device->getProductHelper();
    auto isVmBindPatIndexProgrammingSupported = productHelper.isVmBindPatIndexProgrammingSupported();

    EXPECT_EQ(isVmBindPatIndexProgrammingSupported, mock->isVmBindPatIndexProgrammingSupported());

    if (isVmBindPatIndexProgrammingSupported) {
        auto expectedIndex = mock->getPatIndex(allocation->getDefaultGmm(), allocation->getAllocationType(), CacheRegion::defaultRegion, CachePolicy::writeBack, false, false);

        EXPECT_NE(CommonConstants::unsupportedPatIndex, drmAllocation->getBO()->peekPatIndex());
        EXPECT_EQ(expectedIndex, drmAllocation->getBO()->peekPatIndex());
    } else {
        EXPECT_EQ(CommonConstants::unsupportedPatIndex, drmAllocation->getBO()->peekPatIndex());
    }

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenCompressedAndCachableAllocationWhenQueryingPatIndexThenPassCorrectParams) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 1;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    ASSERT_NE(nullptr, drmAllocation->getBO());
    auto &productHelper = this->device->getProductHelper();
    auto isVmBindPatIndexProgrammingSupported = productHelper.isVmBindPatIndexProgrammingSupported();

    if (isVmBindPatIndexProgrammingSupported) {
        auto mockClientContext = static_cast<MockGmmClientContextBase *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getGmmClientContext());
        auto gmm = allocation->getDefaultGmm();

        {
            gmm->setCompressionEnabled(true);
            gmm->gmmResourceInfo->getResourceFlags()->Info.Cacheable = 1;

            mock->getPatIndex(allocation->getDefaultGmm(), allocation->getAllocationType(), CacheRegion::defaultRegion, CachePolicy::writeBack, false, false);

            EXPECT_TRUE(mockClientContext->passedCachableSettingForGetPatIndexQuery);
            EXPECT_TRUE(mockClientContext->passedCompressedSettingForGetPatIndexQuery);
        }

        {
            gmm->setCompressionEnabled(false);
            gmm->gmmResourceInfo->getResourceFlags()->Info.Cacheable = 0;

            mock->getPatIndex(allocation->getDefaultGmm(), allocation->getAllocationType(), CacheRegion::defaultRegion, CachePolicy::writeBack, false, false);

            EXPECT_FALSE(mockClientContext->passedCachableSettingForGetPatIndexQuery);
            EXPECT_FALSE(mockClientContext->passedCompressedSettingForGetPatIndexQuery);
        }
    }

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenNotSetUseSystemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedForImageThenLocalMemoryAllocationIsReturnedFromStandard64KbHeap) {
    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::image2D;
    imgDesc.imageWidth = 512;
    imgDesc.imageHeight = 512;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::image;
    allocData.flags.resource48Bit = true;
    allocData.imgInfo = &imgInfo;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_TRUE(allocData.imgInfo->useLocalMemory);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());

    auto gmm = allocation->getDefaultGmm();
    EXPECT_NE(nullptr, gmm);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.NonLocalOnly);

    auto gpuAddress = allocation->getGpuAddress();
    auto sizeAligned = alignUp(allocData.imgInfo->size, MemoryConstants::pageSize64k);
    EXPECT_NE(0u, gpuAddress);

    auto gmmHelper = device->getGmmHelper();
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapStandard64KB)), gpuAddress);
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapStandard64KB)), gpuAddress);
    EXPECT_EQ(0u, allocation->getGpuBaseAddress());
    EXPECT_EQ(sizeAligned, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(gpuAddress, reinterpret_cast<uint64_t>(allocation->getReservedAddressPtr()));
    EXPECT_EQ(sizeAligned, allocation->getReservedAddressSize());

    EXPECT_EQ(allocData.storageInfo.memoryBanks, allocation->storageInfo.memoryBanks);
    EXPECT_EQ(allocData.storageInfo.pageTablesVisibility, allocation->storageInfo.pageTablesVisibility);
    EXPECT_EQ(allocData.storageInfo.cloningOfPageTables, allocation->storageInfo.cloningOfPageTables);
    EXPECT_EQ(allocData.storageInfo.tileInstanced, allocation->storageInfo.tileInstanced);
    EXPECT_EQ(allocData.storageInfo.multiStorage, allocation->storageInfo.multiStorage);
    EXPECT_EQ(allocData.flags.flushL3, allocation->isFlushL3Required());

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto bo = drmAllocation->getBO();
    EXPECT_NE(nullptr, bo);
    EXPECT_EQ(gpuAddress, bo->peekAddress());
    EXPECT_EQ(sizeAligned, bo->peekSize());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenNotSetUseSystemMemoryWhenGraphicsAllocatioInDevicePoolIsAllocatednForKernelIsaThenLocalMemoryAllocationIsReturnedFromInternalHeap) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.rootDeviceIndex = rootDeviceIndex;
    auto sizeAligned = alignUp(allocData.size, MemoryConstants::pageSize64k);

    AllocationType isaTypes[] = {AllocationType::kernelIsa, AllocationType::kernelIsaInternal};
    for (uint32_t i = 0; i < 2; i++) {
        allocData.type = isaTypes[i];
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
        EXPECT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
        EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());

        auto gpuAddress = allocation->getGpuAddress();
        auto gmmHelper = device->getGmmHelper();
        EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapInternalDeviceMemory)), gpuAddress);
        EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapInternalDeviceMemory)), gpuAddress);
        EXPECT_EQ(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapInternalDeviceMemory)), allocation->getGpuBaseAddress());
        EXPECT_EQ(sizeAligned, allocation->getUnderlyingBufferSize());
        EXPECT_EQ(gpuAddress, reinterpret_cast<uint64_t>(allocation->getReservedAddressPtr()));
        EXPECT_EQ(sizeAligned, allocation->getReservedAddressSize());

        auto drmAllocation = static_cast<DrmAllocation *>(allocation);
        auto bo = drmAllocation->getBO();
        EXPECT_NE(nullptr, bo);
        EXPECT_EQ(gpuAddress, bo->peekAddress());
        EXPECT_EQ(sizeAligned, bo->peekSize());

        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenUnsupportedTypeWhenAllocatingInDevicePoolThenRetryInNonDevicePoolStatusAndNullptrIsReturned) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.rootDeviceIndex = rootDeviceIndex;

    AllocationType unsupportedTypes[] = {AllocationType::sharedResourceCopy};

    for (auto unsupportedType : unsupportedTypes) {
        allocData.type = unsupportedType;

        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
        ASSERT_EQ(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);

        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenAllocationsThatAreAlignedToPowerOf2InSizeAndAreGreaterThen8GBThenTheyAreAlignedToPreviousPowerOfTwoForGpuVirtualAddress) {
    if (!memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapExtended)) {
        GTEST_SKIP();
    }
    auto &productHelper = this->device->getProductHelper();

    auto size = 8 * MemoryConstants::gigaByte;

    auto status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.size = size;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(allocData.size, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(allocData.size, static_cast<DrmAllocation *>(allocation)->getBO()->peekSize());
    EXPECT_TRUE(allocation->getGpuAddress() % size == 0u);

    size = 8 * MemoryConstants::gigaByte + MemoryConstants::pageSize64k;
    size_t expectedSize{};

    if (productHelper.is2MBLocalMemAlignmentEnabled()) {
        expectedSize = alignUp(size, MemoryConstants::pageSize2M);
    } else {
        expectedSize = alignUp(size, MemoryConstants::pageSize64k);
    }

    allocData.size = size;
    auto allocation2 = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    ASSERT_NE(nullptr, allocation2);
    EXPECT_EQ(expectedSize, allocation2->getUnderlyingBufferSize());
    EXPECT_EQ(expectedSize, static_cast<DrmAllocation *>(allocation2)->getBO()->peekSize());
    EXPECT_TRUE(allocation2->getGpuAddress() % MemoryConstants::pageSize2M == 0u);

    memoryManager->freeGraphicsMemory(allocation);
    memoryManager->freeGraphicsMemory(allocation2);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDebugVariableToDisableAddressAlignmentWhenAllocationsAreMadeTheyAreAlignedTo2MB) {
    if (!memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapExtended)) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restorer;
    debugManager.flags.UseHighAlignmentForHeapExtended.set(0);

    auto size = 16 * MemoryConstants::megaByte;

    auto status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.size = size;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(allocData.size, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(allocData.size, static_cast<DrmAllocation *>(allocation)->getBO()->peekSize());
    EXPECT_TRUE(allocation->getGpuAddress() % MemoryConstants::pageSize2M == 0u);

    size = 32 * MemoryConstants::megaByte;
    allocData.size = size;
    auto allocation2 = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    ASSERT_NE(nullptr, allocation2);
    EXPECT_EQ(allocData.size, allocation2->getUnderlyingBufferSize());
    EXPECT_EQ(allocData.size, static_cast<DrmAllocation *>(allocation2)->getBO()->peekSize());
    EXPECT_TRUE(allocation2->getGpuAddress() % MemoryConstants::pageSize2M == 0u);

    memoryManager->freeGraphicsMemory(allocation);
    memoryManager->freeGraphicsMemory(allocation2);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenEnabled2MBSizeAlignmentWhenAllocatingLargeImageAllocationThenUseProperAlignment) {
    auto mockProductHelper = new MockProductHelper;
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper.reset(mockProductHelper);
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = true;

    ASSERT_TRUE(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper->is2MBLocalMemAlignmentEnabled());

    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::image1D;
    imgDesc.imageWidth = 2 * MemoryConstants::megaByte + 1;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::image;
    allocData.flags.resource48Bit = true;
    allocData.imgInfo = &imgInfo;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_TRUE(allocData.imgInfo->useLocalMemory);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());

    auto gmm = allocation->getDefaultGmm();
    EXPECT_NE(nullptr, gmm);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.NonLocalOnly);

    auto gpuAddress = allocation->getGpuAddress();
    auto sizeAligned = alignUp(allocData.imgInfo->size, MemoryConstants::pageSize2M);

    EXPECT_NE(0u, gpuAddress);

    auto gmmHelper = device->getGmmHelper();
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapStandard2MB)), gpuAddress);
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapStandard2MB)), gpuAddress);
    EXPECT_EQ(0u, allocation->getGpuBaseAddress());
    EXPECT_EQ(sizeAligned, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(gpuAddress, reinterpret_cast<uint64_t>(allocation->getReservedAddressPtr()));
    EXPECT_EQ(sizeAligned, allocation->getReservedAddressSize());

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto bo = drmAllocation->getBO();
    EXPECT_NE(nullptr, bo);
    EXPECT_EQ(gpuAddress, bo->peekAddress());
    EXPECT_EQ(sizeAligned, bo->peekSize());

    memoryManager->freeGraphicsMemory(allocation);
}

struct DrmMemoryManagerToTestCopyMemoryToAllocationBanks : public DrmMemoryManager {
    DrmMemoryManagerToTestCopyMemoryToAllocationBanks(ExecutionEnvironment &executionEnvironment, size_t lockableLocalMemorySize)
        : DrmMemoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment) {
        lockedLocalMemorySize = lockableLocalMemorySize;
    }
    void *lockBufferObject(BufferObject *bo) override {
        if (lockedLocalMemorySize > 0) {
            if (static_cast<uint32_t>(bo->peekHandle()) < lockedLocalMemory.size()) {
                lockedLocalMemory[bo->peekHandle()].reset(new uint8_t[lockedLocalMemorySize]);
                return lockedLocalMemory[bo->peekHandle()].get();
            }
        }
        return nullptr;
    }
    void unlockBufferObject(BufferObject *bo) override {
    }
    std::array<std::unique_ptr<uint8_t[]>, 4> lockedLocalMemory;
    size_t lockedLocalMemorySize = 0;
};

TEST(DrmMemoryManagerCopyMemoryToAllocationBanksTest, givenDrmMemoryManagerWhenCopyMemoryToAllocationOnSpecificMemoryBanksThenAllocationIsFilledWithCorrectDataOnSpecificBanks) {
    uint8_t sourceData[64]{};
    size_t offset = 3;
    size_t sourceAllocationSize = sizeof(sourceData);
    size_t destinationAllocationSize = sourceAllocationSize + offset;
    MockExecutionEnvironment executionEnvironment;
    auto drm = new DrmMock(mockFd, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface.reset(new OSInterface());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    DrmMemoryManagerToTestCopyMemoryToAllocationBanks drmMemoryManger(executionEnvironment, destinationAllocationSize);
    std::vector<uint8_t> dataToCopy(sourceAllocationSize, 1u);

    MockDrmAllocation mockAllocation(0u, AllocationType::workPartitionSurface, MemoryPool::localMemory);

    mockAllocation.storageInfo.memoryBanks = 0b1111;
    DeviceBitfield memoryBanksToCopy = 0b1010;
    mockAllocation.bufferObjects.clear();

    for (auto index = 0u; index < 4; index++) {
        drmMemoryManger.lockedLocalMemory[index].reset();
        mockAllocation.bufferObjects.push_back(new BufferObject(0u, drm, 3, index, sourceAllocationSize, 3));
    }

    auto ret = drmMemoryManger.copyMemoryToAllocationBanks(&mockAllocation, offset, dataToCopy.data(), dataToCopy.size(), memoryBanksToCopy);
    EXPECT_TRUE(ret);

    EXPECT_EQ(nullptr, drmMemoryManger.lockedLocalMemory[0].get());
    ASSERT_NE(nullptr, drmMemoryManger.lockedLocalMemory[1].get());
    EXPECT_EQ(nullptr, drmMemoryManger.lockedLocalMemory[2].get());
    ASSERT_NE(nullptr, drmMemoryManger.lockedLocalMemory[3].get());

    EXPECT_EQ(0, memcmp(ptrOffset(drmMemoryManger.lockedLocalMemory[1].get(), offset), dataToCopy.data(), dataToCopy.size()));
    EXPECT_EQ(0, memcmp(ptrOffset(drmMemoryManger.lockedLocalMemory[3].get(), offset), dataToCopy.data(), dataToCopy.size()));
    for (auto index = 0u; index < 4; index++) {
        delete mockAllocation.bufferObjects[index];
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenDrmWhenRetrieveMmapOffsetForBufferObjectSucceedsThenReturnTrueAndCorrectOffset) {
    mock->ioctlExpected.gemMmapOffset = 1;
    BufferObject bo(rootDeviceIndex, mock, 3, 1, 1024, 0);
    mock->mmapOffsetExpected = 21;

    uint64_t offset = 0;
    auto ret = memoryManager->retrieveMmapOffsetForBufferObject(rootDeviceIndex, bo, 0, offset);

    EXPECT_TRUE(ret);
    EXPECT_EQ(21u, offset);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenDrmWhenRetrieveMmapOffsetForBufferObjectFailsThenReturnFalse) {
    mock->ioctlExpected.gemMmapOffset = 2;
    BufferObject bo(rootDeviceIndex, mock, 3, 1, 1024, 0);
    mock->failOnMmapOffset = true;

    uint64_t offset = 0;
    auto ret = memoryManager->retrieveMmapOffsetForBufferObject(rootDeviceIndex, bo, 0, offset);

    EXPECT_FALSE(ret);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenDrmWhenRetrieveMmapOffsetForBufferObjectFailsThenReturnFalseTestErrorDescription) {
    mock->ioctlExpected.gemMmapOffset = 2;
    BufferObject bo(rootDeviceIndex, mock, 3, 1, 1024, 0);
    mock->failOnMmapOffset = true;

    // To set the error value used to create the debug string in retrieveMmapOffsetForBufferObject()
    mock->errnoValue = -2;

    uint64_t offset = 0;
    auto ret = memoryManager->retrieveMmapOffsetForBufferObject(rootDeviceIndex, bo, 0, offset);

    const char *systemErrorDescription = nullptr;
    executionEnvironment->getErrorDescription(&systemErrorDescription);

    char expectedErrorDescription[256];
    snprintf(expectedErrorDescription, 256, "ioctl(DRM_IOCTL_I915_GEM_MMAP_OFFSET) failed with %d. errno=%d(%s)\n", -1, mock->getErrno(), strerror(mock->getErrno()));

    EXPECT_STREQ(expectedErrorDescription, systemErrorDescription);

    EXPECT_FALSE(ret);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenDrmWhenRetrieveMmapOffsetForBufferObjectIsCalledForLocalMemoryThenApplyCorrectFlags) {
    mock->ioctlExpected.gemMmapOffset = 5;
    BufferObject bo(rootDeviceIndex, mock, 3, 1, 1024, 0);

    uint64_t offset = 0;
    auto ret = memoryManager->retrieveMmapOffsetForBufferObject(rootDeviceIndex, bo, 0, offset);

    EXPECT_TRUE(ret);
    EXPECT_EQ(4u, mock->mmapOffsetFlags);

    mock->failOnMmapOffset = true;

    for (uint64_t flags : {I915_MMAP_OFFSET_WC, I915_MMAP_OFFSET_WB}) {
        ret = memoryManager->retrieveMmapOffsetForBufferObject(rootDeviceIndex, bo, flags, offset);

        EXPECT_FALSE(ret);
        EXPECT_EQ(flags, mock->mmapOffsetFlags);
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmWhenRetrieveMmapOffsetForBufferObjectIsCalledForSystemMemoryThenApplyCorrectFlags) {
    mock->ioctlExpected.gemMmapOffset = 8;
    BufferObject bo(rootDeviceIndex, mock, 3, 1, 1024, 0);

    uint64_t offset = 0;
    bool ret = false;
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    bo.setBOType(BufferObject::BOType::legacy);
    for (uint64_t flags : {I915_MMAP_OFFSET_WC, I915_MMAP_OFFSET_WB}) {
        ret = memoryManager->retrieveMmapOffsetForBufferObject(rootDeviceIndex, bo, flags, offset);

        EXPECT_TRUE(ret);
        if (productHelper.useGemCreateExtInAllocateMemoryByKMD()) {
            EXPECT_EQ(static_cast<uint32_t>(I915_MMAP_OFFSET_WB), mock->mmapOffsetFlags);
        } else {
            EXPECT_EQ(flags, mock->mmapOffsetFlags);
        }
    }

    bo.setBOType(BufferObject::BOType::coherent);
    for (uint64_t flags : {I915_MMAP_OFFSET_WC, I915_MMAP_OFFSET_WB}) {
        ret = memoryManager->retrieveMmapOffsetForBufferObject(rootDeviceIndex, bo, flags, offset);

        EXPECT_TRUE(ret);
        if (productHelper.useGemCreateExtInAllocateMemoryByKMD()) {
            EXPECT_EQ(static_cast<uint32_t>(I915_MMAP_OFFSET_WB), mock->mmapOffsetFlags);
        } else {
            EXPECT_EQ(flags, mock->mmapOffsetFlags);
        }
    }

    bo.setBOType(BufferObject::BOType::nonCoherent);
    for (uint64_t flags : {I915_MMAP_OFFSET_WC, I915_MMAP_OFFSET_WB}) {
        ret = memoryManager->retrieveMmapOffsetForBufferObject(rootDeviceIndex, bo, flags, offset);

        EXPECT_TRUE(ret);
        if (productHelper.useGemCreateExtInAllocateMemoryByKMD()) {
            EXPECT_EQ(static_cast<uint32_t>(I915_MMAP_OFFSET_WC), mock->mmapOffsetFlags);
        } else {
            EXPECT_EQ(flags, mock->mmapOffsetFlags);
        }
    }

    mock->failOnMmapOffset = true;

    for (uint64_t flags : {I915_MMAP_OFFSET_WC, I915_MMAP_OFFSET_WB}) {
        ret = memoryManager->retrieveMmapOffsetForBufferObject(rootDeviceIndex, bo, flags, offset);

        EXPECT_FALSE(ret);
        if (productHelper.useGemCreateExtInAllocateMemoryByKMD()) {
            EXPECT_EQ(static_cast<uint32_t>(I915_MMAP_OFFSET_WC), mock->mmapOffsetFlags);
        } else {
            EXPECT_EQ(flags, mock->mmapOffsetFlags);
        }
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmWhenGetBOTypeFromPatIndexIsCalledThenReturnCorrectBOType) {
    const bool isPatIndexSupported = memoryManager->getGmmHelper(mockRootDeviceIndex)->getRootDeviceEnvironment().getProductHelper().isVmBindPatIndexProgrammingSupported();
    if (!isPatIndexSupported) {
        EXPECT_EQ(BufferObject::BOType::legacy, memoryManager->getBOTypeFromPatIndex(0, isPatIndexSupported));
        EXPECT_EQ(BufferObject::BOType::legacy, memoryManager->getBOTypeFromPatIndex(1, isPatIndexSupported));
        EXPECT_EQ(BufferObject::BOType::legacy, memoryManager->getBOTypeFromPatIndex(2, isPatIndexSupported));
        EXPECT_EQ(BufferObject::BOType::legacy, memoryManager->getBOTypeFromPatIndex(3, isPatIndexSupported));
        EXPECT_EQ(BufferObject::BOType::legacy, memoryManager->getBOTypeFromPatIndex(4, isPatIndexSupported));
    } else {
        EXPECT_EQ(BufferObject::BOType::nonCoherent, memoryManager->getBOTypeFromPatIndex(0, isPatIndexSupported));
        EXPECT_EQ(BufferObject::BOType::nonCoherent, memoryManager->getBOTypeFromPatIndex(1, isPatIndexSupported));
        EXPECT_EQ(BufferObject::BOType::nonCoherent, memoryManager->getBOTypeFromPatIndex(2, isPatIndexSupported));
        EXPECT_EQ(BufferObject::BOType::coherent, memoryManager->getBOTypeFromPatIndex(3, isPatIndexSupported));
        EXPECT_EQ(BufferObject::BOType::coherent, memoryManager->getBOTypeFromPatIndex(4, isPatIndexSupported));
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenEligbleAllocationTypeWhenCheckingAllocationEligbleForCompletionFenceThenReturnTrue) {
    constexpr size_t arraySize = 5;

    AllocationType validAllocations[arraySize] = {
        AllocationType::commandBuffer,
        AllocationType::ringBuffer,
        AllocationType::deferredTasksList,
        AllocationType::semaphoreBuffer,
        AllocationType::tagBuffer};

    for (size_t i = 0; i < arraySize; i++) {
        EXPECT_TRUE(memoryManager->allocationTypeForCompletionFence(validAllocations[i]));
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenNotEligbleAllocationTypeWhenCheckingAllocationEligbleForCompletionFenceThenReturnFalse) {
    AllocationType invalidAllocations[] = {
        AllocationType::bufferHostMemory,
        AllocationType::constantSurface,
        AllocationType::fillPattern,
        AllocationType::globalSurface};

    for (size_t i = 0; i < 4; i++) {
        EXPECT_FALSE(memoryManager->allocationTypeForCompletionFence(invalidAllocations[i]));
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenNotEligbleAllocationTypeAndDebugFlagOverridingWhenCheckingAllocationEligbleForCompletionFenceThenReturnTrue) {
    DebugManagerStateRestore dbgState;
    debugManager.flags.UseDrmCompletionFenceForAllAllocations.set(1);

    AllocationType invalidAllocations[] = {
        AllocationType::bufferHostMemory,
        AllocationType::constantSurface,
        AllocationType::fillPattern,
        AllocationType::globalSurface};

    for (size_t i = 0; i < 4; i++) {
        EXPECT_TRUE(memoryManager->allocationTypeForCompletionFence(invalidAllocations[i]));
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenCompletionFenceEnabledWhenHandlingCompletionOfUsedAndEligbleAllocationThenCallWaitUserFence) {
    mock->ioctlExpected.total = -1;

    VariableBackup<bool> backupFenceSupported{&mock->completionFenceSupported, true};
    VariableBackup<bool> backupVmBindCallParent{&mock->isVmBindAvailableCall.callParent, false};
    VariableBackup<bool> backupVmBindReturnValue{&mock->isVmBindAvailableCall.returnValue, true};

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, 1024, AllocationType::commandBuffer});
    auto engine = memoryManager->getRegisteredEngines(rootDeviceIndex)[0];
    allocation->updateTaskCount(2, engine.osContext->getContextId());

    uint64_t expectedFenceAddress = castToUint64(const_cast<TagAddressType *>(engine.commandStreamReceiver->getTagAddress())) + TagAllocationLayout::completionFenceOffset;
    constexpr uint64_t expectedValue = 2;

    memoryManager->handleFenceCompletion(allocation);

    EXPECT_EQ(1u, mock->waitUserFenceCall.called);
    EXPECT_EQ(expectedFenceAddress, mock->waitUserFenceCall.address);
    EXPECT_EQ(expectedValue, mock->waitUserFenceCall.value);

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmAllocationWithDifferentScratchPageOptionsWhenHandleFenceCompletionThenCallResetStatsOnlyWithScratchPageDisabledAndProperFaultCheckThreshold) {
    mock->ioctlExpected.total = -1;

    DebugManagerStateRestore dbgStateRestore;

    VariableBackup<bool> backupFenceSupported{&mock->completionFenceSupported, true};
    VariableBackup<bool> backupVmBindCallParent{&mock->isVmBindAvailableCall.callParent, false};
    VariableBackup<bool> backupVmBindReturnValue{&mock->isVmBindAvailableCall.returnValue, true};

    for (int err : {EIO, ETIME}) {
        for (bool disableScratchPage : {false, true}) {
            for (int gpuFaultCheckThreshold : {0, 10}) {
                debugManager.flags.DisableScratchPages.set(disableScratchPage);
                debugManager.flags.GpuFaultCheckThreshold.set(gpuFaultCheckThreshold);

                mock->configureScratchPagePolicy();
                mock->configureGpuFaultCheckThreshold();

                mock->ioctlCnt.reset();
                mock->waitUserFenceCall.called = 0u;
                mock->checkResetStatusCalled = 0u;

                mock->waitUserFenceCall.failOnWaitUserFence = true;
                mock->errnoValue = err;

                auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, 1024, AllocationType::commandBuffer});
                auto engine = memoryManager->getRegisteredEngines(rootDeviceIndex)[0];
                allocation->updateTaskCount(2, engine.osContext->getContextId());

                uint64_t expectedFenceAddress = castToUint64(const_cast<TagAddressType *>(engine.commandStreamReceiver->getTagAddress())) + TagAllocationLayout::completionFenceOffset;
                constexpr uint64_t expectedValue = 2;

                memoryManager->handleFenceCompletion(allocation);

                EXPECT_EQ(1u, mock->waitUserFenceCall.called);
                EXPECT_EQ(expectedFenceAddress, mock->waitUserFenceCall.address);
                EXPECT_EQ(expectedValue, mock->waitUserFenceCall.value);
                if (err == EIO && disableScratchPage && gpuFaultCheckThreshold != 0) {
                    EXPECT_EQ(1u, mock->checkResetStatusCalled);
                } else {
                    EXPECT_EQ(0u, mock->checkResetStatusCalled);
                }

                memoryManager->freeGraphicsMemory(allocation);
            }
        }
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenCompletionFenceEnabledWhenHandlingCompletionOfNotUsedAndEligbleAllocationThenDoNotCallWaitUserFence) {
    mock->ioctlExpected.total = -1;

    VariableBackup<bool> backupFenceSupported{&mock->completionFenceSupported, true};
    VariableBackup<bool> backupVmBindCallParent{&mock->isVmBindAvailableCall.callParent, false};
    VariableBackup<bool> backupVmBindReturnValue{&mock->isVmBindAvailableCall.returnValue, true};

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, 1024, AllocationType::commandBuffer});

    memoryManager->handleFenceCompletion(allocation);

    EXPECT_EQ(0u, mock->waitUserFenceCall.called);

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenCompletionFenceEnabledWhenHandlingCompletionOfUsedAndNotEligbleAllocationThenDoNotCallWaitUserFence) {
    mock->ioctlExpected.total = -1;

    VariableBackup<bool> backupFenceSupported{&mock->completionFenceSupported, true};
    VariableBackup<bool> backupVmBindCallParent{&mock->isVmBindAvailableCall.callParent, false};
    VariableBackup<bool> backupVmBindReturnValue{&mock->isVmBindAvailableCall.returnValue, true};

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, 1024, AllocationType::globalSurface});
    auto engine = memoryManager->getRegisteredEngines(rootDeviceIndex)[0];
    allocation->updateTaskCount(2, engine.osContext->getContextId());

    memoryManager->handleFenceCompletion(allocation);

    EXPECT_EQ(0u, mock->waitUserFenceCall.called);

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenCompletionFenceEnabledWhenHandlingCompletionAndTagAddressIsNullThenDoNotCallWaitUserFence) {
    mock->ioctlExpected.total = -1;

    VariableBackup<bool> backupFenceSupported{&mock->completionFenceSupported, true};
    VariableBackup<bool> backupVmBindCallParent{&mock->isVmBindAvailableCall.callParent, false};
    VariableBackup<bool> backupVmBindReturnValue{&mock->isVmBindAvailableCall.returnValue, true};

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, 1024, AllocationType::commandBuffer});
    auto engine = memoryManager->getRegisteredEngines(rootDeviceIndex)[0];
    allocation->updateTaskCount(2, engine.osContext->getContextId());

    auto testCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(engine.commandStreamReceiver);
    auto backupTagAddress = testCsr->tagAddress;
    testCsr->tagAddress = nullptr;

    memoryManager->handleFenceCompletion(allocation);
    EXPECT_EQ(0u, mock->waitUserFenceCall.called);

    testCsr->tagAddress = backupTagAddress;

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenMultiSubDevicesBitfieldWhenAllocatingSbaTrackingBufferThenCorrectMultiHostAllocationReturned) {
    mock->ioctlExpected.total = -1;

    NEO::AllocationProperties properties{device->getRootDeviceIndex(), true, MemoryConstants::pageSize,
                                         NEO::AllocationType::debugSbaTrackingBuffer,
                                         false, false,
                                         0b0011};

    const uint64_t gpuAddresses[] = {0, 0x12340000};

    for (auto gpuAddress : gpuAddresses) {
        properties.gpuAddress = gpuAddress;

        auto sbaBuffer = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(properties));

        EXPECT_NE(nullptr, sbaBuffer);

        EXPECT_EQ(MemoryPool::system4KBPages, sbaBuffer->getMemoryPool());
        EXPECT_EQ(2u, sbaBuffer->getNumGmms());

        EXPECT_NE(nullptr, sbaBuffer->getUnderlyingBuffer());
        EXPECT_EQ(MemoryConstants::pageSize, sbaBuffer->getUnderlyingBufferSize());

        auto &bos = sbaBuffer->getBOs();

        EXPECT_NE(nullptr, bos[0]);
        EXPECT_NE(nullptr, bos[1]);

        if (gpuAddress != 0) {
            EXPECT_EQ(gpuAddress, sbaBuffer->getGpuAddress());

            EXPECT_EQ(gpuAddress, bos[0]->peekAddress());
            EXPECT_EQ(gpuAddress, bos[1]->peekAddress());
            EXPECT_EQ(0u, sbaBuffer->getReservedAddressPtr());
        } else {
            EXPECT_EQ(bos[0]->peekAddress(), bos[1]->peekAddress());
            EXPECT_NE(nullptr, sbaBuffer->getReservedAddressPtr());
            EXPECT_NE(0u, sbaBuffer->getGpuAddress());
        }

        EXPECT_EQ(nullptr, bos[2]);
        EXPECT_EQ(nullptr, bos[3]);

        memoryManager->freeGraphicsMemory(sbaBuffer);
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenSingleSubDevicesBitfieldWhenAllocatingSbaTrackingBufferThenSingleHostAllocationReturned) {
    mock->ioctlExpected.total = -1;

    NEO::AllocationProperties properties{device->getRootDeviceIndex(), true, MemoryConstants::pageSize,
                                         NEO::AllocationType::debugSbaTrackingBuffer,
                                         false, false,
                                         0b0001};

    const uint64_t gpuAddresses[] = {0, 0x12340000};

    for (auto gpuAddress : gpuAddresses) {
        properties.gpuAddress = gpuAddress;

        auto sbaBuffer = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(properties));

        EXPECT_NE(nullptr, sbaBuffer);

        EXPECT_EQ(MemoryPool::system4KBPages, sbaBuffer->getMemoryPool());
        EXPECT_EQ(1u, sbaBuffer->getNumGmms());

        EXPECT_NE(nullptr, sbaBuffer->getUnderlyingBuffer());
        EXPECT_EQ(MemoryConstants::pageSize, sbaBuffer->getUnderlyingBufferSize());

        auto &bos = sbaBuffer->getBOs();

        EXPECT_NE(nullptr, bos[0]);
        EXPECT_EQ(nullptr, bos[1]);
        EXPECT_EQ(nullptr, bos[2]);
        EXPECT_EQ(nullptr, bos[3]);

        if (gpuAddress != 0) {
            EXPECT_EQ(gpuAddress, sbaBuffer->getGpuAddress());
            EXPECT_EQ(gpuAddress, bos[0]->peekAddress());
        } else {
            EXPECT_NE(0u, sbaBuffer->getGpuAddress());
        }

        memoryManager->freeGraphicsMemory(sbaBuffer);
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenMakeBosResidentErrorWhenRegisteringMemoryAllocationThenErrorIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.MakeEachAllocationResident.set(1);
    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(rootDeviceIndex));
    drm.vmIdToCreate = 1;
    drm.createVirtualMemoryAddressSpace(1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::system4KBPages);
    allocation.makeBOsResidentResult = ENOSPC;

    EXPECT_EQ(MemoryManager::AllocationStatus::Error, memoryManager->registerSysMemAlloc(&allocation));

    EXPECT_EQ(MemoryManager::AllocationStatus::Error, memoryManager->registerLocalMemAlloc(&allocation, rootDeviceIndex));
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenMakeBosResidentSuccessWhenRegisteringMemoryAllocationThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.MakeEachAllocationResident.set(1);
    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(0));
    drm.vmIdToCreate = 1;
    drm.createVirtualMemoryAddressSpace(1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::system4KBPages);
    allocation.makeBOsResidentResult = 0;

    EXPECT_EQ(MemoryManager::AllocationStatus::Success, memoryManager->registerSysMemAlloc(&allocation));

    EXPECT_EQ(MemoryManager::AllocationStatus::Success, memoryManager->registerLocalMemAlloc(&allocation, 0));
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerWhenGpuAddressReservationIsAttemptedWithKnownAddressAtIndex1ThenAddressFromGfxPartitionIsUsed) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(1);
    uint32_t rootDeviceIndexReserved = 0;
    auto gmmHelper = memoryManager->getGmmHelper(1);
    auto addressRange = memoryManager->reserveGpuAddressOnHeap(0ull, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved, NEO::HeapIndex::heapStandard, MemoryConstants::pageSize64k);

    EXPECT_EQ(rootDeviceIndexReserved, 1u);
    EXPECT_LE(memoryManager->getGfxPartition(1)->getHeapBase(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memoryManager->getGfxPartition(1)->getHeapLimit(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    uint64_t requiredAddr = addressRange.address;
    memoryManager->freeGpuAddress(addressRange, 1);
    addressRange = memoryManager->reserveGpuAddressOnHeap(requiredAddr, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved, NEO::HeapIndex::heapStandard, MemoryConstants::pageSize64k);

    EXPECT_EQ(rootDeviceIndexReserved, 1u);
    EXPECT_EQ(addressRange.address, requiredAddr);
    EXPECT_LE(memoryManager->getGfxPartition(1)->getHeapBase(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memoryManager->getGfxPartition(1)->getHeapLimit(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    memoryManager->freeGpuAddress(addressRange, 1);

    addressRange = memoryManager->reserveGpuAddress(0ull, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved);

    EXPECT_EQ(rootDeviceIndexReserved, 1u);
    EXPECT_LE(memoryManager->getGfxPartition(1)->getHeapBase(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memoryManager->getGfxPartition(1)->getHeapLimit(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    requiredAddr = addressRange.address;
    memoryManager->freeGpuAddress(addressRange, 1);
    addressRange = memoryManager->reserveGpuAddress(requiredAddr, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved);

    EXPECT_EQ(rootDeviceIndexReserved, 1u);
    EXPECT_EQ(addressRange.address, requiredAddr);
    EXPECT_LE(memoryManager->getGfxPartition(1)->getHeapBase(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memoryManager->getGfxPartition(1)->getHeapLimit(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    memoryManager->freeGpuAddress(addressRange, 1);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerWhenCpuAddressReservationIsAttemptedThenCorrectAddressRangesAreReturned) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(1);

    auto addressRange = memoryManager->reserveCpuAddress(0, 0);
    EXPECT_EQ(0u, addressRange.address);
    EXPECT_EQ(0u, addressRange.size);

    addressRange = memoryManager->reserveCpuAddress(0, MemoryConstants::pageSize);
    EXPECT_NE(0u, addressRange.address);
    EXPECT_EQ(MemoryConstants::pageSize, addressRange.size);
    memoryManager->freeCpuAddress(addressRange);

    addressRange = memoryManager->reserveCpuAddress(MemoryConstants::pageSize * 1234, MemoryConstants::pageSize);
    EXPECT_NE(0u, addressRange.address);
    EXPECT_EQ(MemoryConstants::pageSize, addressRange.size);
    memoryManager->freeCpuAddress(addressRange);

    VariableBackup<bool> backup(&SysCalls::failMmap, true);
    addressRange = memoryManager->reserveCpuAddress(0, 0);
    EXPECT_EQ(0u, addressRange.address);
    EXPECT_EQ(0u, addressRange.size);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, given57bAddressSpaceCpuAndGpuWhenAllocatingHostUSMThenAddressFromExtendedHeapIsPassedAsHintAndSetAsGpuAddressAndReservedAddress) {
    if (defaultHwInfo->capabilityTable.gpuAddressSpace < maxNBitValue(57)) {
        GTEST_SKIP();
    }
    auto mockGfxPartition = std::make_unique<MockGfxPartition>();
    mockGfxPartition->initHeap(HeapIndex::heapExtendedHost, maxNBitValue(48) + 1, MemoryConstants::teraByte, MemoryConstants::pageSize64k);
    memoryManager->overrideGfxPartition(mockGfxPartition.release());
    VariableBackup<bool> backupCaptureExtendedPointers(&SysCalls::mmapCaptureExtendedPointers, true);
    VariableBackup<bool> backupAllowExtendedPointers(&SysCalls::mmapAllowExtendedPointers, true);
    SysCalls::mmapCapturedExtendedPointers.clear();
    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    drm.memoryInfo.reset(new MemoryInfo(regionInfo, drm));
    AllocationProperties allocationProperties(mockRootDeviceIndex, MemoryConstants::cacheLineSize, AllocationType::svmCpu, {});
    allocationProperties.flags.isUSMHostAllocation = true;
    auto hostUSM = memoryManager->allocateGraphicsMemoryInPreferredPool(allocationProperties, nullptr);
    EXPECT_NE(nullptr, hostUSM);

    EXPECT_EQ(2u, SysCalls::mmapCapturedExtendedPointers.size());
    auto gpuAddress = reinterpret_cast<uint64_t>(SysCalls::mmapCapturedExtendedPointers[0]);
    SysCalls::mmapCapturedExtendedPointers.clear();
    auto gmmHelper = memoryManager->getGmmHelper(mockRootDeviceIndex);
    EXPECT_LE(memoryManager->getGfxPartition(mockRootDeviceIndex)->getHeapBase(HeapIndex::heapExtendedHost), gmmHelper->decanonize(gpuAddress));
    EXPECT_GT(memoryManager->getGfxPartition(mockRootDeviceIndex)->getHeapLimit(HeapIndex::heapExtendedHost), gmmHelper->decanonize(gpuAddress));

    EXPECT_EQ(hostUSM->getGpuAddress(), gpuAddress);
    EXPECT_EQ(hostUSM->getReservedAddressPtr(), reinterpret_cast<void *>(gpuAddress));
    memoryManager->freeGraphicsMemory(hostUSM);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, given48bAddressSpaceCpuAnd57bGpuWhenAllocatingHostUSMThenAddressFromExtendedHeapIsPassedAsHintAndThenIgnored) {
    if (defaultHwInfo->capabilityTable.gpuAddressSpace < maxNBitValue(57)) {
        GTEST_SKIP();
    }
    auto mockGfxPartition = std::make_unique<MockGfxPartition>();
    mockGfxPartition->initHeap(HeapIndex::heapExtendedHost, maxNBitValue(48) + 1, MemoryConstants::teraByte, MemoryConstants::pageSize64k);
    memoryManager->overrideGfxPartition(mockGfxPartition.release());
    VariableBackup<bool> backupCaptureExtendedPointers(&SysCalls::mmapCaptureExtendedPointers, true);
    VariableBackup<bool> backupAllowExtendedPointers(&SysCalls::mmapAllowExtendedPointers, false);
    SysCalls::mmapCapturedExtendedPointers.clear();
    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    drm.memoryInfo.reset(new MemoryInfo(regionInfo, drm));
    AllocationProperties allocationProperties(mockRootDeviceIndex, MemoryConstants::cacheLineSize, AllocationType::svmCpu, {});
    allocationProperties.flags.isUSMHostAllocation = true;
    auto hostUSM = memoryManager->allocateGraphicsMemoryInPreferredPool(allocationProperties, nullptr);
    EXPECT_NE(nullptr, hostUSM);

    EXPECT_EQ(1u, SysCalls::mmapCapturedExtendedPointers.size());
    auto gpuAddress = reinterpret_cast<uint64_t>(SysCalls::mmapCapturedExtendedPointers[0]);
    SysCalls::mmapCapturedExtendedPointers.clear();
    auto gmmHelper = memoryManager->getGmmHelper(mockRootDeviceIndex);
    EXPECT_LE(memoryManager->getGfxPartition(mockRootDeviceIndex)->getHeapBase(HeapIndex::heapExtendedHost), gmmHelper->decanonize(gpuAddress));
    EXPECT_GT(memoryManager->getGfxPartition(mockRootDeviceIndex)->getHeapLimit(HeapIndex::heapExtendedHost), gmmHelper->decanonize(gpuAddress));

    EXPECT_NE(hostUSM->getGpuAddress(), gpuAddress);
    memoryManager->freeGraphicsMemory(hostUSM);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, given57bAddressSpaceCpuAndGpuAndDisabledHeapExtendedUsageForUsmHostWhenAllocatingHostUSMThenAddressFromExtendedHeapIsNotUsed) {
    if (defaultHwInfo->capabilityTable.gpuAddressSpace < maxNBitValue(57)) {
        GTEST_SKIP();
    }

    auto mockGfxPartition = std::make_unique<MockGfxPartition>();
    mockGfxPartition->initHeap(HeapIndex::heapExtendedHost, maxNBitValue(48) + 1, MemoryConstants::teraByte, MemoryConstants::pageSize64k);
    memoryManager->overrideGfxPartition(mockGfxPartition.release());
    DebugManagerStateRestore restorer;
    debugManager.flags.AllocateHostAllocationsInHeapExtendedHost.set(false);
    VariableBackup<bool> backupCaptureExtendedPointers(&SysCalls::mmapCaptureExtendedPointers, true);
    VariableBackup<bool> backupAllowExtendedPointers(&SysCalls::mmapAllowExtendedPointers, true);
    SysCalls::mmapCapturedExtendedPointers.clear();
    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    drm.memoryInfo.reset(new MemoryInfo(regionInfo, drm));
    AllocationProperties allocationProperties(mockRootDeviceIndex, MemoryConstants::cacheLineSize, AllocationType::svmCpu, {});
    allocationProperties.flags.isUSMHostAllocation = true;
    auto hostUSM = memoryManager->allocateGraphicsMemoryInPreferredPool(allocationProperties, nullptr);
    EXPECT_NE(nullptr, hostUSM);

    EXPECT_EQ(0u, SysCalls::mmapCapturedExtendedPointers.size());
    memoryManager->freeGraphicsMemory(hostUSM);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, given57bAddressSpaceCpuAndGpuWhenAllocatingSharedUSMThenAddressFromExtendedHeapIsPassedAsHintAndSetAsGpuAddressAndReservedAddress) {
    if (defaultHwInfo->capabilityTable.gpuAddressSpace < maxNBitValue(57)) {
        GTEST_SKIP();
    }

    auto mockGfxPartition = std::make_unique<MockGfxPartition>();
    mockGfxPartition->initHeap(HeapIndex::heapExtendedHost, maxNBitValue(48) + 1, MemoryConstants::teraByte, MemoryConstants::pageSize64k);
    memoryManager->overrideGfxPartition(mockGfxPartition.release());
    DebugManagerStateRestore restorer;
    debugManager.flags.AllocateSharedAllocationsInHeapExtendedHost.set(true);
    VariableBackup<bool> backupCaptureExtendedPointers(&SysCalls::mmapCaptureExtendedPointers, true);
    VariableBackup<bool> backupAllowExtendedPointers(&SysCalls::mmapAllowExtendedPointers, true);
    SysCalls::mmapCapturedExtendedPointers.clear();
    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    drm.memoryInfo.reset(new MemoryInfo(regionInfo, drm));
    drm.ioctlHelper = std::make_unique<MockIoctlHelper>(drm);

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::cacheLineSize;
    allocationData.rootDeviceIndex = mockRootDeviceIndex;
    allocationData.alignment = MemoryConstants::pageSize;
    allocationData.useMmapObject = true;

    auto sharedUSM = memoryManager->createSharedUnifiedMemoryAllocation(allocationData);
    EXPECT_NE(nullptr, sharedUSM);

    EXPECT_EQ(2u, SysCalls::mmapCapturedExtendedPointers.size());
    auto gpuAddress = reinterpret_cast<uint64_t>(SysCalls::mmapCapturedExtendedPointers[0]);
    SysCalls::mmapCapturedExtendedPointers.clear();
    auto gmmHelper = memoryManager->getGmmHelper(mockRootDeviceIndex);
    EXPECT_LE(memoryManager->getGfxPartition(mockRootDeviceIndex)->getHeapBase(HeapIndex::heapExtendedHost), gmmHelper->decanonize(gpuAddress));
    EXPECT_GT(memoryManager->getGfxPartition(mockRootDeviceIndex)->getHeapLimit(HeapIndex::heapExtendedHost), gmmHelper->decanonize(gpuAddress));

    EXPECT_EQ(sharedUSM->getGpuAddress(), gpuAddress);
    EXPECT_EQ(sharedUSM->getReservedAddressPtr(), reinterpret_cast<void *>(gpuAddress));
    memoryManager->freeGraphicsMemory(sharedUSM);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, given48bAddressSpaceCpuAnd57bGpuWhenAllocatingSharedUSMThenAddressFromExtendedHeapIsPassedAsHintAndThenIgnored) {
    if (defaultHwInfo->capabilityTable.gpuAddressSpace < maxNBitValue(57)) {
        GTEST_SKIP();
    }
    auto mockGfxPartition = std::make_unique<MockGfxPartition>();
    mockGfxPartition->initHeap(HeapIndex::heapExtendedHost, maxNBitValue(48) + 1, MemoryConstants::teraByte, MemoryConstants::pageSize64k);
    memoryManager->overrideGfxPartition(mockGfxPartition.release());
    DebugManagerStateRestore restorer;
    debugManager.flags.AllocateSharedAllocationsInHeapExtendedHost.set(true);
    VariableBackup<bool> backupCaptureExtendedPointers(&SysCalls::mmapCaptureExtendedPointers, true);
    VariableBackup<bool> backupAllowExtendedPointers(&SysCalls::mmapAllowExtendedPointers, false);
    SysCalls::mmapCapturedExtendedPointers.clear();
    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    drm.memoryInfo.reset(new MemoryInfo(regionInfo, drm));
    drm.ioctlHelper = std::make_unique<MockIoctlHelper>(drm);

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::cacheLineSize;
    allocationData.rootDeviceIndex = mockRootDeviceIndex;
    allocationData.alignment = MemoryConstants::pageSize;
    allocationData.useMmapObject = true;

    auto sharedUSM = memoryManager->createSharedUnifiedMemoryAllocation(allocationData);
    EXPECT_NE(nullptr, sharedUSM);

    EXPECT_EQ(1u, SysCalls::mmapCapturedExtendedPointers.size());
    auto gpuAddress = reinterpret_cast<uint64_t>(SysCalls::mmapCapturedExtendedPointers[0]);
    SysCalls::mmapCapturedExtendedPointers.clear();
    auto gmmHelper = memoryManager->getGmmHelper(mockRootDeviceIndex);
    EXPECT_LE(memoryManager->getGfxPartition(mockRootDeviceIndex)->getHeapBase(HeapIndex::heapExtendedHost), gmmHelper->decanonize(gpuAddress));
    EXPECT_GT(memoryManager->getGfxPartition(mockRootDeviceIndex)->getHeapLimit(HeapIndex::heapExtendedHost), gmmHelper->decanonize(gpuAddress));

    EXPECT_NE(sharedUSM->getGpuAddress(), gpuAddress);
    memoryManager->freeGraphicsMemory(sharedUSM);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, given57bAddressSpaceCpuAndGpuWhenAllocating48bResourceSharedUSMThenAddressFromExtendedHeapIsNotUsed) {
    if (defaultHwInfo->capabilityTable.gpuAddressSpace < maxNBitValue(57)) {
        GTEST_SKIP();
    }
    auto mockGfxPartition = std::make_unique<MockGfxPartition>();
    mockGfxPartition->initHeap(HeapIndex::heapExtendedHost, maxNBitValue(48) + 1, MemoryConstants::teraByte, MemoryConstants::pageSize64k);
    memoryManager->overrideGfxPartition(mockGfxPartition.release());
    DebugManagerStateRestore restorer;
    debugManager.flags.AllocateSharedAllocationsInHeapExtendedHost.set(true);
    VariableBackup<bool> backupCaptureExtendedPointers(&SysCalls::mmapCaptureExtendedPointers, true);
    VariableBackup<bool> backupAllowExtendedPointers(&SysCalls::mmapAllowExtendedPointers, true);
    SysCalls::mmapCapturedExtendedPointers.clear();
    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    drm.memoryInfo.reset(new MemoryInfo(regionInfo, drm));
    drm.ioctlHelper = std::make_unique<MockIoctlHelper>(drm);

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::cacheLineSize;
    allocationData.rootDeviceIndex = mockRootDeviceIndex;
    allocationData.alignment = MemoryConstants::pageSize;
    allocationData.useMmapObject = true;
    allocationData.flags.resource48Bit = true;

    auto sharedUSM = memoryManager->createSharedUnifiedMemoryAllocation(allocationData);
    EXPECT_NE(nullptr, sharedUSM);

    EXPECT_EQ(0u, SysCalls::mmapCapturedExtendedPointers.size());

    EXPECT_LT(sharedUSM->getGpuAddress(), maxNBitValue(48));
    EXPECT_EQ(sharedUSM->getReservedAddressPtr(), nullptr);
    memoryManager->freeGraphicsMemory(sharedUSM);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDebugVariableToToggleGpuVaBitsWhenAllocatingResourceInHeapExtendedThenSpecificBitIsToggled) {
    if (defaultHwInfo->capabilityTable.gpuAddressSpace < maxNBitValue(57)) {
        GTEST_SKIP();
    }
    auto mockGfxPartition = std::make_unique<MockGfxPartition>();
    mockGfxPartition->initHeap(HeapIndex::heapExtended, maxNBitValue(56) + 1, MemoryConstants::teraByte, MemoryConstants::pageSize64k);
    memoryManager->gfxPartitions[1] = std::move(mockGfxPartition);

    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(rootDeviceIndex));
    drm.memoryInfo.reset(new MemoryInfo(regionInfo, drm));
    drm.ioctlHelper = std::make_unique<MockIoctlHelper>(drm);
    memoryManager->localMemorySupported[rootDeviceIndex] = true;

    mock->ioctlExpected.gemCreateExt = 3;
    mock->ioctlExpected.gemWait = 3;
    mock->ioctlExpected.gemClose = 3;

    DebugManagerStateRestore restorer;
    EXPECT_EQ(4u, static_cast<uint32_t>(AllocationType::constantSurface));
    EXPECT_EQ(7u, static_cast<uint32_t>(AllocationType::globalSurface));
    debugManager.flags.ToggleBitIn57GpuVa.set("4:55,7:32");

    auto size = MemoryConstants::kiloByte;

    auto status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.size = size;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;

    {
        allocData.type = AllocationType::buffer;
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
        ASSERT_NE(nullptr, allocation);

        auto gpuVA = allocation->getGpuAddress();

        EXPECT_TRUE(isBitSet(gpuVA, 56));
        EXPECT_FALSE(isBitSet(gpuVA, 55));
        EXPECT_TRUE(isBitSet(gpuVA, 32));

        memoryManager->freeGraphicsMemory(allocation);
    }
    {
        allocData.type = AllocationType::constantSurface;
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
        ASSERT_NE(nullptr, allocation);

        auto gpuVA = allocation->getGpuAddress();

        EXPECT_TRUE(isBitSet(gpuVA, 56));
        EXPECT_TRUE(isBitSet(gpuVA, 55));
        EXPECT_TRUE(isBitSet(gpuVA, 32));

        memoryManager->freeGraphicsMemory(allocation);
    }

    {
        allocData.type = AllocationType::globalSurface;
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
        ASSERT_NE(nullptr, allocation);

        auto gpuVA = allocation->getGpuAddress();

        EXPECT_TRUE(isBitSet(gpuVA, 56));
        EXPECT_FALSE(isBitSet(gpuVA, 55));
        EXPECT_FALSE(isBitSet(gpuVA, 32));

        memoryManager->freeGraphicsMemory(allocation);
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenIsCompressionSupportedForShareableThenReturnCorrectValue) {
    EXPECT_FALSE(memoryManager->isCompressionSupportedForShareable(true));
    EXPECT_TRUE(memoryManager->isCompressionSupportedForShareable(false));
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenVmAdviseAtomicAttributeEqualZeroWhenCreateSharedUnifiedMemoryAllocationIsCalledThenNullptrReturned) {
    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    auto mockIoctlHelper = new MockIoctlHelper(*mock);
    drm.memoryInfo.reset(new MemoryInfo(regionInfo, drm));
    drm.ioctlHelper.reset(mockIoctlHelper);

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::cacheLineSize;
    allocationData.rootDeviceIndex = mockRootDeviceIndex;
    allocationData.alignment = MemoryConstants::pageSize;

    mockIoctlHelper->callBaseVmAdviseAtomicAttribute = false;
    mockIoctlHelper->vmAdviseAtomicAttribute = 0;

    auto sharedUSM = memoryManager->createSharedUnifiedMemoryAllocation(allocationData);
    EXPECT_EQ(nullptr, sharedUSM);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenVmAdviseAtomicAttributeNotPresentWhenCreateSharedUnifiedMemoryAllocationIsCalledThenAllocationIsCreatedSuccessfully) {
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.gemCreateExt = 1;
    mock->ioctlExpected.gemMmapOffset = 1;

    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    auto mockIoctlHelper = new MockIoctlHelper(*mock);
    drm.memoryInfo.reset(new MemoryInfo(regionInfo, drm));
    drm.ioctlHelper.reset(mockIoctlHelper);

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::cacheLineSize;
    allocationData.rootDeviceIndex = mockRootDeviceIndex;
    allocationData.alignment = MemoryConstants::pageSize;

    mockIoctlHelper->callBaseVmAdviseAtomicAttribute = false;
    mockIoctlHelper->vmAdviseAtomicAttribute = std::nullopt;

    auto sharedUSM = memoryManager->createSharedUnifiedMemoryAllocation(allocationData);
    EXPECT_NE(nullptr, sharedUSM);

    memoryManager->freeGraphicsMemory(sharedUSM);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenGfxPartitionWhenReleasedAndReinitializedThenNewGfxPartitionIsCorrect) {

    auto gfxPartition = memoryManager->getGfxPartition(0);

    auto heapExternal = gfxPartition->getHeapBase(HeapIndex::heapExternal);
    auto heapStandard = gfxPartition->getHeapBase(HeapIndex::heapStandard);
    auto heapStandard64KB = gfxPartition->getHeapBase(HeapIndex::heapStandard64KB);
    auto heapSvm = gfxPartition->getHeapBase(HeapIndex::heapSvm);
    auto heapExtended = gfxPartition->getHeapBase(HeapIndex::heapExtended);
    auto heapExternalFrontWindow = gfxPartition->getHeapBase(HeapIndex::heapExternalFrontWindow);
    auto heapExternalDeviceFrontWindow = gfxPartition->getHeapBase(HeapIndex::heapExternalDeviceFrontWindow);
    auto heapInternalFrontWindow = gfxPartition->getHeapBase(HeapIndex::heapInternalFrontWindow);
    auto heapInternalDeviceFrontWindow = gfxPartition->getHeapBase(HeapIndex::heapInternalDeviceFrontWindow);

    memoryManager->releaseDeviceSpecificGfxPartition(0);
    EXPECT_EQ(nullptr, memoryManager->getGfxPartition(0));
    memoryManager->reInitDeviceSpecificGfxPartition(0);

    EXPECT_NE(nullptr, memoryManager->getGfxPartition(0));

    gfxPartition = memoryManager->getGfxPartition(0);

    auto heapExternal2 = gfxPartition->getHeapBase(HeapIndex::heapExternal);
    auto heapStandard2 = gfxPartition->getHeapBase(HeapIndex::heapStandard);
    auto heapStandard64KB2 = gfxPartition->getHeapBase(HeapIndex::heapStandard64KB);
    auto heapSvm2 = gfxPartition->getHeapBase(HeapIndex::heapSvm);
    auto heapExtended2 = gfxPartition->getHeapBase(HeapIndex::heapExtended);
    auto heapExternalFrontWindow2 = gfxPartition->getHeapBase(HeapIndex::heapExternalFrontWindow);
    auto heapExternalDeviceFrontWindow2 = gfxPartition->getHeapBase(HeapIndex::heapExternalDeviceFrontWindow);
    auto heapInternalFrontWindow2 = gfxPartition->getHeapBase(HeapIndex::heapInternalFrontWindow);
    auto heapInternalDeviceFrontWindow2 = gfxPartition->getHeapBase(HeapIndex::heapInternalDeviceFrontWindow);

    EXPECT_EQ(heapExternal, heapExternal2);
    EXPECT_EQ(heapStandard, heapStandard2);
    EXPECT_EQ(heapStandard64KB, heapStandard64KB2);
    EXPECT_EQ(heapSvm, heapSvm2);
    EXPECT_EQ(heapExtended, heapExtended2);
    EXPECT_EQ(heapExternalFrontWindow, heapExternalFrontWindow2);
    EXPECT_EQ(heapExternalDeviceFrontWindow, heapExternalDeviceFrontWindow2);
    EXPECT_EQ(heapInternalFrontWindow, heapInternalFrontWindow2);
    EXPECT_EQ(heapInternalDeviceFrontWindow, heapInternalDeviceFrontWindow2);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDeviceUsmAllocationWhenLocalOnlyFlagValueComputedThenProductHelperIsNotUsed) {
    constexpr bool preferCompressed{false};
    MockProductHelper productHelper{};

    EXPECT_EQ(0U, productHelper.getStorageInfoLocalOnlyFlagCalled);

    productHelper.getStorageInfoLocalOnlyFlagResult = false;
    EXPECT_EQ(memoryManager->getLocalOnlyRequired(AllocationType::buffer, productHelper, nullptr, preferCompressed), true);
    EXPECT_EQ(memoryManager->getLocalOnlyRequired(AllocationType::svmGpu, productHelper, nullptr, preferCompressed), true);

    EXPECT_EQ(0U, productHelper.getStorageInfoLocalOnlyFlagCalled);
}
