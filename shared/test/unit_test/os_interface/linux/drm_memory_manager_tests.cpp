/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/linux/drm_memory_manager_tests.h"

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_stream/tag_allocation_layout.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/local_memory_usage.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/linux/drm_fabric.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_bind.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/linux/mock_buffer_object.h"
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
#include "shared/test/common/mocks/mock_release_helper.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_fixture.h"
#include "shared/test/common/os_interface/linux/drm_mock_cache_info.h"
#include "shared/test/common/os_interface/linux/drm_mock_memory_info.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "clos_matchers.h"
#include "gtest/gtest.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <vector>

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

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenCallingGetImportHandleFromNullReservedHandleDataThenFailureIsReturned) {
    int result = memoryManager->getImportHandleFromReservedHandleData(nullptr, rootDeviceIndex);

    EXPECT_EQ(-1, result);
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
    EXPECT_EQ(nullptr, fragment->residency);

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

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenCreatePhysicalGraphicsMemoryFromSharedHandleThenSuccessReturned) {
    mock->ioctlExpected.total = -1;

    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(MemoryConstants::pageSize));
    this->mock->outputHandle = 2u;

    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::buffer, false, mockDeviceBitfield);
    properties.flags.isHostInaccessibleAllocation = true;
    TestedDrmMemoryManager::OsHandleData osHandleData{1u};

    auto allocation = memoryManager->createPhysicalGraphicsMemoryFromSharedHandle(osHandleData, properties);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::systemCpuInaccessible, allocation->getMemoryPool());

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenImportedPhysicalDeviceMemoryFreedBeforeItsMappingWhenMappingIsUnmappedThenSharedHandleIsClosedOnceAndUnregistered) {
    mock->ioctlExpected.total = -1;

    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(MemoryConstants::pageSize));
    this->mock->outputHandle = 2u;
    const auto sharedHandleKey = std::make_pair(2, rootDeviceIndex);

    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::buffer, false, mockDeviceBitfield);
    properties.flags.isHostInaccessibleAllocation = true;
    TestedDrmMemoryManager::OsHandleData osHandleData{1u};

    auto physicalAllocation = memoryManager->createPhysicalGraphicsMemoryFromSharedHandle(osHandleData, properties);
    ASSERT_NE(nullptr, physicalAllocation);
    EXPECT_TRUE(memoryManager->sharedBoHandles.contains(sharedHandleKey));

    const uint64_t gpuRange = 0x100000;
    MultiGraphicsAllocation mapping(rootDeviceIndex);
    ASSERT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(mapping, physicalAllocation, gpuRange, MemoryConstants::pageSize, nullptr, 0u));
    auto mappedAllocation = mapping.getGraphicsAllocation(rootDeviceIndex);
    ASSERT_NE(nullptr, mappedAllocation);

    const auto gemClosesBeforeRelease = mock->ioctlCnt.gemClose.load();
    memoryManager->freeGraphicsMemory(physicalAllocation);
    EXPECT_EQ(gemClosesBeforeRelease, mock->ioctlCnt.gemClose.load());
    EXPECT_TRUE(memoryManager->sharedBoHandles.contains(sharedHandleKey));

    auto osContext = device->getDefaultEngine().osContext;
    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(mapping, mappedAllocation, gpuRange, MemoryConstants::pageSize, osContext, rootDeviceIndex));
    EXPECT_EQ(gemClosesBeforeRelease + 1, mock->ioctlCnt.gemClose.load());
    EXPECT_FALSE(memoryManager->sharedBoHandles.contains(sharedHandleKey));
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenSameBoHandleRegisteredOnSeveralRootDevicesWhenLookingUpEachSharedHandleThenItsSharedOwnershipIsAcquired) {
    const std::pair<int, uint32_t> registeredHandles[] = {{29, 0u}, {17, 1u}, {17, 2u}, {17, 3u}, {17, 4u}, {17, 5u}, {17, 6u}, {17, 7u}};
    std::vector<BufferObjectHandleWrapper> owners;
    owners.reserve(std::size(registeredHandles));
    for (const auto &[boHandle, handleRootDeviceIndex] : registeredHandles) {
        owners.emplace_back(boHandle, handleRootDeviceIndex);
        memoryManager->sharedBoHandles.insert({std::make_pair(boHandle, handleRootDeviceIndex), owners.back().acquireWeakOwnership()});
    }

    for (const auto &[boHandle, handleRootDeviceIndex] : registeredHandles) {
        auto sharedWrapper = memoryManager->tryToGetBoHandleWrapperWithSharedOwnership(boHandle, handleRootDeviceIndex);
        EXPECT_FALSE(sharedWrapper.canCloseBoHandle());
    }

    memoryManager->sharedBoHandles.clear();
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenUnmappedPhysicalDeviceMemoryMappingWhenSameRangeIsMappedAgainThenMappingAllocationAndItsBufferObjectAreReused) {
    mock->ioctlExpected.total = -1;

    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(MemoryConstants::pageSize));
    this->mock->outputHandle = 2u;

    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::buffer, false, mockDeviceBitfield);
    properties.flags.isHostInaccessibleAllocation = true;
    TestedDrmMemoryManager::OsHandleData osHandleData{1u};

    auto physicalAllocation = memoryManager->createPhysicalGraphicsMemoryFromSharedHandle(osHandleData, properties);
    ASSERT_NE(nullptr, physicalAllocation);

    const uint64_t gpuRange = 0x100000;
    MultiGraphicsAllocation mapping(rootDeviceIndex);
    ASSERT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(mapping, physicalAllocation, gpuRange, MemoryConstants::pageSize, nullptr, 0u));
    auto mappedAllocation = static_cast<DrmAllocation *>(mapping.getGraphicsAllocation(rootDeviceIndex));
    ASSERT_NE(nullptr, mappedAllocation);
    auto mappedBufferObject = mappedAllocation->getBO();

    auto osContext = device->getDefaultEngine().osContext;
    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(mapping, mappedAllocation, gpuRange, MemoryConstants::pageSize, osContext, rootDeviceIndex));
    EXPECT_EQ(nullptr, mapping.getGraphicsAllocation(rootDeviceIndex));

    ASSERT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(mapping, physicalAllocation, gpuRange, MemoryConstants::pageSize, nullptr, 0u));
    EXPECT_EQ(mappedAllocation, mapping.getGraphicsAllocation(rootDeviceIndex));
    EXPECT_EQ(mappedBufferObject, mappedAllocation->getBO());

    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(mapping, mappedAllocation, gpuRange, MemoryConstants::pageSize, osContext, rootDeviceIndex));
    memoryManager->freeGraphicsMemory(physicalAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenCreatePhysicalGraphicsMemoryFromSharedHandleForHostAllocationThenMmapOffsetIsRetrieved) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemMmapOffset = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(MemoryConstants::pageSize));
    this->mock->outputHandle = 2u;

    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::buffer, false, mockDeviceBitfield);
    properties.flags.isHostInaccessibleAllocation = false;
    TestedDrmMemoryManager::OsHandleData osHandleData{1u};

    auto allocation = memoryManager->createPhysicalGraphicsMemoryFromSharedHandle(osHandleData, properties);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::system4KBPages, allocation->getMemoryPool());

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenCreatePhysicalGraphicsMemoryFromSharedHandleWhenPrimeFdToHandleFailsThenNullptrReturned) {
    mock->ioctlExpected.primeFdToHandle = 1;
    this->ioctlResExt = {mock->ioctlCnt.total, -1};
    mock->ioctlResExt = &this->ioctlResExt;

    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::buffer, false, mockDeviceBitfield);
    properties.flags.isHostInaccessibleAllocation = true;
    TestedDrmMemoryManager::OsHandleData osHandleData{1u};

    auto allocation = memoryManager->createPhysicalGraphicsMemoryFromSharedHandle(osHandleData, properties);
    EXPECT_EQ(nullptr, allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenCreatePhysicalGraphicsMemoryFromSharedHandleForHostAllocationWhenMmapOffsetFailsThenNullptrReturned) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemMmapOffset = 1;
    mock->ioctlExpected.gemClose = 1;

    mock->failOnMmapOffset = true;
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(MemoryConstants::pageSize));
    this->mock->outputHandle = 2u;

    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::buffer, false, mockDeviceBitfield);
    properties.flags.isHostInaccessibleAllocation = false;
    TestedDrmMemoryManager::OsHandleData osHandleData{1u};

    auto allocation = memoryManager->createPhysicalGraphicsMemoryFromSharedHandle(osHandleData, properties);
    EXPECT_EQ(nullptr, allocation);
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

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenAllocatePhysicalHostMemoryThenSuccessReturnedAndCacheableFlagIsOverridden) {
    mock->ioctlExpected.gemWait = static_cast<uint32_t>(AllocationType::count);
    mock->ioctlExpected.gemCreateExt = static_cast<uint32_t>(AllocationType::count);
    mock->ioctlExpected.gemMmapOffset = static_cast<uint32_t>(AllocationType::count);
    mock->ioctlExpected.gemClose = static_cast<uint32_t>(AllocationType::count);

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

        auto *gmmResourceParams = reinterpret_cast<GMM_RESCREATE_PARAMS *>(allocation->getDefaultGmm()->resourceParamsData.data());
        if (productHelper.overrideAllocationCpuCacheable(allocationData)) {
            EXPECT_TRUE(gmmResourceParams->Flags.Info.Cacheable);
        } else {
            auto gmmResourceUsage = CacheSettingsHelper::getGmmUsageType(allocationData.type, allocationData.flags.uncacheable, productHelper, *gmmHelper->getHardwareInfo());
            auto preferNoCpuAccess = CacheSettingsHelper::preferNoCpuAccess(gmmResourceUsage, gmmHelper->getRootDeviceEnvironment());
            bool cacheable = !preferNoCpuAccess && !CacheSettingsHelper::isUncachedType(gmmResourceUsage);
            EXPECT_EQ(cacheable, gmmResourceParams->Flags.Info.Cacheable);
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

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerWhenGpuAddressIsReservedWithStartAddressOnSpecifiedHeapAndFreedThenAddressFromGfxPartitionIsUsed) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    uint32_t rootDeviceIndexReserved = 10;
    auto gmmHelper = memoryManager->getGmmHelper(0);

    const auto &gfxPartition = memoryManager->getGfxPartition(rootDeviceIndices[0]);
    HeapIndex heap = HeapIndex::heapStandard64KB;
    auto heapBase = gfxPartition->getHeapBase(heap);
    const uint64_t requiredStartAddress = heapBase;

    auto alignment = memoryManager->selectAlignmentAndHeap(requiredStartAddress, MemoryConstants::pageSize, &heap);
    EXPECT_EQ(heap, HeapIndex::heapStandard64KB);
    EXPECT_EQ(alignment, MemoryConstants::pageSize64k);
    auto addressRange = memoryManager->reserveGpuAddressOnHeap(requiredStartAddress, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved, heap, alignment);
    EXPECT_EQ(0u, rootDeviceIndexReserved);
    EXPECT_NE(requiredStartAddress, addressRange.address);
    EXPECT_LE(memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapStandard64KB), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapStandard64KB), gmmHelper->decanonize(addressRange.address));

    memoryManager->freeGpuAddress(addressRange, 0);

    addressRange = memoryManager->reserveGpuAddressOnHeap(requiredStartAddress, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved, heap, alignment);
    EXPECT_EQ(0u, rootDeviceIndexReserved);
    EXPECT_NE(requiredStartAddress, addressRange.address);
    EXPECT_LE(memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapStandard64KB), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapStandard64KB), gmmHelper->decanonize(addressRange.address));
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

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerWhenGpuAddressReservationWithStartAddressIsAttemptedWithTheQuieriedHeapThenSuccessReturned) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    uint32_t rootDeviceIndexReserved = 1;

    HeapIndex heap = HeapIndex::heapStandard;
    auto gfxPartition = memoryManager->getGfxPartition(0);
    EXPECT_NE(gfxPartition->getHeapLimit(heap), 0u);
    auto heapBase = gfxPartition->getHeapBase(heap);
    auto requiredStartAddress = heapBase + 64;

    auto alignment = memoryManager->selectAlignmentAndHeap(requiredStartAddress, MemoryConstants::pageSize, &heap);
    EXPECT_EQ(heap, HeapIndex::heapStandard);
    EXPECT_EQ(MemoryConstants::pageSize, alignment);

    auto addressRange = memoryManager->reserveGpuAddressOnHeap(requiredStartAddress, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved, heap, alignment);
    EXPECT_NE(static_cast<int>(addressRange.address), 0);
    EXPECT_NE(static_cast<int>(addressRange.size), 0);
    memoryManager->freeGpuAddress(addressRange, 0);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerWhenSelectAlignmentAndHeapWithInvalidStartAddressOnSpecifiedHeapIsCalledThenHeapAndAlignmentUsedBasedOnSizeIsUsed) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);

    const auto &gfxPartition = memoryManager->getGfxPartition(rootDeviceIndices[0]);

    uint64_t maxHeapLimit = 0;
    for (uint32_t heapIndex = static_cast<uint32_t>(HeapIndex::heapInternalDeviceMemory); heapIndex < static_cast<uint32_t>(HeapIndex::totalHeaps); ++heapIndex) {
        maxHeapLimit = std::max(maxHeapLimit, gfxPartition->getHeapLimit(static_cast<HeapIndex>(heapIndex)));
    }
    const uint64_t requiredStartAddress = maxHeapLimit + 64;

    HeapIndex heap = HeapIndex::heapStandard64KB;
    size_t size = MemoryConstants::pageSize;
    auto alignment = memoryManager->selectAlignmentAndHeap(requiredStartAddress, size, &heap);
    AlignmentSelector::CandidateAlignment expectedAlignmentBase = memoryManager->alignmentSelector.selectAlignment(size);
    if (gfxPartition->getHeapLimit(HeapIndex::heapExtended) > 0) {
        EXPECT_EQ(heap, HeapIndex::heapExtended);
    } else {
        EXPECT_EQ(heap, expectedAlignmentBase.heap);
    }
    EXPECT_EQ(alignment, expectedAlignmentBase.alignment);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerWhenSelectAlignmentAndHeapCalledWithInvalidAddressThenAlignmentBasedOnSizeReturned) {

    auto gfxPartition = new MockGfxPartition();
    gfxPartition->getHeapIndexAndPageSizeBasedOnAddressCalled = 0u;
    gfxPartition->callBaseGetHeapIndexAndPageSizeBasedOnAddress = false;
    gfxPartition->getHeapIndexAndPageSizeBasedOnAddressReturnTrue = false;

    memoryManager->gfxPartitions[0].reset(gfxPartition);

    HeapIndex heap = HeapIndex::heapStandard;
    uint64_t requiredStartAddress = 1234u;

    auto alignment = memoryManager->selectAlignmentAndHeap(requiredStartAddress, MemoryConstants::pageSize, &heap);
    EXPECT_EQ(1u, gfxPartition->getHeapIndexAndPageSizeBasedOnAddressCalled);
    EXPECT_EQ(heap, HeapIndex::heapStandard64KB);
    EXPECT_EQ(alignment, MemoryConstants::pageSize64k);
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

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerWhenGpuAddressIsReservedWithInvalidStartAddressOnSpecifiedHeapAndFreedThenSomeOtherAddressIsUsed) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    uint32_t rootDeviceIndexReserved = 10;
    auto gmmHelper = memoryManager->getGmmHelper(0);

    const auto &gfxPartition = memoryManager->getGfxPartition(rootDeviceIndices[0]);
    HeapIndex heap = HeapIndex::heapStandard64KB;
    auto heapLimit = gfxPartition->getHeapLimit(heap);
    const uint64_t requiredStartAddress = heapLimit + 64;
    size_t alignment = MemoryConstants::pageSize64k;

    auto addressRange = memoryManager->reserveGpuAddressOnHeap(requiredStartAddress, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved, heap, alignment);
    EXPECT_EQ(0u, rootDeviceIndexReserved);
    EXPECT_NE(requiredStartAddress, addressRange.address);
    EXPECT_LE(memoryManager->getGfxPartition(0)->getHeapBase(heap), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memoryManager->getGfxPartition(0)->getHeapLimit(heap), gmmHelper->decanonize(addressRange.address));

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
    int ret = allocation->peekInternalHandle(this->memoryManager, handle, nullptr);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(handle, static_cast<uint64_t>(1337));

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenObtainDmaBufFdIsCalledThenBoIsExportedOnEachCallAndInternalHandleIsNotCached) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.handleToPrimeFd = 3;
    mock->outputFd = 1337;
    auto allocation = static_cast<DrmAllocation *>(this->memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, 10 * MemoryConstants::pageSize, true)));
    ASSERT_NE(allocation->getBO(), nullptr);

    EXPECT_EQ(1337, this->memoryManager->obtainDmaBufFd(*allocation));
    EXPECT_EQ(static_cast<uint32_t>(allocation->getBO()->peekHandle()), mock->inputHandle);
    EXPECT_EQ(1337, this->memoryManager->obtainDmaBufFd(*allocation));

    uint64_t internalHandle = 0;
    EXPECT_EQ(0, allocation->peekInternalHandle(this->memoryManager, internalHandle, nullptr));

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenBufferObjectMappedFromOffsetOrOnlyInPartWhenCheckingPartialObjectMappingThenTrueIsReturned) {
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);
    EXPECT_FALSE(allocation.isPartialObjectMapping());

    MockBufferObject bo(rootDeviceIndex, mock, 3, 0, MemoryConstants::pageSize64k, 1);
    allocation.bufferObjects[0] = &bo;
    EXPECT_FALSE(allocation.isPartialObjectMapping());

    bo.setVirtualMappingSize(MemoryConstants::pageSize64k);
    EXPECT_FALSE(allocation.isPartialObjectMapping());

    bo.setVirtualMappingSize(MemoryConstants::pageSize);
    EXPECT_TRUE(allocation.isPartialObjectMapping());

    bo.setVirtualMappingSize(0u);
    bo.setPhysicalMemoryOffset(MemoryConstants::pageSize);
    EXPECT_TRUE(allocation.isPartialObjectMapping());
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
    int ret = allocation->createInternalHandle(this->memoryManager, 0u, handle, nullptr);
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
    int ret = allocation->createInternalHandle(this->memoryManager, 0u, handle, nullptr);
    ASSERT_EQ(ret, 0);
    allocation->clearInternalHandle(0u);
    ret = allocation->createInternalHandle(this->memoryManager, 0u, handle, nullptr);
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
    int ret = allocation->peekInternalHandle(this->memoryManager, handle, nullptr);
    ASSERT_EQ(ret, -1);

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenPeekInternalHandleIsCalledWithReservedHandleDataAndObtainReservedHandleDataSucceedsThenSuccessIsReturned) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.handleToPrimeFd = 1;
    mock->outputFd = 1337;
    memoryManager->mockObtainReservedHandleData = true;
    auto allocation = static_cast<DrmAllocation *>(this->memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, 10 * MemoryConstants::pageSize, true)));
    ASSERT_NE(allocation->getBO(), nullptr);
    uint8_t reservedHandleData[32] = {0};
    uint64_t handle = 0;
    int ret = allocation->peekInternalHandle(this->memoryManager, handle, reservedHandleData);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(handle, static_cast<uint64_t>(1337));

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenPeekInternalHandleIsCalledWithReservedHandleDataThenDmabufFdAndBoHandleArePassedToObtainReservedHandleData) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.handleToPrimeFd = 1;
    mock->outputFd = 1337;
    memoryManager->mockObtainReservedHandleData = true;
    auto allocation = static_cast<DrmAllocation *>(this->memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, 10 * MemoryConstants::pageSize, true)));
    ASSERT_NE(allocation->getBO(), nullptr);
    auto expectedBoHandle = static_cast<uint32_t>(allocation->getBO()->peekHandle());

    uint8_t reservedHandleData[32] = {0};
    uint64_t handle = 0;
    int ret = allocation->peekInternalHandle(this->memoryManager, handle, reservedHandleData);
    ASSERT_EQ(ret, 0);
    EXPECT_EQ(1337, memoryManager->obtainReservedHandleDataLastFd);
    EXPECT_EQ(expectedBoHandle, memoryManager->obtainReservedHandleDataLastBoHandle);

    memoryManager->obtainReservedHandleDataLastFd = -1;
    memoryManager->obtainReservedHandleDataLastBoHandle = 0;

    uint64_t cachedHandle = 0;
    ret = allocation->peekInternalHandle(this->memoryManager, cachedHandle, reservedHandleData);
    ASSERT_EQ(ret, 0);
    EXPECT_EQ(1337, memoryManager->obtainReservedHandleDataLastFd);
    EXPECT_EQ(expectedBoHandle, memoryManager->obtainReservedHandleDataLastBoHandle);

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenPeekInternalHandleIsCalledWithReservedHandleDataAndObtainReservedHandleDataFailsThenErrorIsReturnedAndDmabufFdIsClosed) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.handleToPrimeFd = 2;
    mock->outputFd = 1337;
    memoryManager->mockObtainReservedHandleData = true;
    memoryManager->mockObtainReservedHandleDataResult = -1;
    auto allocation = static_cast<DrmAllocation *>(this->memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, 10 * MemoryConstants::pageSize, true)));
    ASSERT_NE(allocation->getBO(), nullptr);
    uint8_t reservedHandleData[32] = {0};
    uint64_t handle = 0;
    int ret = allocation->peekInternalHandle(this->memoryManager, handle, reservedHandleData);
    ASSERT_EQ(ret, -1);
    EXPECT_EQ(1, closeCalledCount);
    EXPECT_EQ(1337, closeInputFd);

    closeCalledCount = 0;
    uint64_t retryHandle = 0;
    ret = allocation->peekInternalHandle(this->memoryManager, retryHandle, reservedHandleData);
    ASSERT_EQ(ret, -1);
    EXPECT_EQ(1, closeCalledCount);

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenPeekInternalHandleIsCalledWithHandleInCacheAndReservedHandleDataIsPassedTwoTimesThenObtainingDataIsSuccessFullyReturnedBothTimes) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.handleToPrimeFd = 1;
    mock->outputFd = 1337;
    memoryManager->mockObtainReservedHandleData = true;
    auto allocation = static_cast<DrmAllocation *>(this->memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, 10 * MemoryConstants::pageSize, true)));
    ASSERT_NE(allocation->getBO(), nullptr);

    uint8_t reservedHandleData[32] = {0};
    uint64_t handle = 0;
    int ret = allocation->peekInternalHandle(this->memoryManager, handle, reservedHandleData);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(handle, static_cast<uint64_t>(1337));

    memoryManager->mockObtainReservedHandleDataResult = 0;

    uint64_t handle2 = 0;
    ret = allocation->peekInternalHandle(this->memoryManager, handle2, reservedHandleData);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(handle2, static_cast<uint64_t>(1337));

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenPeekInternalHandleIsCalledWithHandleInCacheAndReservedHandleDataNotYetReadThenReservedHandleDataIsReturned) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.handleToPrimeFd = 1;
    mock->outputFd = 1337;
    auto allocation = static_cast<DrmAllocation *>(this->memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, 10 * MemoryConstants::pageSize, true)));
    ASSERT_NE(allocation->getBO(), nullptr);

    uint64_t handle = 0;
    int ret = allocation->peekInternalHandle(this->memoryManager, handle, nullptr);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(handle, static_cast<uint64_t>(1337));

    memoryManager->mockObtainReservedHandleData = true;
    memoryManager->mockObtainReservedHandleDataResult = 0;
    uint8_t reservedHandleData[32] = {0};
    uint64_t handle2 = 0;
    ret = allocation->peekInternalHandle(this->memoryManager, handle2, reservedHandleData);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(handle2, static_cast<uint64_t>(1337));

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenPeekInternalHandleIsCalledWithHandleInCacheAndReservedHandleDataNotYetReadAndObtainFailsThenErrorIsReturned) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.handleToPrimeFd = 1;
    mock->outputFd = 1337;
    auto allocation = static_cast<DrmAllocation *>(this->memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, 10 * MemoryConstants::pageSize, true)));
    ASSERT_NE(allocation->getBO(), nullptr);

    uint64_t handle = 0;
    int ret = allocation->peekInternalHandle(this->memoryManager, handle, nullptr);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(handle, static_cast<uint64_t>(1337));

    memoryManager->mockObtainReservedHandleData = true;
    memoryManager->mockObtainReservedHandleDataResult = -1;
    uint8_t reservedHandleData[32] = {0};
    uint64_t handle2 = 0;
    ret = allocation->peekInternalHandle(this->memoryManager, handle2, reservedHandleData);
    EXPECT_EQ(ret, -1);

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
    int ret = allocation->peekInternalHandle(this->memoryManager, handle0, nullptr);
    ASSERT_EQ(ret, 0);
    EXPECT_NE(mock->outputFd, static_cast<int32_t>(expectedFd));

    uint64_t handle1 = 0;
    uint64_t handle2 = 0;

    ret = allocation->peekInternalHandle(this->memoryManager, handle1, nullptr);
    ASSERT_EQ(ret, 0);
    ret = allocation->peekInternalHandle(this->memoryManager, handle2, nullptr);
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
    int ret = allocation->createInternalHandle(this->memoryManager, 0u, handle0, nullptr);
    ASSERT_EQ(ret, 0);
    EXPECT_NE(mock->outputFd, static_cast<int32_t>(expectedFd));

    uint64_t handle1 = 0;
    uint64_t handle2 = 0;

    ret = allocation->createInternalHandle(this->memoryManager, 0u, handle1, nullptr);
    ASSERT_EQ(ret, 0);
    ret = allocation->createInternalHandle(this->memoryManager, 0u, handle2, nullptr);
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
    int ret = allocation->peekInternalHandle(this->memoryManager, handleId, handle, nullptr);
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
    int ret = allocation->peekInternalHandle(this->memoryManager, handleId, handle0, nullptr);
    ASSERT_EQ(ret, 0);
    EXPECT_NE(mock->outputFd, static_cast<int32_t>(expectedFd));

    uint64_t handle1 = 0;
    uint64_t handle2 = 0;
    ret = allocation->peekInternalHandle(this->memoryManager, handleId, handle1, nullptr);
    ASSERT_EQ(ret, 0);
    ret = allocation->peekInternalHandle(this->memoryManager, handleId, handle2, nullptr);
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
    allocationData.size = 4 * MemoryConstants::kiloByte;
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

    allocationData.size = 4 * MemoryConstants::kiloByte;
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

    allocationData.size = 4 * MemoryConstants::kiloByte;
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
    BufferObject *bo = memoryManager->allocUserptrWithTemporaryGmm(1234, (size_t)1024, AllocationType::externalHostPtr, rootDeviceIndex);
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
    BufferObject *bo = memoryManager->allocUserptrWithTemporaryGmm(0, (size_t)1024, AllocationType::externalHostPtr, rootDeviceIndex);
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
    DrmMemoryManager drmMemoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, *executionEnvironment);
    EXPECT_EQ(nullptr, drmMemoryManager.peekGemCloseWorker());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenAllocationWhenClosingSharedHandleThenSucceeds) {
    mock->ioctlExpected.total = -1;

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

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenPhysicalOffsetAndNoVmBindWhenCreateGraphicsAllocationFromSharedHandleThenOffsetIsFoldedIntoReportedAddress) {
    mock->ioctlExpected.total = -1;
    this->mock->bindAvailable = false;

    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));
    const uint64_t physicalOffset = MemoryConstants::pageSize;
    this->mock->outputHandle = 2u;
    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    osHandleData.physicalOffset = physicalOffset;
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, {});

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, false, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);

    auto bo = static_cast<DrmAllocation *>(graphicsAllocation)->getBO();
    ASSERT_NE(nullptr, bo);
    EXPECT_EQ(physicalOffset, bo->getPhysicalMemoryOffset());

    auto gmmHelper = device->getGmmHelper();
    EXPECT_EQ(gmmHelper->canonize(bo->peekAddress() + physicalOffset), graphicsAllocation->getGpuAddress());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST2_TEMPLATED_F(DrmMemoryManagerTest, givenUllsLightWhenCreateGraphicsAllocationFromSharedHandleThenAddToResidencyContainer, IsXeLpg) {
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
    }
    memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())[0].osContext->setDirectSubmissionActive();

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
    EXPECT_EQ(0, graphicsAllocation->createInternalHandle(this->memoryManager, 0u, handleVal, nullptr));

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
    EXPECT_EQ(0, graphicsAllocation->createInternalHandle(this->memoryManager, 0u, handleVal, nullptr));

    memoryManager->closeInternalHandle(handleVal, 0u, nullptr);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenAllocationWhenClosingInternalHandleWithNullReservedDataThenSucceeds) {
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
    EXPECT_EQ(0, graphicsAllocation->createInternalHandle(this->memoryManager, 0u, handleVal, nullptr));

    memoryManager->closeInternalHandleWithReservedData(handleVal, 0u, graphicsAllocation, nullptr);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenNoAllocationWhenClosingInternalHandleWithNullReservedDataThenSucceeds) {
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
    EXPECT_EQ(0, graphicsAllocation->createInternalHandle(this->memoryManager, 0u, handleVal, nullptr));

    memoryManager->closeInternalHandleWithReservedData(handleVal, 0u, nullptr, nullptr);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenNullReservedDataWhenClosingInternalHandleWithReservedDataThenSucceeds) {
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
    EXPECT_EQ(0, graphicsAllocation->createInternalHandle(this->memoryManager, 0u, handleVal, nullptr));

    memoryManager->closeInternalHandleWithReservedData(handleVal, 0u, graphicsAllocation, nullptr);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenDrmMemoryManagerWhenClosingInternalHandleWithNullReservedDataThenCloseInternalHandleIsCalled) {
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
    EXPECT_EQ(0, graphicsAllocation->createInternalHandle(this->memoryManager, 0u, handleVal, nullptr));

    // Test that closeInternalHandleWithReservedData calls closeInternalHandle.
    memoryManager->closeInternalHandleWithReservedData(handleVal, 0u, graphicsAllocation, nullptr);

    // If we get here without errors, the test passed
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenSharedHandleAllocationWithoutImageInfoThenGmmIsCacheable) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    osHandle handle = 1u;
    this->mock->outputHandle = 2u;
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::buffer, false, {});
    TestedDrmMemoryManager::OsHandleData osHandleData{handle};

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);

    auto gmm = graphicsAllocation->getDefaultGmm();
    ASSERT_NE(nullptr, gmm);
    auto *gmmResourceParams = reinterpret_cast<GMM_RESCREATE_PARAMS *>(gmm->resourceParamsData.data());
    EXPECT_TRUE(gmmResourceParams->Flags.Info.Cacheable);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenSharedHandleAllocationWithCompressionThenCacheableIsNotOverridden) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    osHandle handle = 1u;
    this->mock->outputHandle = 2u;
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::buffer, false, {});
    properties.flags.preferCompressed = true;
    TestedDrmMemoryManager::OsHandleData osHandleData{handle};

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);

    auto gmm = graphicsAllocation->getDefaultGmm();
    ASSERT_NE(nullptr, gmm);
    auto *gmmResourceParams = reinterpret_cast<GMM_RESCREATE_PARAMS *>(gmm->resourceParamsData.data());

    auto gmmHelper = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getGmmHelper();
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getHelper<ProductHelper>();
    auto gmmResourceUsage = CacheSettingsHelper::getGmmUsageType(AllocationType::buffer, false, productHelper, *gmmHelper->getHardwareInfo());
    auto preferNoCpuAccess = CacheSettingsHelper::preferNoCpuAccess(gmmResourceUsage, *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    bool expectedCacheable = !preferNoCpuAccess && !CacheSettingsHelper::isUncachedType(gmmResourceUsage);
    EXPECT_EQ(expectedCacheable, gmmResourceParams->Flags.Info.Cacheable);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenSharedHandleAllocationWithUncacheableFlagThenGmmUsageTypeIsUncachedAndGmmIsCacheable) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    osHandle handle = 1u;
    this->mock->outputHandle = 2u;
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::buffer, false, {});
    properties.flags.uncacheable = true;
    TestedDrmMemoryManager::OsHandleData osHandleData{handle};

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);

    auto gmm = graphicsAllocation->getDefaultGmm();
    ASSERT_NE(nullptr, gmm);

    auto gmmHelper = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getGmmHelper();
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getHelper<ProductHelper>();
    auto expectedUsageType = CacheSettingsHelper::getGmmUsageType(AllocationType::buffer, true, productHelper, *gmmHelper->getHardwareInfo());
    EXPECT_EQ(expectedUsageType, gmm->getResourceUsageType());
    EXPECT_TRUE(CacheSettingsHelper::isUncachedType(gmm->getResourceUsageType()));

    auto *gmmResourceParams = reinterpret_cast<GMM_RESCREATE_PARAMS *>(gmm->resourceParamsData.data());
    EXPECT_TRUE(gmmResourceParams->Flags.Info.Cacheable);

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

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenLowerBoHandleRegisteredOnHigherRootDeviceIndexWhenTryingToGetSharedOwnershipAndEraseThenRegisteredWrapperIsFound) {
    const int higherBoHandle{5};
    const int lowerBoHandle{3};
    BufferObjectHandleWrapper firstBoHandleWrapper{higherBoHandle, rootDeviceIndex};
    BufferObjectHandleWrapper secondBoHandleWrapper{lowerBoHandle, rootDeviceIndex + 1};

    memoryManager->sharedBoHandles.emplace(std::make_pair(higherBoHandle, rootDeviceIndex), firstBoHandleWrapper.acquireWeakOwnership());
    memoryManager->sharedBoHandles.emplace(std::make_pair(lowerBoHandle, rootDeviceIndex + 1), secondBoHandleWrapper.acquireWeakOwnership());
    ASSERT_EQ(2u, memoryManager->sharedBoHandles.size());

    {
        auto newBoHandleWrapper = memoryManager->tryToGetBoHandleWrapperWithSharedOwnership(lowerBoHandle, rootDeviceIndex + 1);
        EXPECT_EQ(lowerBoHandle, newBoHandleWrapper.getBoHandle());
        EXPECT_FALSE(newBoHandleWrapper.canCloseBoHandle());
    }

    memoryManager->eraseSharedBoHandleWrapper(lowerBoHandle, rootDeviceIndex + 1);
    EXPECT_EQ(1u, memoryManager->sharedBoHandles.size());
    EXPECT_EQ(1u, memoryManager->sharedBoHandles.count(std::make_pair(higherBoHandle, rootDeviceIndex)));
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

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenDeviceSharedAllocationWhenSetCacheRegionFailsThenNullptrIsReturnedAndBoIsNotShared) {
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
    EXPECT_TRUE(memoryManager->sharingBufferObjects.empty());
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

    BufferObject *bo = memoryManager->allocUserptrWithTemporaryGmm(0, (size_t)1024, AllocationType::externalHostPtr, rootDeviceIndex);
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

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledHostMemoryValidationWhenReadOnlyPointerCausesPinningFailWithEfaultThenAllocateMemoryForNonSvmHostPtrReturnsNullptr) {
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

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledHostMemoryValidationWhenHostPtrDoesntCausePinningFailThenAllocateMemoryForNonSvmHostPtrReturnsAllocation) {
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

    EXPECT_TRUE(storage.fragmentStorageData[0].freeTheFragment);
    memoryManager->cleanOsHandles(storage, rootDeviceIndex);
    mock->ioctlResExt = &mock->none;
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenUserptrFailingForSecondNewFragmentWhenPopulatingOsHandlesThenFragmentsCreatedInThatCallAreMarkedToFree) {
    mock->reset();
    DrmMockCustom::IoctlResExt ioctlResExt = {1, -1};
    mock->ioctlResExt = &ioctlResExt;
    mock->ioctlExpected.gemUserptr = 2;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    OsHandleStorage handleStorage;
    for (auto fragmentId = 0u; fragmentId < 3u; fragmentId++) {
        handleStorage.fragmentStorageData[fragmentId].cpuPtr = reinterpret_cast<void *>(0x1000 + fragmentId * MemoryConstants::pageSize);
        handleStorage.fragmentStorageData[fragmentId].fragmentSize = MemoryConstants::pageSize;
    }

    EXPECT_EQ(MemoryManager::AllocationStatus::Error, memoryManager->populateOsHandles(handleStorage, rootDeviceIndex));
    mock->ioctlResExt = &mock->none;
    EXPECT_TRUE(handleStorage.fragmentStorageData[0].freeTheFragment);
    EXPECT_TRUE(handleStorage.fragmentStorageData[1].freeTheFragment);
    EXPECT_EQ(nullptr, handleStorage.fragmentStorageData[2].osHandleStorage);
    EXPECT_EQ(0u, static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager())->getFragmentCount());

    memoryManager->cleanOsHandles(handleStorage, rootDeviceIndex);
    mock->testIoctls();
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

    EXPECT_EQ(ptr, allocation->fragmentsStorage->fragmentStorageData[0].cpuPtr);
    EXPECT_EQ(ptr2, allocation->fragmentsStorage->fragmentStorageData[1].cpuPtr);
    EXPECT_EQ(nullptr, allocation->fragmentsStorage->fragmentStorageData[2].cpuPtr);

    EXPECT_EQ(size, allocation->fragmentsStorage->fragmentStorageData[0].fragmentSize);
    EXPECT_EQ(size * 2, allocation->fragmentsStorage->fragmentStorageData[1].fragmentSize);
    EXPECT_EQ(size * 3, allocation->fragmentsStorage->fragmentStorageData[2].fragmentSize);

    EXPECT_NE(allocation->fragmentsStorage.get(), &handleStorage);
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

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenMemoryManagerWhenAllocateByKmdThenAlignmentIsCorrect) {
    mock->ioctlHelper.reset(new MockIoctlHelper(*mock));
    mock->queryMemoryInfo();

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
    allocationData.alignment = 8 * MemoryConstants::megaByte;
    auto allocation = memoryManager->allocateMemoryByKMD(allocationData);
    auto gmm = allocation->getDefaultGmm();
    auto *gmmResourceParams = reinterpret_cast<GMM_RESCREATE_PARAMS *>(gmm->resourceParamsData.data());
    EXPECT_EQ(gmmResourceParams->BaseAlignment, allocationData.alignment);

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
        auto osHandle = static_cast<OsHandleLinux *>(graphicsAllocation->fragmentsStorage->fragmentStorageData[i].osHandleStorage);
        ASSERT_NE(nullptr, osHandle->bo);
        EXPECT_EQ(reqs.allocationFragments[i].allocationSize, osHandle->bo->peekSize());
        EXPECT_EQ(reqs.allocationFragments[i].allocationPtr, reinterpret_cast<void *>(osHandle->bo->peekAddress()));
    }
    memoryManager->freeGraphicsMemory(graphicsAllocation);

    EXPECT_EQ(0u, hostPtrManager->getFragmentCount());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedForInternal32BitAllocationAndAllocUserptrFailsThenFails) {
    mock->ioctlExpected.gemUserptr = 1;

    this->ioctlResExt = {mock->ioctlCnt.total, -1};
    mock->ioctlResExt = &ioctlResExt;

    auto size = 10u;
    auto allocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, size, nullptr, AllocationType::internalHeap);

    EXPECT_EQ(nullptr, allocation);
    mock->ioctlResExt = &mock->none;
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

    EXPECT_TRUE(imageGraphicsAllocation->getDefaultGmm()->getResourceUsageType() == GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE);

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

    const auto expectedUnmapSize = alignUp(size, memoryManager->getGfxPartition(rootDeviceIndex)->getHeapAllocationAlignment(expectedHeap));
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
        if (i == 2) {
            this->mock->outputHandle = 3u;
        }

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
        if (it != nullptr) {
            memoryManager->freeGraphicsMemory(it);
        }
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenNon32BitAddressingWhenBufferFromSharedHandleIsCreatedAndDRequireBitnessThenItIsNot32BitAllocation) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    SysCalls::lseekCalledCount = 0;

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
    auto testedCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(device->getDefaultEngine().commandStreamReceiver);
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
    auto testedCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(device->getDefaultEngine().commandStreamReceiver);
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

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenDeferBackingDecisionFrozenAtCreationWhenIoctlHelperResultChangesBeforeLockThenResidencyFollowsCreationDecision) {
    mock->ioctlExpected.total = -1;
    this->dontTestIoctlInTearDown = true;

    auto mockIoctlHelper = new MockIoctlHelper(*mock);
    mockIoctlHelper->isDeferBackingEnabledForAllocationResult = true;

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(rootDeviceIndex));
    drm.ioctlHelper.reset(mockIoctlHelper);

    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface.reset(new DrmMemoryOperationsHandlerBind(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex].get(), 0));

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, true, MemoryConstants::pageSize, AllocationType::buffer});
    ASSERT_NE(nullptr, allocation);
    EXPECT_TRUE(static_cast<DrmAllocation *>(allocation)->getBO()->isDeferBackingUsed());

    mockIoctlHelper->isDeferBackingEnabledForAllocationResult = false;

    auto ptr = memoryManager->lockResource(allocation);
    EXPECT_NE(nullptr, ptr);

    auto osContext = device->getDefaultEngine().osContext;
    EXPECT_TRUE(allocation->isAlwaysResident(osContext->getContextId()));

    memoryManager->unlockResource(allocation);
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenDeferBackingNotUsedAtCreationWhenIoctlHelperResultChangesBeforeLockThenAllocationStaysNonResident) {
    mock->ioctlExpected.total = -1;
    this->dontTestIoctlInTearDown = true;

    auto mockIoctlHelper = new MockIoctlHelper(*mock);
    mockIoctlHelper->isDeferBackingEnabledForAllocationResult = false;

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(rootDeviceIndex));
    drm.ioctlHelper.reset(mockIoctlHelper);

    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface.reset(new DrmMemoryOperationsHandlerBind(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex].get(), 0));

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, true, MemoryConstants::pageSize, AllocationType::buffer});
    ASSERT_NE(nullptr, allocation);
    EXPECT_FALSE(static_cast<DrmAllocation *>(allocation)->getBO()->isDeferBackingUsed());

    mockIoctlHelper->isDeferBackingEnabledForAllocationResult = true;

    auto ptr = memoryManager->lockResource(allocation);
    EXPECT_NE(nullptr, ptr);

    auto osContext = device->getDefaultEngine().osContext;
    EXPECT_FALSE(allocation->isAlwaysResident(osContext->getContextId()));

    memoryManager->unlockResource(allocation);
    memoryManager->freeGraphicsMemory(allocation);
}

struct MockIoctlHelperDeferBackingForPreemption : public MockIoctlHelper {
    using MockIoctlHelper::MockIoctlHelper;
    bool isDeferBackingEnabledForAllocation(size_t allocationSize, AllocationType allocationType) const override {
        return allocationType == AllocationType::preemption;
    }
};

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenIoctlHelperDefersOnlyPreemptionWhenAllocatingInLocalMemoryThenDeferBackingFollowsAllocationType) {
    mock->ioctlExpected.total = -1;
    this->dontTestIoctlInTearDown = true;

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(rootDeviceIndex));
    drm.ioctlHelper.reset(new MockIoctlHelperDeferBackingForPreemption(*mock));

    auto preemptionAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, true, MemoryConstants::pageSize64k, AllocationType::preemption});
    ASSERT_NE(nullptr, preemptionAllocation);
    EXPECT_EQ(MemoryPool::localMemory, preemptionAllocation->getMemoryPool());
    EXPECT_TRUE(static_cast<DrmAllocation *>(preemptionAllocation)->getBO()->isDeferBackingUsed());

    auto bufferAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, true, MemoryConstants::pageSize64k, AllocationType::buffer});
    ASSERT_NE(nullptr, bufferAllocation);
    EXPECT_EQ(MemoryPool::localMemory, bufferAllocation->getMemoryPool());
    EXPECT_FALSE(static_cast<DrmAllocation *>(bufferAllocation)->getBO()->isDeferBackingUsed());

    memoryManager->freeGraphicsMemory(preemptionAllocation);
    memoryManager->freeGraphicsMemory(bufferAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenDrmMemoryManagerAndResidentNeededbeforeLockWhenLockIsCalledForDebugAllocationsThenVerifyAllocationIsNotResident) {
    mock->ioctlExpected.total = -1;
    this->dontTestIoctlInTearDown = true;

    auto mockIoctlHelper = new MockIoctlHelper(*mock);
    mockIoctlHelper->isDeferBackingEnabledForAllocationResult = true;

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(rootDeviceIndex));
    drm.ioctlHelper.reset(mockIoctlHelper);

    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface.reset(new DrmMemoryOperationsHandlerBind(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex].get(), 0));

    auto osContext = device->getDefaultEngine().osContext;

    // Test debugContextSaveArea - should NOT be resident
    {
        auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, true, MemoryConstants::pageSize, AllocationType::debugContextSaveArea});
        ASSERT_NE(nullptr, allocation);

        auto ptr = memoryManager->lockResource(allocation);
        EXPECT_NE(nullptr, ptr);

        EXPECT_FALSE(allocation->isAlwaysResident(osContext->getContextId()));

        memoryManager->unlockResource(allocation);
        memoryManager->freeGraphicsMemory(allocation);
    }

    // Test debugSbaTrackingBuffer - should NOT be resident
    {
        auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, true, MemoryConstants::pageSize, AllocationType::debugSbaTrackingBuffer});
        ASSERT_NE(nullptr, allocation);

        auto ptr = memoryManager->lockResource(allocation);
        EXPECT_NE(nullptr, ptr);

        EXPECT_FALSE(allocation->isAlwaysResident(osContext->getContextId()));

        memoryManager->unlockResource(allocation);
        memoryManager->freeGraphicsMemory(allocation);
    }

    // Test debugModuleArea - should NOT be resident
    {
        auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, true, MemoryConstants::pageSize, AllocationType::debugModuleArea});
        ASSERT_NE(nullptr, allocation);

        auto ptr = memoryManager->lockResource(allocation);
        EXPECT_NE(nullptr, ptr);

        EXPECT_FALSE(allocation->isAlwaysResident(osContext->getContextId()));

        memoryManager->unlockResource(allocation);
        memoryManager->freeGraphicsMemory(allocation);
    }

    {
        MockAllocationProperties props{rootDeviceIndex, true, MemoryConstants::pageSize, AllocationType::ringBuffer};
        auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(props);
        ASSERT_NE(nullptr, allocation);

        auto ptr = memoryManager->lockResource(allocation);
        EXPECT_NE(nullptr, ptr);

        EXPECT_FALSE(allocation->isAlwaysResident(osContext->getContextId()));

        memoryManager->unlockResource(allocation);
        memoryManager->freeGraphicsMemory(allocation);
    }
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
    auto gmm = std::make_unique<Gmm>(rootDeviceEnvironment->getGmmHelper(), nullptr, 123, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, StorageInfo{}, gmmRequirements);
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize});
    allocation->setDefaultGmm(std::move(gmm));

    auto mockGmmRes = static_cast<MockGmmResourceInfo *>(allocation->getDefaultGmm()->gmmResourceInfo.get());
    mockGmmRes->setUnifiedAuxTranslationCapable();

    EXPECT_FALSE(memoryManager->mapAuxGpuVA(allocation));

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenSharedAllocationWithSmallerThenRealSizeWhenCreateIsCalledThenRealSizeIsUsed) {
    unsigned int realSize = 64 * MemoryConstants::kiloByte;
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

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDebugSbaTrackingBufferTypeWhenAllocateGraphicsMemoryWithPropertiesThenAllocationHasCorrectOsContext) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    MockOsContextLinux osContext(*mock, rootDeviceIndex, 0u, EngineDescriptorHelper::getDefaultDescriptor());

    AllocationProperties properties{rootDeviceIndex, true, MemoryConstants::pageSize, AllocationType::debugSbaTrackingBuffer, false, mockDeviceBitfield};
    properties.osContext = &osContext;
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(properties));
    ASSERT_NE(nullptr, alloc);
    EXPECT_EQ(&osContext, alloc->getOsContext());
    memoryManager->freeGraphicsMemory(alloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerUSMHostAllocationTests, givenCallToAllocateGraphicsMemoryWithAlignmentWithIsHostUsmAllocationSetToFalseThenNewHostPointerIsUsedAndAllocationIsCreatedSuccessfully) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemClose = 1;

    AllocationData allocationData;
    allocationData.size = 16 * MemoryConstants::kiloByte;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    auto alloc = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);
    EXPECT_NE(nullptr, alloc);
    memoryManager->freeGraphicsMemoryImpl(alloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerUSMHostAllocationTests, givenCallToAllocateGraphicsMemoryWithAlignmentWithIsHostUsmAllocationSetToTrueThenGpuAddressIsNotFromGfxPartition) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemClose = 1;

    AllocationData allocationData;
    allocationData.size = 16 * MemoryConstants::kiloByte;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isUSMHostAllocation = true;
    allocationData.type = AllocationType::svmCpu;
    auto alloc = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);

    EXPECT_NE(nullptr, alloc);
    EXPECT_EQ(reinterpret_cast<uint64_t>(alloc->getUnderlyingBuffer()), alloc->getGpuAddress());

    memoryManager->freeGraphicsMemoryImpl(alloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerUSMHostAllocationTests, givenAllocationBackedBySingleBufferObjectThenNumHandlesIsOne) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemClose = 1;

    AllocationData allocationData;
    allocationData.size = 16 * MemoryConstants::kiloByte;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    auto alloc = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);

    ASSERT_NE(nullptr, alloc);
    EXPECT_EQ(1u, alloc->getNumHandles());

    memoryManager->freeGraphicsMemoryImpl(alloc);
}
