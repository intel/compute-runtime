/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/os_interface/windows/wddm_memory_manager_tests.h"

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_library.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/wddm_residency_controller.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/execution_environment_helper.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_deferred_deleter.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gmm_client_context.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/os_interface/windows/mock_wddm_allocation.h"

using namespace NEO;
using namespace ::testing;

void WddmMemoryManagerFixture::setUp() {
    GdiDllFixture::setUp();

    executionEnvironment = platform()->peekExecutionEnvironment();
    rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex].get();
    wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
    if (defaultHwInfo->capabilityTable.ftrRenderCompressedBuffers || defaultHwInfo->capabilityTable.ftrRenderCompressedImages) {
        GMM_TRANSLATIONTABLE_CALLBACKS dummyTTCallbacks = {};

        auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(*executionEnvironment, 1u, 1));
        auto hwInfo = *defaultHwInfo;
        EngineInstancesContainer regularEngines = {
            {aub_stream::ENGINE_CCS, EngineUsage::Regular}};

        memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[0],
                                                                                                          PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

        for (auto engine : memoryManager->getRegisteredEngines()) {
            if (engine.getEngineUsage() == EngineUsage::Regular) {
                engine.commandStreamReceiver->pageTableManager.reset(GmmPageTableMngr::create(nullptr, 0, &dummyTTCallbacks));
            }
        }
    }
    wddm->init();
    constexpr uint64_t heap32Base = (is32bit) ? 0x1000 : 0x800000000000;
    wddm->setHeap32(heap32Base, 1000 * MemoryConstants::pageSize - 1);

    rootDeviceEnvironment->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);

    memoryManager = std::make_unique<MockWddmMemoryManager>(*executionEnvironment);
}

TEST(ResidencyData, givenNewlyConstructedResidencyDataThenItIsNotResidentOnAnyOsContext) {
    auto maxOsContextCount = 3u;
    ResidencyData residencyData(maxOsContextCount);
    for (auto contextId = 0u; contextId < maxOsContextCount; contextId++) {
        EXPECT_EQ(false, residencyData.resident[contextId]);
    }
}

TEST(WddmMemoryManager, WhenWddmMemoryManagerIsCreatedThenItIsNonCopyable) {
    EXPECT_FALSE(std::is_move_constructible<WddmMemoryManager>::value);
    EXPECT_FALSE(std::is_copy_constructible<WddmMemoryManager>::value);
}

TEST(WddmMemoryManager, WhenWddmMemoryManagerIsCreatedThenItIsNonAssignable) {
    EXPECT_FALSE(std::is_move_assignable<WddmMemoryManager>::value);
    EXPECT_FALSE(std::is_copy_assignable<WddmMemoryManager>::value);
}

TEST(WddmAllocationTest, givenAllocationIsTrimCandidateInOneOsContextWhenGettingTrimCandidatePositionThenReturnItsPositionAndUnusedPositionInOtherContexts) {
    auto executionEnvironment = std::unique_ptr<ExecutionEnvironment>(MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u));
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    MockWddmAllocation allocation(executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper());
    MockOsContext osContext(1u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                             PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));
    allocation.setTrimCandidateListPosition(osContext.getContextId(), 700u);
    EXPECT_EQ(trimListUnusedPosition, allocation.getTrimCandidateListPosition(0u));
    EXPECT_EQ(700u, allocation.getTrimCandidateListPosition(1u));
}

TEST(WddmAllocationTest, givenAllocationCreatedWithOsContextCountOneWhenItIsCreatedThenMaxOsContextCountIsUsedInstead) {
    auto executionEnvironment = std::unique_ptr<ExecutionEnvironment>(MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u));
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    MockWddmAllocation allocation(executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper());
    allocation.setTrimCandidateListPosition(1u, 700u);
    EXPECT_EQ(700u, allocation.getTrimCandidateListPosition(1u));
    EXPECT_EQ(trimListUnusedPosition, allocation.getTrimCandidateListPosition(0u));
}

TEST(WddmAllocationTest, givenRequestedContextIdTooLargeWhenGettingTrimCandidateListPositionThenReturnUnusedPosition) {
    auto executionEnvironment = std::unique_ptr<ExecutionEnvironment>(MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u));
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    MockWddmAllocation allocation(executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper());
    EXPECT_EQ(trimListUnusedPosition, allocation.getTrimCandidateListPosition(1u));
    EXPECT_EQ(trimListUnusedPosition, allocation.getTrimCandidateListPosition(1000u));
}

TEST(WddmAllocationTest, givenAllocationTypeWhenPassedToWddmAllocationConstructorThenAllocationTypeIsStored) {
    WddmAllocation allocation{0, AllocationType::COMMAND_BUFFER, nullptr, 0, 0, nullptr, MemoryPool::MemoryNull, 0u, 1u};
    EXPECT_EQ(AllocationType::COMMAND_BUFFER, allocation.getAllocationType());
}

TEST(WddmAllocationTest, givenMemoryPoolWhenPassedToWddmAllocationConstructorThenMemoryPoolIsStored) {
    WddmAllocation allocation{0, AllocationType::COMMAND_BUFFER, nullptr, 0, 0, nullptr, MemoryPool::System64KBPages, 0u, 1u};
    EXPECT_EQ(MemoryPool::System64KBPages, allocation.getMemoryPool());

    WddmAllocation allocation2{0, AllocationType::COMMAND_BUFFER, nullptr, 0, 0, 0u, MemoryPool::SystemCpuInaccessible, 0u, 1u};
    EXPECT_EQ(MemoryPool::SystemCpuInaccessible, allocation2.getMemoryPool());
}

TEST(WddmMemoryManagerExternalHeapTest, WhenExternalHeapIsCreatedThenItHasCorrectBase) {
    HardwareInfo *hwInfo;
    auto executionEnvironment = getExecutionEnvironmentImpl(hwInfo, 1);
    executionEnvironment->incRefInternal();
    {
        std::unique_ptr<WddmMock> wddm(static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment->rootDeviceEnvironments[0].get())));
        wddm->init();
        uint64_t base = 0x56000;
        uint64_t size = 0x9000;
        wddm->setHeap32(base, size);
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::move(wddm));

        std::unique_ptr<WddmMemoryManager> memoryManager = std::unique_ptr<WddmMemoryManager>(new WddmMemoryManager(*executionEnvironment));

        EXPECT_EQ(base, memoryManager->getExternalHeapBaseAddress(0, false));
    }
    executionEnvironment->decRefInternal();
}

TEST(WddmMemoryManagerWithDeferredDeleterTest, givenWmmWhenAsyncDeleterIsEnabledAndWaitForDeletionsIsCalledThenDeleterInWddmIsSetToNullptr) {
    HardwareInfo *hwInfo;
    auto executionEnvironment = getExecutionEnvironmentImpl(hwInfo, 1);
    executionEnvironment->incRefInternal();
    {
        auto wddm = std::make_unique<WddmMock>(*executionEnvironment->rootDeviceEnvironments[0].get());
        wddm->init();
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::move(wddm));
        bool actualDeleterFlag = DebugManager.flags.EnableDeferredDeleter.get();
        DebugManager.flags.EnableDeferredDeleter.set(true);
        MockWddmMemoryManager memoryManager(*executionEnvironment);
        EXPECT_NE(nullptr, memoryManager.getDeferredDeleter());
        memoryManager.waitForDeletions();
        EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
        DebugManager.flags.EnableDeferredDeleter.set(actualDeleterFlag);
    }
    executionEnvironment->decRefInternal();
}

TEST_F(WddmMemoryManagerSimpleTest, givenAllocateGraphicsMemoryWithPropertiesCalledWithDebugSurfaceTypeThenDebugSurfaceIsCreatedAndZerod) {
    AllocationProperties debugSurfaceProperties{0, true, MemoryConstants::pageSize, AllocationType::DEBUG_CONTEXT_SAVE_AREA, false, false, 0b1011};
    auto debugSurface = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(debugSurfaceProperties));
    EXPECT_NE(nullptr, debugSurface);

    auto mem = debugSurface->getUnderlyingBuffer();
    auto size = debugSurface->getUnderlyingBufferSize();

    EXPECT_TRUE(memoryZeroed(mem, size));
    memoryManager->freeGraphicsMemory(debugSurface);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWhenAllocateGraphicsMemoryIsCalledThenMemoryPoolIsSystem4KBPages) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, *executionEnvironment));
    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());
    EXPECT_EQ(executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->featureTable.flags.ftrLocalMemory,
              allocation->getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly);
    memoryManager->freeGraphicsMemory(allocation);
}

class MockCreateWddmAllocationMemoryManager : public MockWddmMemoryManager {
  public:
    MockCreateWddmAllocationMemoryManager(NEO::ExecutionEnvironment &execEnv) : MockWddmMemoryManager(execEnv) {}
    bool createWddmAllocation(WddmAllocation *allocation, void *requiredGpuPtr) override {
        return false;
    }
};

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWhenAllocateGraphicsMemoryFailedThenNullptrFromAllocateMemoryByKMDIsReturned) {
    memoryManager.reset(new MockCreateWddmAllocationMemoryManager(*executionEnvironment));
    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize;
    auto allocation = memoryManager->allocateMemoryByKMD(allocationData);
    EXPECT_EQ(nullptr, allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWith64KBPagesEnabledWhenAllocateGraphicsMemory64kbIsCalledThenMemoryPoolIsSystem64KBPages) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, *executionEnvironment));
    AllocationData allocationData;
    allocationData.size = 4096u;
    auto allocation = memoryManager->allocateGraphicsMemory64kb(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System64KBPages, allocation->getMemoryPool());

    EXPECT_EQ(executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->featureTable.flags.ftrLocalMemory,
              allocation->getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenAllocationDataWithStorageInfoWhenAllocateGraphicsMemory64kbThenStorageInfoInAllocationIsSetCorrectly) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, *executionEnvironment));
    AllocationData allocationData;
    allocationData.storageInfo = {};
    auto allocation = memoryManager->allocateGraphicsMemory64kb(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_TRUE(memcmp(&allocationData.storageInfo, &allocation->storageInfo, sizeof(StorageInfo)) == 0);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenAllocationDataWithFlagsWhenAllocateGraphicsMemory64kbThenAllocationFlagFlushL3RequiredIsSetCorrectly) {
    class MockGraphicsAllocation : public GraphicsAllocation {
      public:
        using GraphicsAllocation::allocationInfo;
    };
    memoryManager.reset(new MockWddmMemoryManager(false, false, *executionEnvironment));
    AllocationData allocationData;
    allocationData.flags.flushL3 = true;
    auto allocation = static_cast<MockGraphicsAllocation *>(memoryManager->allocateGraphicsMemory64kb(allocationData));
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(allocationData.flags.flushL3, allocation->allocationInfo.flags.flushL3Required);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWhenAllocateGraphicsMemoryWithPtrIsCalledThenMemoryPoolIsSystem4KBPages) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, *executionEnvironment));
    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = 4096u;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, size}, ptr);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());
    for (size_t i = 0; i < allocation->fragmentsStorage.fragmentCount; i++) {
        EXPECT_EQ(1u, static_cast<OsHandleWin *>(allocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage)->gmm->resourceParams.Flags.Info.NonLocalOnly);
    }
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWhenAllocate32BitGraphicsMemoryWithPtrIsCalledThenMemoryPoolIsSystem4KBPagesWith32BitGpuAddressing) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, *executionEnvironment));
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize;

    auto allocation = memoryManager->allocate32BitGraphicsMemory(csr->getRootDeviceIndex(), size, ptr, AllocationType::BUFFER);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPagesWith32BitGpuAddressing, allocation->getMemoryPool());
    EXPECT_EQ(executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->featureTable.flags.ftrLocalMemory,
              allocation->getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWith64KBPagesDisabledWhenAllocateGraphicsMemoryForSVMThen4KBGraphicsAllocationIsReturned) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, *executionEnvironment));
    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    auto size = MemoryConstants::pageSize;

    auto svmAllocation = memoryManager->allocateGraphicsMemoryWithProperties({csr->getRootDeviceIndex(), size, AllocationType::SVM_ZERO_COPY, mockDeviceBitfield});
    EXPECT_NE(nullptr, svmAllocation);
    EXPECT_EQ(MemoryPool::System4KBPages, svmAllocation->getMemoryPool());
    EXPECT_EQ(executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->featureTable.flags.ftrLocalMemory,
              svmAllocation->getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly);
    memoryManager->freeGraphicsMemory(svmAllocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWith64KBPagesEnabledWhenAllocateGraphicsMemoryForSVMThenMemoryPoolIsSystem64KBPages) {
    memoryManager.reset(new MockWddmMemoryManager(true, false, *executionEnvironment));
    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    auto size = MemoryConstants::pageSize;

    auto svmAllocation = memoryManager->allocateGraphicsMemoryWithProperties({csr->getRootDeviceIndex(), size, AllocationType::SVM_ZERO_COPY, mockDeviceBitfield});
    EXPECT_NE(nullptr, svmAllocation);
    EXPECT_EQ(MemoryPool::System64KBPages, svmAllocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(svmAllocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWhenCreateAllocationFromHandleIsCalledThenMemoryPoolIsSystemCpuInaccessible) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, *executionEnvironment));
    auto osHandle = 1u;
    gdi->getQueryResourceInfoArgOut().NumAllocations = 1;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), nullptr, 0, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));

    D3DDDI_OPENALLOCATIONINFO allocationInfo;
    allocationInfo.pPrivateDriverData = gmm->gmmResourceInfo->peekHandle();
    allocationInfo.hAllocation = ALLOCATION_HANDLE;
    allocationInfo.PrivateDriverDataSize = sizeof(GMM_RESOURCE_INFO);

    gdi->getOpenResourceArgOut().pOpenAllocationInfo = &allocationInfo;

    AllocationProperties properties(0, false, 0, AllocationType::SHARED_BUFFER, false, false, 0);
    auto allocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, properties, false, false, true);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::SystemCpuInaccessible, allocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenSharedHandleWhenCreateGraphicsAllocationFromMultipleSharedHandlesIsCalledThenNullptrIsReturned) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, *executionEnvironment));
    auto handle = 1u;
    gdi->getQueryResourceInfoArgOut().NumAllocations = 1;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), nullptr, 0, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));

    D3DDDI_OPENALLOCATIONINFO allocationInfo;
    allocationInfo.pPrivateDriverData = gmm->gmmResourceInfo->peekHandle();
    allocationInfo.hAllocation = ALLOCATION_HANDLE;
    allocationInfo.PrivateDriverDataSize = sizeof(GMM_RESOURCE_INFO);

    gdi->getOpenResourceArgOut().pOpenAllocationInfo = &allocationInfo;

    AllocationProperties properties(0, false, 0, AllocationType::SHARED_BUFFER, false, false, 0);
    std::vector<osHandle> handles{handle};
    auto allocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, true);
    EXPECT_EQ(nullptr, allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenAllocationPropertiesWhenCreateAllocationFromHandleIsCalledThenCorrectAllocationTypeIsSet) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, *executionEnvironment));
    auto osHandle = 1u;
    gdi->getQueryResourceInfoArgOut().NumAllocations = 1;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), nullptr, 0, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));

    D3DDDI_OPENALLOCATIONINFO allocationInfo;
    allocationInfo.pPrivateDriverData = gmm->gmmResourceInfo->peekHandle();
    allocationInfo.hAllocation = ALLOCATION_HANDLE;
    allocationInfo.PrivateDriverDataSize = sizeof(GMM_RESOURCE_INFO);

    gdi->getOpenResourceArgOut().pOpenAllocationInfo = &allocationInfo;

    AllocationProperties propertiesBuffer(0, false, 0, AllocationType::SHARED_BUFFER, false, false, 0);
    AllocationProperties propertiesImage(0, false, 0, AllocationType::SHARED_IMAGE, false, false, 0);

    AllocationProperties *propertiesArray[2] = {&propertiesBuffer, &propertiesImage};

    for (auto properties : propertiesArray) {
        auto allocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, *properties, false, false, true);
        EXPECT_NE(nullptr, allocation);
        EXPECT_EQ(properties->allocationType, allocation->getAllocationType());
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(WddmMemoryManagerSimpleTest, whenCreateAllocationFromHandleAndMapCallFailsThenFreeGraphicsMemoryIsCalled) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, *executionEnvironment));
    auto osHandle = 1u;
    gdi->getQueryResourceInfoArgOut().NumAllocations = 1;
    auto gmm = std::make_unique<Gmm>(rootDeviceEnvironment->getGmmHelper(), nullptr, 0, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, StorageInfo{}, true);

    D3DDDI_OPENALLOCATIONINFO allocationInfo;
    allocationInfo.pPrivateDriverData = gmm->gmmResourceInfo->peekHandle();
    allocationInfo.hAllocation = ALLOCATION_HANDLE;
    allocationInfo.PrivateDriverDataSize = sizeof(GMM_RESOURCE_INFO);
    wddm->mapGpuVaStatus = false;
    wddm->callBaseMapGpuVa = false;

    gdi->getOpenResourceArgOut().pOpenAllocationInfo = &allocationInfo;
    EXPECT_EQ(0u, memoryManager->freeGraphicsMemoryImplCalled);

    AllocationProperties properties(0, false, 0, AllocationType::SHARED_BUFFER, false, false, 0);

    auto allocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, properties, false, false, true);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(1u, memoryManager->freeGraphicsMemoryImplCalled);
}

TEST_F(WddmMemoryManagerSimpleTest, givenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledWhenNotAlignedPtrIsPassedThenAlignedGraphicsAllocationIsCreated) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, *executionEnvironment));
    auto size = 13u;
    auto hostPtr = reinterpret_cast<const void *>(0x10001);

    AllocationData allocationData;
    allocationData.size = size;
    allocationData.hostPtr = hostPtr;
    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(hostPtr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(1u, allocation->getAllocationOffset());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledWhenNotAlignedPtrIsPassedAndImportedAllocationIsFalseThenAlignedGraphicsAllocationIsFreed) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, *executionEnvironment));
    auto size = 13u;
    auto hostPtr = reinterpret_cast<const void *>(0x10001);

    AllocationData allocationData;
    allocationData.size = size;
    allocationData.hostPtr = hostPtr;
    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(hostPtr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(1u, allocation->getAllocationOffset());
    memoryManager->freeGraphicsMemoryImpl(allocation, false);
}

TEST_F(WddmMemoryManagerTest, givenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledWhencreateWddmAllocationFailsThenGraphicsAllocationIsNotCreated) {
    char hostPtr[64];
    memoryManager->setDeferredDeleter(nullptr);
    setMapGpuVaFailConfigFcn(0, 1);

    AllocationData allocationData;
    allocationData.size = sizeof(hostPtr);
    allocationData.hostPtr = hostPtr;
    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);
    EXPECT_EQ(nullptr, allocation);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, GivenShareableEnabledAndSmallSizeWhenAskedToCreateGrahicsAllocationThenValidAllocationIsReturned) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, *executionEnvironment));
    memoryManager->hugeGfxMemoryChunkSize = MemoryConstants::pageSize64k;
    AllocationData allocationData;
    allocationData.size = 4096u;
    allocationData.flags.shareable = true;
    auto allocation = memoryManager->allocateMemoryByKMD(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_FALSE(memoryManager->allocateHugeGraphicsMemoryCalled);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, GivenShareableEnabledAndHugeSizeWhenAskedToCreateGrahicsAllocationThenValidAllocationIsReturned) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, *executionEnvironment));
    memoryManager->hugeGfxMemoryChunkSize = MemoryConstants::pageSize64k;
    AllocationData allocationData;
    allocationData.size = 2ULL * MemoryConstants::pageSize64k;
    allocationData.flags.shareable = true;
    auto allocation = memoryManager->allocateMemoryByKMD(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_TRUE(memoryManager->allocateHugeGraphicsMemoryCalled);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenZeroFenceValueOnSingleEngineRegisteredWhenHandleFenceCompletionIsCalledThenDoNotWaitOnCpu) {
    ASSERT_EQ(1u, memoryManager->getRegisteredEnginesCount());

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties({0, 32, AllocationType::BUFFER, mockDeviceBitfield}));
    allocation->getResidencyData().updateCompletionData(0u, 0u);

    memoryManager->handleFenceCompletion(allocation);
    EXPECT_EQ(0u, wddm->waitFromCpuResult.called);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenNonZeroFenceValueOnSingleEngineRegisteredWhenHandleFenceCompletionIsCalledThenWaitOnCpuOnce) {
    ASSERT_EQ(1u, memoryManager->getRegisteredEnginesCount());

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties({0, 32, AllocationType::BUFFER, mockDeviceBitfield}));
    auto fence = &static_cast<OsContextWin *>(memoryManager->getRegisteredEngines()[0].osContext)->getResidencyController().getMonitoredFence();
    allocation->getResidencyData().updateCompletionData(129u, 0u);

    memoryManager->handleFenceCompletion(allocation);
    EXPECT_EQ(1u, wddm->waitFromCpuResult.called);
    EXPECT_EQ(129u, wddm->waitFromCpuResult.uint64ParamPassed);
    EXPECT_EQ(fence, wddm->waitFromCpuResult.monitoredFence);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenNonZeroFenceValuesOnMultipleEnginesRegisteredWhenHandleFenceCompletionIsCalledThenWaitOnCpuForEachEngine) {
    executionEnvironment->prepareRootDeviceEnvironments(2u);
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }
    const uint32_t rootDeviceIndex = 1;
    DeviceBitfield deviceBitfield(2);
    std::unique_ptr<CommandStreamReceiver> csr(createCommandStream(*executionEnvironment, rootDeviceIndex, deviceBitfield));

    auto wddm2 = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex].get()));
    wddm2->init();
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm2);

    auto hwInfo = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
    OsContext *osContext = memoryManager->createAndRegisterOsContext(csr.get(),
                                                                     EngineDescriptorHelper::getDefaultDescriptor(HwHelper::get(hwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*hwInfo)[1],
                                                                                                                  PreemptionHelper::getDefaultPreemptionMode(*hwInfo), deviceBitfield));
    osContext->ensureContextInitialized();
    ASSERT_EQ(2u, memoryManager->getRegisteredEnginesCount());

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties({0u, 32, AllocationType::BUFFER, mockDeviceBitfield}));
    auto lastEngineFence = &static_cast<OsContextWin *>(osContext)->getResidencyController().getMonitoredFence();
    allocation->getResidencyData().updateCompletionData(129u, 0u);
    allocation->getResidencyData().updateCompletionData(152u, 1u);

    memoryManager->handleFenceCompletion(allocation);
    EXPECT_EQ(1u, wddm->waitFromCpuResult.called);
    EXPECT_EQ(1u, wddm2->waitFromCpuResult.called);
    EXPECT_EQ(129u, wddm->waitFromCpuResult.uint64ParamPassed);
    EXPECT_EQ(152u, wddm2->waitFromCpuResult.uint64ParamPassed);
    EXPECT_EQ(lastEngineFence, wddm2->waitFromCpuResult.monitoredFence);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenNonZeroFenceValueOnSomeOfMultipleEnginesRegisteredWhenHandleFenceCompletionIsCalledThenWaitOnCpuForTheseEngines) {
    const uint32_t rootDeviceIndex = 1;
    executionEnvironment->prepareRootDeviceEnvironments(2u);
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }
    DeviceBitfield deviceBitfield(2);
    std::unique_ptr<CommandStreamReceiver> csr(createCommandStream(*executionEnvironment, rootDeviceIndex, deviceBitfield));

    auto wddm2 = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex].get()));
    wddm2->init();
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm2);

    auto hwInfo = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
    memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(HwHelper::get(hwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*hwInfo)[1],
                                                                                                      PreemptionHelper::getDefaultPreemptionMode(*hwInfo), deviceBitfield));
    ASSERT_EQ(2u, memoryManager->getRegisteredEnginesCount());

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties({0u, 32, AllocationType::BUFFER, mockDeviceBitfield}));
    auto lastEngineFence = &static_cast<OsContextWin *>(memoryManager->getRegisteredEngines()[0].osContext)->getResidencyController().getMonitoredFence();
    allocation->getResidencyData().updateCompletionData(129u, 0u);
    allocation->getResidencyData().updateCompletionData(0, 1u);

    memoryManager->handleFenceCompletion(allocation);
    EXPECT_EQ(1u, wddm->waitFromCpuResult.called);
    EXPECT_EQ(129u, wddm->waitFromCpuResult.uint64ParamPassed);
    EXPECT_EQ(lastEngineFence, wddm->waitFromCpuResult.monitoredFence);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenWddmMemoryManagerWhenGpuAddressIsReservedAndFreedThenAddressRangeIsZero) {
    auto addressRange = memoryManager->reserveGpuAddress(MemoryConstants::pageSize, 0);
    auto gmmHelper = memoryManager->getGmmHelper(0);

    EXPECT_EQ(0u, gmmHelper->decanonize(addressRange.address));
    EXPECT_EQ(0u, addressRange.size);

    memoryManager->freeGpuAddress(addressRange, 0);
}

TEST_F(WddmMemoryManagerSimpleTest, givenWddmMemoryManagerWhenAllocatingWithGpuVaThenNullptrIsReturned) {
    AllocationData allocationData;

    allocationData.size = 0x1000;
    allocationData.gpuAddress = 0x2000;
    allocationData.osContext = osContext;

    auto allocation = memoryManager->allocateGraphicsMemoryWithGpuVa(allocationData);
    EXPECT_EQ(nullptr, allocation);
}

TEST_F(WddmMemoryManagerTest, GivenGraphicsAllocationWhenAddAndRemoveAllocationToHostPtrManagerThenFragmentHasCorrectValues) {
    void *cpuPtr = (void *)0x30000;
    size_t size = 0x1000;
    uint64_t gpuPtr = 0x123;

    MockWddmAllocation gfxAllocation(rootDeviceEnvironment->getGmmHelper());
    HostPtrEntryKey key{cpuPtr, gfxAllocation.getRootDeviceIndex()};
    gfxAllocation.cpuPtr = cpuPtr;
    gfxAllocation.size = size;
    gfxAllocation.gpuPtr = gpuPtr;
    memoryManager->addAllocationToHostPtrManager(&gfxAllocation);
    auto fragment = memoryManager->getHostPtrManager()->getFragment(key);
    EXPECT_NE(fragment, nullptr);
    EXPECT_TRUE(fragment->driverAllocation);
    EXPECT_EQ(fragment->refCount, 1);
    EXPECT_EQ(fragment->fragmentCpuPointer, cpuPtr);
    EXPECT_EQ(fragment->fragmentSize, size);
    EXPECT_NE(fragment->osInternalStorage, nullptr);
    auto osHandle = static_cast<OsHandleWin *>(fragment->osInternalStorage);
    EXPECT_EQ(osHandle->gmm, gfxAllocation.getDefaultGmm());
    EXPECT_EQ(osHandle->gpuPtr, gpuPtr);
    EXPECT_EQ(osHandle->handle, gfxAllocation.handle);
    EXPECT_NE(fragment->residency, nullptr);

    FragmentStorage fragmentStorage = {};
    fragmentStorage.fragmentCpuPointer = cpuPtr;
    memoryManager->getHostPtrManager()->storeFragment(gfxAllocation.getRootDeviceIndex(), fragmentStorage);
    fragment = memoryManager->getHostPtrManager()->getFragment(key);
    EXPECT_EQ(fragment->refCount, 2);

    fragment->driverAllocation = false;
    memoryManager->removeAllocationFromHostPtrManager(&gfxAllocation);
    fragment = memoryManager->getHostPtrManager()->getFragment(key);
    EXPECT_EQ(fragment->refCount, 2);
    fragment->driverAllocation = true;

    memoryManager->removeAllocationFromHostPtrManager(&gfxAllocation);
    fragment = memoryManager->getHostPtrManager()->getFragment(key);
    EXPECT_EQ(fragment->refCount, 1);

    memoryManager->removeAllocationFromHostPtrManager(&gfxAllocation);
    fragment = memoryManager->getHostPtrManager()->getFragment(key);
    EXPECT_EQ(fragment, nullptr);
}

TEST_F(WddmMemoryManagerTest, WhenAllocatingGpuMemHostPtrThenCpuPtrAndGpuPtrAreSame) {
    // three pages
    void *ptr = alignedMalloc(3 * 4096, 4096);
    ASSERT_NE(nullptr, ptr);

    auto *gpuAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, false, MemoryConstants::pageSize}, ptr);
    // Should be same cpu ptr and gpu ptr
    EXPECT_EQ(ptr, gpuAllocation->getUnderlyingBuffer());

    memoryManager->freeGraphicsMemory(gpuAllocation);
    alignedFree(ptr);
}

TEST_F(WddmMemoryManagerTest, givenDefaultMemoryManagerWhenAllocateWithSizeIsCalledThenSharedHandleIsZero) {
    auto *gpuAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize});

    auto wddmAllocation = static_cast<WddmAllocation *>(gpuAllocation);

    EXPECT_EQ(0u, wddmAllocation->peekSharedHandle());

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenCreateFromSharedHandleIsCalledThenNonNullGraphicsAllocationIsReturned) {
    auto osHandle = 1u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), pSysMem, 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    AllocationProperties properties(0, false, 4096u, AllocationType::SHARED_BUFFER, false, false, mockDeviceBitfield);

    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, properties, false, false, true);
    auto wddmAlloc = static_cast<WddmAllocation *>(gpuAllocation);
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(RESOURCE_HANDLE, wddmAlloc->resourceHandle);
    EXPECT_EQ(ALLOCATION_HANDLE, wddmAlloc->getDefaultHandle());

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerSimpleTest, whenAllocationCreatedFromSharedHandleIsDestroyedThenDestroyAllocationFromGdiIsNotInvoked) {
    gdi->getQueryResourceInfoArgOut().NumAllocations = 1;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), nullptr, 0, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));

    D3DDDI_OPENALLOCATIONINFO allocationInfo;
    allocationInfo.pPrivateDriverData = gmm->gmmResourceInfo->peekHandle();
    allocationInfo.hAllocation = ALLOCATION_HANDLE;
    allocationInfo.PrivateDriverDataSize = sizeof(GMM_RESOURCE_INFO);

    gdi->getOpenResourceArgOut().pOpenAllocationInfo = &allocationInfo;

    AllocationProperties properties(0, false, 0, AllocationType::SHARED_BUFFER, false, false, 0);

    auto allocation = memoryManager->createGraphicsAllocationFromSharedHandle(1, properties, false, false, true);
    EXPECT_NE(nullptr, allocation);

    memoryManager->setDeferredDeleter(nullptr);
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(1u, memoryManager->freeGraphicsMemoryImplCalled);

    gdi->getDestroyArg().AllocationCount = 7;
    auto destroyArg = gdi->getDestroyArg();
    EXPECT_EQ(7, destroyArg.AllocationCount);
    gdi->getDestroyArg().AllocationCount = 0;
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenCreateFromNTHandleIsCalledThenNonNullGraphicsAllocationIsReturned) {
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), pSysMem, 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromNTHandle(reinterpret_cast<void *>(1), 0, AllocationType::SHARED_IMAGE);
    auto wddmAlloc = static_cast<WddmAllocation *>(gpuAllocation);
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(NT_RESOURCE_HANDLE, wddmAlloc->resourceHandle);
    EXPECT_EQ(NT_ALLOCATION_HANDLE, wddmAlloc->getDefaultHandle());
    EXPECT_EQ(AllocationType::SHARED_IMAGE, wddmAlloc->getAllocationType());

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenCreateFromNTHandleIsCalledWithUmKmDataTranslatorEnabledThenNonNullGraphicsAllocationIsReturned) {
    struct MockUmKmDataTranslator : UmKmDataTranslator {
        using UmKmDataTranslator::isEnabled;
    };

    std::unique_ptr<MockUmKmDataTranslator> translator = std::make_unique<MockUmKmDataTranslator>();
    translator->isEnabled = true;
    std::unique_ptr<NEO::HwDeviceIdWddm> hwDeviceId = std::make_unique<NEO::HwDeviceIdWddm>(wddm->hwDeviceId->getAdapter(),
                                                                                            wddm->hwDeviceId->getAdapterLuid(),
                                                                                            executionEnvironment->osEnvironment.get(),
                                                                                            std::move(translator));
    wddm->hwDeviceId.reset(hwDeviceId.release());

    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), pSysMem, 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromNTHandle(reinterpret_cast<void *>(1), 0, AllocationType::SHARED_IMAGE);
    auto wddmAlloc = static_cast<WddmAllocation *>(gpuAllocation);
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(NT_RESOURCE_HANDLE, wddmAlloc->resourceHandle);
    EXPECT_EQ(NT_ALLOCATION_HANDLE, wddmAlloc->getDefaultHandle());
    EXPECT_EQ(AllocationType::SHARED_IMAGE, wddmAlloc->getAllocationType());

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenLockUnlockIsCalledThenReturnPtr) {
    auto alloc = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize});

    auto ptr = memoryManager->lockResource(alloc);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(1u, wddm->lockResult.called);
    EXPECT_TRUE(wddm->lockResult.success);

    memoryManager->unlockResource(alloc);
    EXPECT_EQ(1u, wddm->unlockResult.called);
    EXPECT_TRUE(wddm->unlockResult.success);

    memoryManager->freeGraphicsMemory(alloc);
}

TEST_F(WddmMemoryManagerTest, GivenForce32bitAddressingAndRequireSpecificBitnessWhenCreatingAllocationFromSharedHandleThen32BitAllocationIsReturned) {
    auto osHandle = 1u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), pSysMem, 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    memoryManager->setForce32BitAllocations(true);

    AllocationProperties properties(0, false, 4096u, AllocationType::SHARED_BUFFER, false, false, 0);

    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, properties, true, false, true);
    ASSERT_NE(nullptr, gpuAllocation);
    if constexpr (is64bit) {
        EXPECT_TRUE(gpuAllocation->is32BitAllocation());

        uint64_t base = memoryManager->getExternalHeapBaseAddress(gpuAllocation->getRootDeviceIndex(), gpuAllocation->isAllocatedInLocalMemoryPool());
        auto gmmHelper = memoryManager->getGmmHelper(gpuAllocation->getRootDeviceIndex());

        EXPECT_EQ(gmmHelper->canonize(base), gpuAllocation->getGpuBaseAddress());
    }

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, GivenForce32bitAddressingAndNotRequiredSpecificBitnessWhenCreatingAllocationFromSharedHandleThenNon32BitAllocationIsReturned) {
    auto osHandle = 1u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), pSysMem, 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    memoryManager->setForce32BitAllocations(true);

    AllocationProperties properties(0, false, 4096u, AllocationType::SHARED_BUFFER, false, false, 0);
    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, properties, false, false, true);
    ASSERT_NE(nullptr, gpuAllocation);

    EXPECT_FALSE(gpuAllocation->is32BitAllocation());
    if constexpr (is64bit) {
        uint64_t base = 0;
        EXPECT_EQ(base, gpuAllocation->getGpuBaseAddress());
    }

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenFreeAllocFromSharedHandleIsCalledThenDestroyResourceHandle) {
    auto osHandle = 1u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), pSysMem, 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    AllocationProperties properties(0, false, 4096u, AllocationType::SHARED_BUFFER, false, false, 0);
    auto gpuAllocation = (WddmAllocation *)memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, properties, false, false, true);
    EXPECT_NE(nullptr, gpuAllocation);
    auto expectedDestroyHandle = gpuAllocation->resourceHandle;
    EXPECT_NE(0u, expectedDestroyHandle);

    auto lastDestroyed = getMockLastDestroyedResHandleFcn();
    EXPECT_EQ(0u, lastDestroyed);

    memoryManager->freeGraphicsMemory(gpuAllocation);
    lastDestroyed = getMockLastDestroyedResHandleFcn();
    EXPECT_EQ(lastDestroyed, expectedDestroyHandle);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenAllocFromHostPtrIsCalledThenResourceHandleIsNotCreated) {
    auto size = 13u;
    auto hostPtr = reinterpret_cast<const void *>(0x10001);

    AllocationData allocationData{};
    allocationData.size = size;
    allocationData.hostPtr = hostPtr;
    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));

    EXPECT_EQ(0u, allocation->resourceHandle);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerSizeZeroWhenCreateFromSharedHandleIsCalledThenUpdateSize) {
    auto osHandle = 1u;
    auto size = 4096u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), pSysMem, size, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    AllocationProperties properties(0, false, size, AllocationType::SHARED_BUFFER, false, false, 0);
    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, properties, false, false, true);
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(size, gpuAllocation->getUnderlyingBufferSize());
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenAllocateGraphicsMemoryWithSetAllocattionPropertisWithAllocationTypeBufferCompressedIsCalledThenIsRendeCompressedTrueAndGpuMappingIsSetWithGoodAddressRange) {
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize;
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    rootDeviceEnvironment->setHwInfo(&hwInfo);

    auto memoryManager = std::make_unique<MockWddmMemoryManager>(true, false, *executionEnvironment);
    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    rootDeviceEnvironment->executionEnvironment.initializeMemoryManager();
    memoryManager->allocateGraphicsMemoryInNonDevicePool = true;

    MockAllocationProperties properties = {mockRootDeviceIndex, true, size, AllocationType::BUFFER, mockDeviceBitfield};
    properties.flags.preferCompressed = true;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, ptr);

    auto gfxPartition = memoryManager->getGfxPartition(mockRootDeviceIndex);
    D3DGPU_VIRTUAL_ADDRESS standard64kbRangeMinimumAddress = gfxPartition->getHeapMinimalAddress(HeapIndex::HEAP_STANDARD64KB);
    D3DGPU_VIRTUAL_ADDRESS standard64kbRangeMaximumAddress = gfxPartition->getHeapLimit(HeapIndex::HEAP_STANDARD64KB);

    ASSERT_NE(nullptr, allocation);
    EXPECT_TRUE(memoryManager->allocationGraphicsMemory64kbCreated);
    EXPECT_TRUE(allocation->getDefaultGmm()->isCompressionEnabled);
    if ((is32bit || rootDeviceEnvironment->isFullRangeSvm()) &&
        allocation->getDefaultGmm()->gmmResourceInfo->is64KBPageSuitable()) {
        auto gmmHelper = memoryManager->getGmmHelper(0);

        EXPECT_GE(gmmHelper->decanonize(allocation->getGpuAddress()), standard64kbRangeMinimumAddress);
        EXPECT_LE(gmmHelper->decanonize(allocation->getGpuAddress()), standard64kbRangeMaximumAddress);
    }

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_F(WddmMemoryManagerTest, givenInternalHeapOrLinearStreamTypeWhenAllocatingThenSetCorrectUsage) {
    auto memoryManager = std::make_unique<MockWddmMemoryManager>(true, false, *executionEnvironment);

    rootDeviceEnvironment->executionEnvironment.initializeMemoryManager();

    {
        MockAllocationProperties properties = {mockRootDeviceIndex, true, 1, AllocationType::INTERNAL_HEAP, mockDeviceBitfield};

        auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, nullptr);

        ASSERT_NE(nullptr, allocation);

        EXPECT_TRUE(allocation->getDefaultGmm()->resourceParams.Usage == GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER);

        memoryManager->freeGraphicsMemory(allocation);
    }

    {
        MockAllocationProperties properties = {mockRootDeviceIndex, true, 1, AllocationType::LINEAR_STREAM, mockDeviceBitfield};

        auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, nullptr);

        ASSERT_NE(nullptr, allocation);

        EXPECT_TRUE(allocation->getDefaultGmm()->resourceParams.Usage == GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER);

        memoryManager->freeGraphicsMemory(allocation);
    }
}

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenAllocateGraphicsMemoryWithSetAllocattionPropertisWithAllocationTypeBufferIsCalledThenIsRendeCompressedFalseAndCorrectAddressRange) {
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize;
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    rootDeviceEnvironment->setHwInfo(&hwInfo);

    auto memoryManager = std::make_unique<MockWddmMemoryManager>(false, false, *executionEnvironment);
    memoryManager->allocateGraphicsMemoryInNonDevicePool = true;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{mockRootDeviceIndex, true, size, AllocationType::BUFFER, mockDeviceBitfield}, ptr);

    auto gfxPartition = memoryManager->getGfxPartition(mockRootDeviceIndex);
    D3DGPU_VIRTUAL_ADDRESS svmRangeMinimumAddress = gfxPartition->getHeapMinimalAddress(HeapIndex::HEAP_SVM);
    D3DGPU_VIRTUAL_ADDRESS svmRangeMaximumAddress = gfxPartition->getHeapLimit(HeapIndex::HEAP_SVM);

    ASSERT_NE(nullptr, allocation);
    EXPECT_FALSE(memoryManager->allocationGraphicsMemory64kbCreated);
    EXPECT_FALSE(allocation->getDefaultGmm()->isCompressionEnabled);
    if (is32bit || rootDeviceEnvironment->isFullRangeSvm()) {
        auto gmmHelper = memoryManager->getGmmHelper(0);

        EXPECT_GE(gmmHelper->decanonize(allocation->getGpuAddress()), svmRangeMinimumAddress);
        EXPECT_LE(gmmHelper->decanonize(allocation->getGpuAddress()), svmRangeMaximumAddress);
    }
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenCreateFromSharedHandleFailsThenReturnNull) {
    auto osHandle = 1u;
    auto size = 4096u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), pSysMem, size, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    wddm->failOpenSharedHandle = true;

    AllocationProperties properties(0, false, size, AllocationType::SHARED_BUFFER, false, false, 0);
    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, properties, false, false, true);
    EXPECT_EQ(nullptr, gpuAllocation);
}

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenTiledImageWithMipCountZeroIsBeingCreatedThenallocateGraphicsMemoryForImageIsUsed) {
    if (!UnitTestHelper<FamilyType>::tiledImagesSupported) {
        GTEST_SKIP();
    }
    MockContext context;
    context.memoryManager = memoryManager.get();

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 64u;
    imageDesc.image_height = 64u;

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    std::unique_ptr<Image> dstImage(
        Image::create(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                      flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);

    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex());
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_EQ(GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE, imageGraphicsAllocation->getDefaultGmm()->resourceParams.Usage);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenTiledImageWithMipCountNonZeroIsBeingCreatedThenallocateGraphicsMemoryForImageIsUsed) {
    MockContext context;
    context.memoryManager = memoryManager.get();

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 64u;
    imageDesc.image_height = 64u;
    imageDesc.num_mip_levels = 1u;

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    std::unique_ptr<Image> dstImage(
        Image::create(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                      flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);
    EXPECT_EQ(static_cast<int>(imageDesc.num_mip_levels), dstImage->peekMipCount());

    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex());
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_EQ(GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE, imageGraphicsAllocation->getDefaultGmm()->resourceParams.Usage);
}

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenTiledImageIsBeingCreatedFromHostPtrThenallocateGraphicsMemoryForImageIsUsed) {
    if (!UnitTestHelper<FamilyType>::tiledImagesSupported) {
        GTEST_SKIP();
    }

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0u));
    MockContext context(device.get());
    context.memoryManager = memoryManager.get();

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 64u;
    imageDesc.image_height = 64u;

    char data[64u * 64u * 4 * 8];

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    std::unique_ptr<Image> dstImage(
        Image::create(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                      flags, 0, surfaceFormat, &imageDesc, data, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);

    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex());
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_EQ(GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE, imageGraphicsAllocation->getDefaultGmm()->resourceParams.Usage);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenNonTiledImgWithMipCountNonZeroisBeingCreatedThenAllocateGraphicsMemoryForImageIsUsed) {
    MockContext context;
    context.memoryManager = memoryManager.get();

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 64u;
    imageDesc.num_mip_levels = 1u;

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    std::unique_ptr<Image> dstImage(
        Image::create(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                      flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);
    EXPECT_EQ(static_cast<int>(imageDesc.num_mip_levels), dstImage->peekMipCount());

    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex());
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_EQ(GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE, imageGraphicsAllocation->getDefaultGmm()->resourceParams.Usage);
}

TEST_F(WddmMemoryManagerTest, GivenOffsetsWhenAllocatingGpuMemHostThenAllocatedOnlyIfInBounds) {
    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    MockWddmAllocation alloc(rootDeviceEnvironment->getGmmHelper()), allocOffseted(rootDeviceEnvironment->getGmmHelper());
    // three pages
    void *ptr = alignedMalloc(4 * 4096, 4096);
    ASSERT_NE(nullptr, ptr);

    size_t baseOffset = 1024;
    // misaligned buffer spanning across 3 pages
    auto *gpuAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, false, 2 * MemoryConstants::pageSize}, (char *)ptr + baseOffset);
    // Should be same cpu ptr and gpu ptr
    EXPECT_EQ((char *)ptr + baseOffset, gpuAllocation->getUnderlyingBuffer());

    auto hostPtrManager = memoryManager->getHostPtrManager();

    auto fragment = hostPtrManager->getFragment({ptr, rootDeviceIndex});
    ASSERT_NE(nullptr, fragment);
    EXPECT_TRUE(fragment->refCount == 1);
    EXPECT_NE(fragment->osInternalStorage, nullptr);

    // offset by 3 pages, not in boundary
    auto fragment2 = hostPtrManager->getFragment({reinterpret_cast<char *>(ptr) + 3 * 4096, rootDeviceIndex});

    EXPECT_EQ(nullptr, fragment2);

    // offset by one page, still in boundary
    void *offsetPtr = ptrOffset(ptr, 4096);
    auto *gpuAllocation2 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, false, MemoryConstants::pageSize}, offsetPtr);
    // Should be same cpu ptr and gpu ptr
    EXPECT_EQ(offsetPtr, gpuAllocation2->getUnderlyingBuffer());

    auto fragment3 = hostPtrManager->getFragment({offsetPtr, rootDeviceIndex});
    ASSERT_NE(nullptr, fragment3);

    EXPECT_TRUE(fragment3->refCount == 2);
    EXPECT_EQ(alloc.handle, allocOffseted.handle);
    EXPECT_EQ(alloc.getUnderlyingBufferSize(), allocOffseted.getUnderlyingBufferSize());
    EXPECT_EQ(alloc.getAlignedCpuPtr(), allocOffseted.getAlignedCpuPtr());

    memoryManager->freeGraphicsMemory(gpuAllocation2);

    auto fragment4 = hostPtrManager->getFragment({ptr, rootDeviceIndex});
    ASSERT_NE(nullptr, fragment4);

    EXPECT_TRUE(fragment4->refCount == 1);

    memoryManager->freeGraphicsMemory(gpuAllocation);

    fragment4 = hostPtrManager->getFragment({ptr, rootDeviceIndex});
    EXPECT_EQ(nullptr, fragment4);

    alignedFree(ptr);
}

TEST_F(WddmMemoryManagerTest, WhenAllocatingGpuMemThenOsInternalStorageIsPopulatedCorrectly) {
    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper());
    // three pages
    void *ptr = reinterpret_cast<void *>(0x200000);
    auto *gpuAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, false, 3 * MemoryConstants::pageSize}, ptr);
    // Should be same cpu ptr and gpu ptr
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(ptr, gpuAllocation->getUnderlyingBuffer());

    auto fragment = memoryManager->getHostPtrManager()->getFragment({ptr, rootDeviceIndex});
    ASSERT_NE(nullptr, fragment);
    EXPECT_TRUE(fragment->refCount == 1);
    EXPECT_NE(static_cast<OsHandleWin *>(fragment->osInternalStorage)->handle, 0);
    EXPECT_NE(static_cast<OsHandleWin *>(fragment->osInternalStorage)->gmm, nullptr);
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, GivenAlignedPointerWhenAllocate32BitMemoryThenGmmCalledWithCorrectPointerAndSize) {
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper());
    uint32_t size = 4096;
    void *ptr = reinterpret_cast<void *>(4096);
    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, size, ptr, AllocationType::BUFFER);
    EXPECT_EQ(ptr, reinterpret_cast<void *>(gpuAllocation->getDefaultGmm()->resourceParams.pExistingSysMem));
    EXPECT_EQ(size, gpuAllocation->getDefaultGmm()->resourceParams.ExistingSysMemSize);
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, GivenUnAlignedPointerAndSizeWhenAllocate32BitMemoryThenGmmCalledWithCorrectPointerAndSize) {
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper());
    uint32_t size = 0x1001;
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, size, ptr, AllocationType::BUFFER);
    EXPECT_EQ(reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(gpuAllocation->getDefaultGmm()->resourceParams.pExistingSysMem));
    EXPECT_EQ(0x2000u, gpuAllocation->getDefaultGmm()->resourceParams.ExistingSysMemSize);
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, WhenInitializingWddmThenSystemSharedMemoryIsCorrect) {
    executionEnvironment->prepareRootDeviceEnvironments(4u);
    for (auto i = 0u; i < 4u; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->osInterface.reset();
        auto mockWddm = Wddm::createWddm(nullptr, *executionEnvironment->rootDeviceEnvironments[i].get());
        mockWddm->init();

        int64_t mem = memoryManager->getSystemSharedMemory(i);
        EXPECT_EQ(mem, 4249540608);
    }
}

TEST_F(WddmMemoryManagerTest, GivenBitnessWhenGettingMaxAddressThenCorrectAddressIsReturned) {
    uint64_t maxAddr = memoryManager->getMaxApplicationAddress();
    if (is32bit) {
        EXPECT_EQ(maxAddr, MemoryConstants::max32BitAppAddress);
    } else {
        EXPECT_EQ(maxAddr, MemoryConstants::max64BitAppAddress);
    }
}

TEST_F(WddmMemoryManagerTest, GivenNullptrWhenAllocating32BitMemoryThenAddressIsCorrect) {
    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, 3 * MemoryConstants::pageSize, nullptr, AllocationType::BUFFER);

    ASSERT_NE(nullptr, gpuAllocation);

    auto gmmHelper = memoryManager->getGmmHelper(gpuAllocation->getRootDeviceIndex());
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_EXTERNAL)), gpuAllocation->getGpuAddress());
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::HEAP_EXTERNAL)), gpuAllocation->getGpuAddress() + gpuAllocation->getUnderlyingBufferSize());

    EXPECT_EQ(0u, gpuAllocation->fragmentsStorage.fragmentCount);
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, given32BitAllocationWhenItIsCreatedThenItHasNonZeroGpuAddressToPatch) {
    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, 3 * MemoryConstants::pageSize, nullptr, AllocationType::BUFFER);

    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_NE(0llu, gpuAllocation->getGpuAddressToPatch());

    auto gmmHelper = memoryManager->getGmmHelper(gpuAllocation->getRootDeviceIndex());
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_EXTERNAL)), gpuAllocation->getGpuAddress());
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::HEAP_EXTERNAL)), gpuAllocation->getGpuAddress() + gpuAllocation->getUnderlyingBufferSize());
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, GivenMisalignedHostPtrWhenAllocating32BitMemoryThenTripleAllocationDoesNotOccur) {
    size_t misalignedSize = 0x2500;
    void *misalignedPtr = reinterpret_cast<void *>(0x12500);

    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, misalignedSize, misalignedPtr, AllocationType::BUFFER);

    ASSERT_NE(nullptr, gpuAllocation);

    EXPECT_EQ(alignSizeWholePage(misalignedPtr, misalignedSize), gpuAllocation->getUnderlyingBufferSize());

    auto gmmHelper = memoryManager->getGmmHelper(gpuAllocation->getRootDeviceIndex());
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::HEAP_EXTERNAL)), gpuAllocation->getGpuAddress());
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::HEAP_EXTERNAL)), gpuAllocation->getGpuAddress() + gpuAllocation->getUnderlyingBufferSize());

    EXPECT_EQ(0u, gpuAllocation->fragmentsStorage.fragmentCount);

    void *alignedPtr = alignDown(misalignedPtr, MemoryConstants::allocationAlignment);
    uint64_t offset = ptrDiff(misalignedPtr, alignedPtr);

    EXPECT_EQ(offset, gpuAllocation->getAllocationOffset());
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, WhenAllocating32BitMemoryThenGpuBaseAddressIsCannonized) {
    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, 3 * MemoryConstants::pageSize, nullptr, AllocationType::BUFFER);

    ASSERT_NE(nullptr, gpuAllocation);

    auto gmmHelper = memoryManager->getGmmHelper(gpuAllocation->getRootDeviceIndex());
    uint64_t cannonizedAddress = gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapBase(MemoryManager::selectExternalHeap(gpuAllocation->isAllocatedInLocalMemoryPool())));
    EXPECT_EQ(cannonizedAddress, gpuAllocation->getGpuBaseAddress());

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, GivenThreeOsHandlesWhenAskedForDestroyAllocationsThenAllMarkedAllocationsAreDestroyed) {
    OsHandleStorage storage;
    void *pSysMem = reinterpret_cast<void *>(0x1000);
    uint32_t maxOsContextCount = 1u;

    auto osHandle0 = new OsHandleWin();
    auto osHandle1 = new OsHandleWin();
    auto osHandle2 = new OsHandleWin();

    storage.fragmentStorageData[0].osHandleStorage = osHandle0;
    storage.fragmentStorageData[0].residency = new ResidencyData(maxOsContextCount);

    osHandle0->handle = ALLOCATION_HANDLE;
    storage.fragmentStorageData[0].freeTheFragment = true;
    osHandle0->gmm = new Gmm(rootDeviceEnvironment->getGmmHelper(), pSysMem, 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);

    storage.fragmentStorageData[1].osHandleStorage = osHandle1;
    osHandle1->handle = ALLOCATION_HANDLE;
    storage.fragmentStorageData[1].residency = new ResidencyData(maxOsContextCount);

    storage.fragmentStorageData[1].freeTheFragment = false;

    storage.fragmentStorageData[2].osHandleStorage = osHandle2;
    osHandle2->handle = ALLOCATION_HANDLE;
    storage.fragmentStorageData[2].freeTheFragment = true;
    osHandle2->gmm = new Gmm(rootDeviceEnvironment->getGmmHelper(), pSysMem, 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    storage.fragmentStorageData[2].residency = new ResidencyData(maxOsContextCount);

    memoryManager->cleanOsHandles(storage, 0);

    auto destroyWithResourceHandleCalled = 0u;
    D3DKMT_DESTROYALLOCATION2 *ptrToDestroyAlloc2 = nullptr;

    getSizesFcn(destroyWithResourceHandleCalled, ptrToDestroyAlloc2);

    EXPECT_EQ(0u, ptrToDestroyAlloc2->Flags.SynchronousDestroy);
    EXPECT_EQ(1u, ptrToDestroyAlloc2->Flags.AssumeNotInUse);

    EXPECT_EQ(ALLOCATION_HANDLE, osHandle1->handle);

    delete storage.fragmentStorageData[1].osHandleStorage;
    delete storage.fragmentStorageData[1].residency;
}

TEST_F(WddmMemoryManagerTest, GivenNullptrWhenFreeingAllocationThenCrashDoesNotOccur) {
    EXPECT_NO_THROW(memoryManager->freeGraphicsMemory(nullptr));
}

TEST_F(WddmMemoryManagerTest, givenDefaultWddmMemoryManagerWhenAskedForAlignedMallocRestrictionsThenValueIsReturned) {
    AlignedMallocRestrictions *mallocRestrictions = memoryManager->getAlignedMallocRestrictions();
    ASSERT_NE(nullptr, mallocRestrictions);
    EXPECT_EQ(NEO::windowsMinAddress, mallocRestrictions->minAddress);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenCpuMemNotMeetRestrictionsThenReserveMemRangeForMap) {
    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    void *cpuPtr = reinterpret_cast<void *>(memoryManager->getAlignedMallocRestrictions()->minAddress - 0x1000);
    size_t size = 0x1000;

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{0, false, size}, cpuPtr));

    void *expectReserve = reinterpret_cast<void *>(wddm->virtualAllocAddress);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(expectReserve, allocation->getReservedAddressPtr());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerTest, givenManagerWithDisabledDeferredDeleterWhenMapGpuVaFailThenFailToCreateAllocation) {
    void *ptr = reinterpret_cast<void *>(0x1000);
    size_t size = 0x1000;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), ptr, size, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));

    memoryManager->setDeferredDeleter(nullptr);
    setMapGpuVaFailConfigFcn(0, 1);

    auto gmmHelper = rootDeviceEnvironment->getGmmHelper();
    auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(ptr)));
    WddmAllocation allocation(0, AllocationType::BUFFER, ptr, canonizedAddress, size, nullptr, MemoryPool::MemoryNull, 0u, 1u);
    allocation.setDefaultGmm(gmm.get());
    bool ret = memoryManager->createWddmAllocation(&allocation, allocation.getAlignedCpuPtr());
    EXPECT_FALSE(ret);
}

TEST_F(WddmMemoryManagerTest, givenManagerWithEnabledDeferredDeleterWhenFirstMapGpuVaFailSecondAfterDrainSuccessThenCreateAllocation) {
    void *ptr = reinterpret_cast<void *>(0x10000);
    size_t size = 0x1000;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), ptr, size, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));

    MockDeferredDeleter *deleter = new MockDeferredDeleter;
    memoryManager->setDeferredDeleter(deleter);

    setMapGpuVaFailConfigFcn(0, 1);

    auto gmmHelper = rootDeviceEnvironment->getGmmHelper();
    auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(ptr)));
    WddmAllocation allocation(0, AllocationType::BUFFER, ptr, canonizedAddress, size, nullptr, MemoryPool::MemoryNull, 0u, 1u);
    allocation.setDefaultGmm(gmm.get());
    bool ret = memoryManager->createWddmAllocation(&allocation, allocation.getAlignedCpuPtr());
    EXPECT_TRUE(ret);
}

TEST_F(WddmMemoryManagerTest, givenManagerWithEnabledDeferredDeleterWhenFirstAndMapGpuVaFailSecondAfterDrainFailThenFailToCreateAllocation) {
    void *ptr = reinterpret_cast<void *>(0x1000);
    size_t size = 0x1000;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), ptr, size, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));

    MockDeferredDeleter *deleter = new MockDeferredDeleter;
    memoryManager->setDeferredDeleter(deleter);

    setMapGpuVaFailConfigFcn(0, 2);

    auto gmmHelper = rootDeviceEnvironment->getGmmHelper();
    auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(ptr)));
    WddmAllocation allocation(0, AllocationType::BUFFER, ptr, canonizedAddress, size, nullptr, MemoryPool::MemoryNull, 0u, 1u);
    allocation.setDefaultGmm(gmm.get());
    bool ret = memoryManager->createWddmAllocation(&allocation, allocation.getAlignedCpuPtr());
    EXPECT_FALSE(ret);
}

TEST_F(WddmMemoryManagerTest, givenNullPtrAndSizePassedToCreateInternalAllocationWhenCallIsMadeThenAllocationIsCreatedIn32BitHeapInternal) {
    auto wddmAllocation = static_cast<WddmAllocation *>(memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, MemoryConstants::pageSize, nullptr, AllocationType::INTERNAL_HEAP));
    ASSERT_NE(nullptr, wddmAllocation);
    auto gmmHelper = memoryManager->getGmmHelper(wddmAllocation->getRootDeviceIndex());
    EXPECT_EQ(wddmAllocation->getGpuBaseAddress(), gmmHelper->canonize(memoryManager->getInternalHeapBaseAddress(wddmAllocation->getRootDeviceIndex(), wddmAllocation->isAllocatedInLocalMemoryPool())));
    EXPECT_NE(nullptr, wddmAllocation->getUnderlyingBuffer());
    EXPECT_EQ(4096u, wddmAllocation->getUnderlyingBufferSize());
    EXPECT_NE((uint64_t)wddmAllocation->getUnderlyingBuffer(), wddmAllocation->getGpuAddress());
    auto cannonizedHeapBase = gmmHelper->canonize(memoryManager->getInternalHeapBaseAddress(rootDeviceIndex, wddmAllocation->isAllocatedInLocalMemoryPool()));
    auto cannonizedHeapEnd = gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(MemoryManager::selectInternalHeap(wddmAllocation->isAllocatedInLocalMemoryPool())));

    EXPECT_GT(wddmAllocation->getGpuAddress(), cannonizedHeapBase);
    EXPECT_LT(wddmAllocation->getGpuAddress() + wddmAllocation->getUnderlyingBufferSize(), cannonizedHeapEnd);

    EXPECT_NE(nullptr, wddmAllocation->getDriverAllocatedCpuPtr());
    EXPECT_TRUE(wddmAllocation->is32BitAllocation());
    memoryManager->freeGraphicsMemory(wddmAllocation);
}

TEST_F(WddmMemoryManagerTest, givenPtrAndSizePassedToCreateInternalAllocationWhenCallIsMadeThenAllocationIsCreatedIn32BitHeapInternal) {
    auto ptr = reinterpret_cast<void *>(0x1000000);
    auto wddmAllocation = static_cast<WddmAllocation *>(memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, MemoryConstants::pageSize, ptr, AllocationType::INTERNAL_HEAP));
    ASSERT_NE(nullptr, wddmAllocation);
    auto gmmHelper = memoryManager->getGmmHelper(wddmAllocation->getRootDeviceIndex());
    EXPECT_EQ(wddmAllocation->getGpuBaseAddress(), gmmHelper->canonize(memoryManager->getInternalHeapBaseAddress(rootDeviceIndex, wddmAllocation->isAllocatedInLocalMemoryPool())));
    EXPECT_EQ(ptr, wddmAllocation->getUnderlyingBuffer());
    EXPECT_EQ(4096u, wddmAllocation->getUnderlyingBufferSize());
    EXPECT_NE((uint64_t)wddmAllocation->getUnderlyingBuffer(), wddmAllocation->getGpuAddress());

    auto cannonizedHeapBase = gmmHelper->canonize(memoryManager->getInternalHeapBaseAddress(rootDeviceIndex, wddmAllocation->isAllocatedInLocalMemoryPool()));
    auto cannonizedHeapEnd = gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(MemoryManager::selectInternalHeap(wddmAllocation->isAllocatedInLocalMemoryPool())));
    EXPECT_GT(wddmAllocation->getGpuAddress(), cannonizedHeapBase);
    EXPECT_LT(wddmAllocation->getGpuAddress() + wddmAllocation->getUnderlyingBufferSize(), cannonizedHeapEnd);

    EXPECT_EQ(nullptr, wddmAllocation->getDriverAllocatedCpuPtr());
    EXPECT_TRUE(wddmAllocation->is32BitAllocation());
    memoryManager->freeGraphicsMemory(wddmAllocation);
}

TEST_F(BufferWithWddmMemory, WhenCreatingBufferThenBufferIsCreatedCorrectly) {
    flags = CL_MEM_USE_HOST_PTR | CL_MEM_FORCE_HOST_MEMORY_INTEL;

    auto ptr = alignedMalloc(MemoryConstants::preferredAlignment, MemoryConstants::preferredAlignment);

    auto buffer = Buffer::create(
        &context,
        flags,
        MemoryConstants::preferredAlignment,
        ptr,
        retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, buffer);

    auto address = buffer->getCpuAddress();
    if (buffer->isMemObjZeroCopy()) {
        EXPECT_EQ(ptr, address);
    } else {
        EXPECT_NE(address, ptr);
    }
    EXPECT_NE(nullptr, buffer->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex()));
    EXPECT_NE(nullptr, buffer->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex())->getUnderlyingBuffer());

    delete buffer;
    alignedFree(ptr);
}

TEST_F(BufferWithWddmMemory, GivenNullOsHandleStorageWhenPopulatingThenFilledPointerIsReturned) {
    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    storage.fragmentStorageData[0].fragmentSize = MemoryConstants::pageSize;
    memoryManager->populateOsHandles(storage, 0);
    EXPECT_NE(nullptr, storage.fragmentStorageData[0].osHandleStorage);
    EXPECT_NE(nullptr, static_cast<OsHandleWin *>(storage.fragmentStorageData[0].osHandleStorage)->gmm);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[2].osHandleStorage);
    storage.fragmentStorageData[0].freeTheFragment = true;
    memoryManager->cleanOsHandles(storage, 0);
}

TEST_F(BufferWithWddmMemory, GivenMisalignedHostPtrAndMultiplePagesSizeWhenAskedForGraphicsAllocationThenItContainsAllFragmentsWithProperGpuAdrresses) {
    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    auto ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1001);
    auto size = MemoryConstants::pageSize * 10;
    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{context.getDevice(0)->getRootDeviceIndex(), false, size, context.getDevice(0)->getDeviceBitfield()}, ptr);

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());

    ASSERT_EQ(3u, hostPtrManager->getFragmentCount());

    auto reqs = MockHostPtrManager::getAllocationRequirements(context.getDevice(0)->getRootDeviceIndex(), ptr, size);

    for (int i = 0; i < maxFragmentsCount; i++) {
        auto osHandle = static_cast<OsHandleWin *>(graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage);
        EXPECT_NE((D3DKMT_HANDLE) nullptr, osHandle->handle);

        EXPECT_NE(nullptr, osHandle->gmm);
        EXPECT_EQ(reqs.allocationFragments[i].allocationPtr,
                  reinterpret_cast<void *>(osHandle->gmm->resourceParams.pExistingSysMem));
        EXPECT_EQ(reqs.allocationFragments[i].allocationSize,
                  osHandle->gmm->resourceParams.BaseWidth);
    }
    memoryManager->freeGraphicsMemory(graphicsAllocation);
    EXPECT_EQ(0u, hostPtrManager->getFragmentCount());
}

TEST_F(BufferWithWddmMemory, GivenPointerAndSizeWhenAskedToCreateGrahicsAllocationThenGraphicsAllocationIsCreated) {
    OsHandleStorage handleStorage;

    auto ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1000);
    auto ptr2 = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1001);
    auto size = MemoryConstants::pageSize;

    handleStorage.fragmentStorageData[0].cpuPtr = ptr;
    handleStorage.fragmentStorageData[1].cpuPtr = ptr2;
    handleStorage.fragmentStorageData[2].cpuPtr = nullptr;

    handleStorage.fragmentStorageData[0].fragmentSize = size;
    handleStorage.fragmentStorageData[1].fragmentSize = size * 2;
    handleStorage.fragmentStorageData[2].fragmentSize = size * 3;

    AllocationData allocationData;
    allocationData.size = size;
    allocationData.hostPtr = ptr;
    auto allocation = memoryManager->createGraphicsAllocation(handleStorage, allocationData);

    EXPECT_EQ(ptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());

    EXPECT_EQ(ptr, allocation->fragmentsStorage.fragmentStorageData[0].cpuPtr);
    EXPECT_EQ(ptr2, allocation->fragmentsStorage.fragmentStorageData[1].cpuPtr);
    EXPECT_EQ(nullptr, allocation->fragmentsStorage.fragmentStorageData[2].cpuPtr);

    EXPECT_EQ(size, allocation->fragmentsStorage.fragmentStorageData[0].fragmentSize);
    EXPECT_EQ(size * 2, allocation->fragmentsStorage.fragmentStorageData[1].fragmentSize);
    EXPECT_EQ(size * 3, allocation->fragmentsStorage.fragmentStorageData[2].fragmentSize);

    EXPECT_NE(&allocation->fragmentsStorage, &handleStorage);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(BufferWithWddmMemory, givenFragmentsThatAreNotInOrderWhenGraphicsAllocationIsBeingCreatedThenGraphicsAddressIsPopulatedFromProperFragment) {
    memoryManager->setForce32bitAllocations(true);
    OsHandleStorage handleStorage = {};
    D3DGPU_VIRTUAL_ADDRESS gpuAdress = MemoryConstants::pageSize * 1;
    auto ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + MemoryConstants::pageSize);
    auto size = MemoryConstants::pageSize * 2;
    auto maxOsContextCount = 1u;

    auto osHandle = new OsHandleWin();

    handleStorage.fragmentStorageData[0].cpuPtr = ptr;
    handleStorage.fragmentStorageData[0].fragmentSize = size;
    handleStorage.fragmentStorageData[0].osHandleStorage = osHandle;
    handleStorage.fragmentStorageData[0].residency = new ResidencyData(maxOsContextCount);
    handleStorage.fragmentStorageData[0].freeTheFragment = true;
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    osHandle->gmm = new Gmm(rootDeviceEnvironment->getGmmHelper(), ptr, size, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    handleStorage.fragmentCount = 1;

    FragmentStorage fragment = {};
    fragment.driverAllocation = true;
    fragment.fragmentCpuPointer = ptr;
    fragment.fragmentSize = size;
    fragment.osInternalStorage = handleStorage.fragmentStorageData[0].osHandleStorage;
    osHandle->gpuPtr = gpuAdress;
    memoryManager->getHostPtrManager()->storeFragment(rootDeviceIndex, fragment);

    AllocationData allocationData;
    allocationData.size = size;
    allocationData.hostPtr = ptr;
    auto allocation = memoryManager->createGraphicsAllocation(handleStorage, allocationData);
    EXPECT_EQ(ptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(gpuAdress, allocation->getGpuAddress());
    EXPECT_EQ(0ULL, allocation->getAllocationOffset());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(BufferWithWddmMemory, givenFragmentsThatAreNotInOrderWhenGraphicsAllocationIsBeingCreatedNotAllignedToPageThenGraphicsAddressIsPopulatedFromProperFragmentAndOffsetisAssigned) {
    memoryManager->setForce32bitAllocations(true);
    OsHandleStorage handleStorage = {};
    D3DGPU_VIRTUAL_ADDRESS gpuAdress = MemoryConstants::pageSize * 1;
    auto ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + MemoryConstants::pageSize);
    auto size = MemoryConstants::pageSize * 2;
    auto maxOsContextCount = 1u;

    auto osHandle = new OsHandleWin();

    handleStorage.fragmentStorageData[0].cpuPtr = ptr;
    handleStorage.fragmentStorageData[0].fragmentSize = size;
    handleStorage.fragmentStorageData[0].osHandleStorage = osHandle;
    handleStorage.fragmentStorageData[0].residency = new ResidencyData(maxOsContextCount);
    handleStorage.fragmentStorageData[0].freeTheFragment = true;
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    osHandle->gmm = new Gmm(rootDeviceEnvironment->getGmmHelper(), ptr, size, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    handleStorage.fragmentCount = 1;

    FragmentStorage fragment = {};
    fragment.driverAllocation = true;
    fragment.fragmentCpuPointer = ptr;
    fragment.fragmentSize = size;
    fragment.osInternalStorage = handleStorage.fragmentStorageData[0].osHandleStorage;
    osHandle->gpuPtr = gpuAdress;
    memoryManager->getHostPtrManager()->storeFragment(rootDeviceIndex, fragment);

    auto offset = 80u;
    auto allocationPtr = ptrOffset(ptr, offset);
    AllocationData allocationData;
    allocationData.size = size;
    allocationData.hostPtr = allocationPtr;
    auto allocation = memoryManager->createGraphicsAllocation(handleStorage, allocationData);

    EXPECT_EQ(allocationPtr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(gpuAdress + offset, allocation->getGpuAddress()); // getGpuAddress returns gpuAddress + allocationOffset
    EXPECT_EQ(offset, allocation->getAllocationOffset());
    memoryManager->freeGraphicsMemory(allocation);
}

struct WddmMemoryManagerWithAsyncDeleterTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = getExecutionEnvironmentImpl(hwInfo, 1);
        executionEnvironment->incRefInternal();
        wddm = static_cast<WddmMock *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Wddm>());
        wddm->resetGdi(new MockGdi());
        wddm->callBaseDestroyAllocations = false;
        wddm->init();
        deleter = new MockDeferredDeleter;
        memoryManager = std::make_unique<MockWddmMemoryManager>(*executionEnvironment);
        memoryManager->setDeferredDeleter(deleter);
    }

    void TearDown() override {
        executionEnvironment->decRefInternal();
    }

    MockDeferredDeleter *deleter = nullptr;
    std::unique_ptr<MockWddmMemoryManager> memoryManager;
    ExecutionEnvironment *executionEnvironment;
    HardwareInfo *hwInfo;
    WddmMock *wddm;
};

TEST_F(WddmMemoryManagerWithAsyncDeleterTest, givenWddmWhenAsyncDeleterIsEnabledThenCanDeferDeletions) {
    EXPECT_EQ(0, deleter->deferDeletionCalled);
    memoryManager->tryDeferDeletions(nullptr, 0, 0, 0);
    EXPECT_EQ(1, deleter->deferDeletionCalled);
    EXPECT_EQ(1u, wddm->destroyAllocationResult.called);
}

TEST_F(WddmMemoryManagerWithAsyncDeleterTest, givenWddmWhenAsyncDeleterIsDisabledThenCannotDeferDeletions) {
    memoryManager->setDeferredDeleter(nullptr);
    memoryManager->tryDeferDeletions(nullptr, 0, 0, 0);
    EXPECT_EQ(1u, wddm->destroyAllocationResult.called);
}

TEST_F(WddmMemoryManagerWithAsyncDeleterTest, givenMemoryManagerWithAsyncDeleterWhenCannotAllocateMemoryForTiledImageThenDrainIsCalledAndCreateAllocationIsCalledTwice) {
    UltDeviceFactory deviceFactory{1, 0};
    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::Image3D;
    ImageInfo imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    wddm->createAllocationStatus = STATUS_GRAPHICS_NO_VIDEO_MEMORY;
    EXPECT_EQ(0, deleter->drainCalled);
    EXPECT_EQ(0u, wddm->createAllocationResult.called);
    deleter->expectDrainBlockingValue(true);
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, 0, deviceFactory.rootDevices[0]);
    AllocationProperties allocProperties = MemObjHelper::getAllocationPropertiesWithImageInfo(0, imgInfo, true, memoryProperties, *hwInfo, mockDeviceBitfield, true);

    memoryManager->allocateGraphicsMemoryInPreferredPool(allocProperties, nullptr);
    EXPECT_EQ(1, deleter->drainCalled);
    EXPECT_EQ(2u, wddm->createAllocationResult.called);
}

TEST_F(WddmMemoryManagerWithAsyncDeleterTest, givenMemoryManagerWithAsyncDeleterWhenCanAllocateMemoryForTiledImageThenDrainIsNotCalledAndCreateAllocationIsCalledOnce) {
    UltDeviceFactory deviceFactory{1, 0};
    ImageDescriptor imgDesc;
    imgDesc.imageType = ImageType::Image3D;
    ImageInfo imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    wddm->createAllocationStatus = STATUS_SUCCESS;
    wddm->mapGpuVaStatus = true;
    wddm->callBaseMapGpuVa = false;
    EXPECT_EQ(0, deleter->drainCalled);
    EXPECT_EQ(0u, wddm->createAllocationResult.called);
    EXPECT_EQ(0u, wddm->mapGpuVirtualAddressResult.called);

    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, 0, deviceFactory.rootDevices[0]);
    AllocationProperties allocProperties = MemObjHelper::getAllocationPropertiesWithImageInfo(0, imgInfo, true, memoryProperties, *hwInfo, mockDeviceBitfield, true);

    auto allocation = memoryManager->allocateGraphicsMemoryInPreferredPool(allocProperties, nullptr);
    EXPECT_EQ(0, deleter->drainCalled);
    EXPECT_EQ(1u, wddm->createAllocationResult.called);
    EXPECT_EQ(1u, wddm->mapGpuVirtualAddressResult.called);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerWithAsyncDeleterTest, givenMemoryManagerWithoutAsyncDeleterWhenCannotAllocateMemoryForTiledImageThenCreateAllocationIsCalledOnce) {
    UltDeviceFactory deviceFactory{1, 0};
    memoryManager->setDeferredDeleter(nullptr);
    ImageDescriptor imgDesc;
    imgDesc.imageType = ImageType::Image3D;
    ImageInfo imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    wddm->createAllocationStatus = STATUS_GRAPHICS_NO_VIDEO_MEMORY;
    EXPECT_EQ(0u, wddm->createAllocationResult.called);

    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, 0, deviceFactory.rootDevices[0]);
    AllocationProperties allocProperties = MemObjHelper::getAllocationPropertiesWithImageInfo(0, imgInfo, true, memoryProperties, *hwInfo, mockDeviceBitfield, true);

    memoryManager->allocateGraphicsMemoryInPreferredPool(allocProperties, nullptr);
    EXPECT_EQ(1u, wddm->createAllocationResult.called);
}

TEST(WddmMemoryManagerDefaults, givenDefaultWddmMemoryManagerWhenItIsQueriedForInternalHeapBaseThenHeapInternalBaseIsReturned) {
    HardwareInfo *hwInfo;
    auto executionEnvironment = getExecutionEnvironmentImpl(hwInfo, 1);
    executionEnvironment->incRefInternal();
    {
        auto wddm = new WddmMock(*executionEnvironment->rootDeviceEnvironments[0].get());
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
        executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
        wddm->init();
        MockWddmMemoryManager memoryManager(*executionEnvironment);
        auto heapBase = wddm->getGfxPartition().Heap32[static_cast<uint32_t>(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY)].Base;
        heapBase = std::max(heapBase, static_cast<uint64_t>(wddm->getWddmMinAddress()));
        EXPECT_EQ(heapBase, memoryManager.getInternalHeapBaseAddress(0, true));
    }
    executionEnvironment->decRefInternal();
}

TEST_F(MockWddmMemoryManagerTest, givenValidateAllocationFunctionWhenItIsCalledWithTripleAllocationThenSuccessIsReturned) {
    wddm->init();
    MockWddmMemoryManager memoryManager(*executionEnvironment);

    auto wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, false, MemoryConstants::pageSize}, reinterpret_cast<void *>(0x1000)));

    EXPECT_TRUE(memoryManager.validateAllocationMock(wddmAlloc));

    memoryManager.freeGraphicsMemory(wddmAlloc);
}

TEST_F(MockWddmMemoryManagerTest, givenCreateOrReleaseDeviceSpecificMemResourcesWhenCreatingMemoryManagerObjectThenTheseMethodsAreEmpty) {
    wddm->init();
    MockWddmMemoryManager memoryManager(*executionEnvironment);
    memoryManager.createDeviceSpecificMemResources(1);
    memoryManager.releaseDeviceSpecificMemResources(1);
}

TEST_F(MockWddmMemoryManagerTest, givenWddmMemoryManagerWhenVerifySharedHandleThenVerifySharedHandleIsCalled) {
    wddm->init();
    MockWddmMemoryManager memoryManager(*executionEnvironment);
    osHandle handle = 1;
    memoryManager.verifyHandle(handle, 0, false);
    EXPECT_EQ(0, wddm->counterVerifyNTHandle);
    EXPECT_EQ(1, wddm->counterVerifySharedHandle);
}

TEST_F(MockWddmMemoryManagerTest, givenWddmMemoryManagerWhenVerifyNTHandleThenVerifyNTHandleIsCalled) {
    wddm->init();
    MockWddmMemoryManager memoryManager(*executionEnvironment);
    osHandle handle = 1;
    memoryManager.verifyHandle(handle, 0, true);
    EXPECT_EQ(1, wddm->counterVerifyNTHandle);
    EXPECT_EQ(0, wddm->counterVerifySharedHandle);
}

TEST_F(MockWddmMemoryManagerTest, givenWddmMemoryManagerWhenIsNTHandleisCalledThenVerifyNTHandleisCalled) {
    wddm->init();
    MockWddmMemoryManager memoryManager(*executionEnvironment);
    osHandle handle = 1;
    memoryManager.isNTHandle(handle, 0);
    EXPECT_EQ(1, wddm->counterVerifyNTHandle);
    EXPECT_EQ(0, wddm->counterVerifySharedHandle);
}

TEST_F(MockWddmMemoryManagerTest, givenEnabled64kbpagesWhenCreatingGraphicsMemoryForBufferWithoutHostPtrThen64kbAdressIsAllocated) {
    DebugManagerStateRestore dbgRestore;
    wddm->init();
    DebugManager.flags.Enable64kbpages.set(true);
    MemoryManagerCreate<WddmMemoryManager> memoryManager64k(true, false, *executionEnvironment);
    if (memoryManager64k.isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    EXPECT_EQ(0U, wddm->createAllocationResult.called);

    GraphicsAllocation *galloc = memoryManager64k.allocateGraphicsMemoryWithProperties({rootDeviceIndex, MemoryConstants::pageSize64k, AllocationType::BUFFER_HOST_MEMORY, mockDeviceBitfield});
    EXPECT_NE(0U, wddm->createAllocationResult.called);
    EXPECT_NE(nullptr, galloc);
    EXPECT_EQ(true, galloc->isLocked());
    EXPECT_NE(nullptr, galloc->getUnderlyingBuffer());
    EXPECT_EQ(0u, (uintptr_t)galloc->getUnderlyingBuffer() % MemoryConstants::pageSize64k);
    EXPECT_EQ(0u, (uintptr_t)galloc->getGpuAddress() % MemoryConstants::pageSize64k);
    memoryManager64k.freeGraphicsMemory(galloc);
}

TEST_F(OsAgnosticMemoryManagerUsingWddmTest, givenEnabled64kbPagesWhenAllocationIsCreatedWithSizeSmallerThan64kbThenGraphicsAllocationsHas64kbAlignedUnderlyingSize) {
    DebugManagerStateRestore dbgRestore;
    wddm->init();
    DebugManager.flags.Enable64kbpages.set(true);
    MockWddmMemoryManager memoryManager(true, false, *executionEnvironment);
    AllocationData allocationData;
    allocationData.size = 1u;
    auto graphicsAllocation = memoryManager.allocateGraphicsMemory64kb(allocationData);

    EXPECT_NE(nullptr, graphicsAllocation);
    EXPECT_EQ(MemoryConstants::pageSize64k, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_NE(0llu, graphicsAllocation->getGpuAddress());
    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(1u, graphicsAllocation->getDefaultGmm()->resourceParams.Flags.Info.Cacheable);

    memoryManager.freeGraphicsMemory(graphicsAllocation);
}

TEST_F(MockWddmMemoryManagerTest, givenWddmWhenallocateGraphicsMemory64kbThenLockResultAndmapGpuVirtualAddressIsCalled) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.Enable64kbpages.set(true);
    wddm->init();
    MockWddmMemoryManager memoryManager64k(*executionEnvironment);
    uint32_t lockCount = wddm->lockResult.called;
    uint32_t mapGpuVirtualAddressResult = wddm->mapGpuVirtualAddressResult.called;
    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;
    GraphicsAllocation *galloc = memoryManager64k.allocateGraphicsMemory64kb(allocationData);
    EXPECT_EQ(lockCount + 1, wddm->lockResult.called);
    EXPECT_EQ(mapGpuVirtualAddressResult + 1, wddm->mapGpuVirtualAddressResult.called);

    if (is32bit || executionEnvironment->rootDeviceEnvironments[0]->isFullRangeSvm()) {
        EXPECT_NE(nullptr, wddm->mapGpuVirtualAddressResult.cpuPtrPassed);
    } else {
        EXPECT_EQ(nullptr, wddm->mapGpuVirtualAddressResult.cpuPtrPassed);
    }
    memoryManager64k.freeGraphicsMemory(galloc);
}

TEST_F(MockWddmMemoryManagerTest, givenAllocateGraphicsMemoryForBufferAndRequestedSizeIsHugeThenResultAllocationIsSplitted) {
    DebugManagerStateRestore dbgRestore;

    wddm->init();
    wddm->mapGpuVaStatus = true;
    VariableBackup<bool> restorer{&wddm->callBaseMapGpuVa, false};

    for (bool enable64KBpages : {true, false}) {
        wddm->createAllocationResult.called = 0U;
        DebugManager.flags.Enable64kbpages.set(enable64KBpages);
        MemoryManagerCreate<MockWddmMemoryManager> memoryManager(true, false, *executionEnvironment);
        if (memoryManager.isLimitedGPU(0)) {
            GTEST_SKIP();
        }
        EXPECT_EQ(0, wddm->createAllocationResult.called);

        memoryManager.hugeGfxMemoryChunkSize = MemoryConstants::pageSize64k - MemoryConstants::pageSize;

        WddmAllocation *wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemoryWithProperties({rootDeviceIndex, MemoryConstants::pageSize64k * 3, AllocationType::BUFFER, mockDeviceBitfield}));
        EXPECT_NE(nullptr, wddmAlloc);
        EXPECT_EQ(4, wddmAlloc->getNumGmms());
        EXPECT_EQ(4, wddm->createAllocationResult.called);

        auto gmmHelper = executionEnvironment->rootDeviceEnvironments[wddmAlloc->getRootDeviceIndex()]->getGmmHelper();
        EXPECT_EQ(wddmAlloc->getGpuAddressToModify(), gmmHelper->canonize(wddmAlloc->reservedGpuVirtualAddress));

        memoryManager.freeGraphicsMemory(wddmAlloc);
    }
}

TEST_F(MockWddmMemoryManagerTest, givenDefaultMemoryManagerWhenItIsCreatedThenCorrectHugeGfxMemoryChunkIsSet) {
    MockWddmMemoryManager memoryManager(*executionEnvironment);
    EXPECT_EQ(memoryManager.getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod::AllocateByKmd), 4 * MemoryConstants::gigaByte - MemoryConstants::pageSize64k);
    EXPECT_EQ(memoryManager.getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod::UseUmdSystemPtr), 4 * MemoryConstants::gigaByte - MemoryConstants::pageSize64k);
}

TEST_F(MockWddmMemoryManagerTest, givenAllocateGraphicsMemoryForHostBufferAndRequestedSizeIsHugeThenResultAllocationIsSplitted) {
    DebugManagerStateRestore dbgRestore;

    wddm->init();
    wddm->mapGpuVaStatus = true;
    VariableBackup<bool> restorer{&wddm->callBaseMapGpuVa, false};

    DebugManager.flags.Enable64kbpages.set(true);
    MemoryManagerCreate<MockWddmMemoryManager> memoryManager(true, false, *executionEnvironment);
    if (memoryManager.isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    EXPECT_EQ(0, wddm->createAllocationResult.called);

    memoryManager.hugeGfxMemoryChunkSize = MemoryConstants::pageSize64k - MemoryConstants::pageSize;

    std::vector<uint8_t> hostPtr(MemoryConstants::pageSize64k * 3);
    AllocationProperties allocProps{rootDeviceIndex, MemoryConstants::pageSize64k * 3, AllocationType::BUFFER_HOST_MEMORY, mockDeviceBitfield};
    allocProps.flags.allocateMemory = false;
    WddmAllocation *wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemoryWithProperties(allocProps, hostPtr.data()));

    EXPECT_NE(nullptr, wddmAlloc);
    EXPECT_EQ(4, wddmAlloc->getNumGmms());
    EXPECT_EQ(4, wddm->createAllocationResult.called);

    auto gmmHelper = memoryManager.getGmmHelper(wddmAlloc->getRootDeviceIndex());
    EXPECT_EQ(wddmAlloc->getGpuAddressToModify(), gmmHelper->canonize(wddmAlloc->reservedGpuVirtualAddress));

    memoryManager.freeGraphicsMemory(wddmAlloc);
}

TEST_F(MockWddmMemoryManagerTest, givenDefaultMemoryManagerWhenItIsCreatedThenAsyncDeleterEnabledIsTrue) {
    wddm->init();
    WddmMemoryManager memoryManager(*executionEnvironment);
    EXPECT_TRUE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_NE(nullptr, memoryManager.getDeferredDeleter());
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWithNoRegisteredOsContextsWhenCallingIsMemoryBudgetExhaustedThenReturnFalse) {
    EXPECT_FALSE(memoryManager->isMemoryBudgetExhausted());
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerAnd32bitBuildThenSvmPartitionIsAlwaysInitialized) {
    if (is32bit) {
        EXPECT_EQ(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::HEAP_SVM), MemoryConstants::max32BitAddress);
    }
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWithRegisteredOsContextWhenCallingIsMemoryBudgetExhaustedThenReturnFalse) {
    executionEnvironment->prepareRootDeviceEnvironments(3u);
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }
    executionEnvironment->initializeMemoryManager();
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->osInterface.reset();
        auto wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment->rootDeviceEnvironments[i].get()));
        wddm->init();
        executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
    }
    std::unique_ptr<CommandStreamReceiver> csr(createCommandStream(*executionEnvironment, 0u, 1));
    std::unique_ptr<CommandStreamReceiver> csr1(createCommandStream(*executionEnvironment, 1u, 2));
    std::unique_ptr<CommandStreamReceiver> csr2(createCommandStream(*executionEnvironment, 2u, 3));
    memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                                                      PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo), 1));
    memoryManager->createAndRegisterOsContext(csr1.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                                                       PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo), 2));
    memoryManager->createAndRegisterOsContext(csr2.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                                                       PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo), 3));
    EXPECT_FALSE(memoryManager->isMemoryBudgetExhausted());
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWithRegisteredOsContextWithExhaustedMemoryBudgetWhenCallingIsMemoryBudgetExhaustedThenReturnTrue) {
    executionEnvironment->prepareRootDeviceEnvironments(3u);
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }
    executionEnvironment->initializeMemoryManager();
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->osInterface.reset();
        auto wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment->rootDeviceEnvironments[i].get()));
        wddm->init();
        executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
    }
    std::unique_ptr<CommandStreamReceiver> csr(createCommandStream(*executionEnvironment, 0u, 1));
    std::unique_ptr<CommandStreamReceiver> csr1(createCommandStream(*executionEnvironment, 1u, 2));
    std::unique_ptr<CommandStreamReceiver> csr2(createCommandStream(*executionEnvironment, 2u, 3));
    memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                                                      PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo), 1));
    memoryManager->createAndRegisterOsContext(csr1.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                                                       PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo), 2));
    memoryManager->createAndRegisterOsContext(csr2.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                                                       PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo), 3));
    auto osContext = static_cast<OsContextWin *>(memoryManager->getRegisteredEngines()[1].osContext);
    osContext->getResidencyController().setMemoryBudgetExhausted();
    EXPECT_TRUE(memoryManager->isMemoryBudgetExhausted());
}

TEST_F(MockWddmMemoryManagerTest, givenEnabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsTrueAndDeleterIsNotNullptr) {
    wddm->init();
    bool defaultEnableDeferredDeleterFlag = DebugManager.flags.EnableDeferredDeleter.get();
    DebugManager.flags.EnableDeferredDeleter.set(true);
    WddmMemoryManager memoryManager(*executionEnvironment);
    EXPECT_TRUE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_NE(nullptr, memoryManager.getDeferredDeleter());
    DebugManager.flags.EnableDeferredDeleter.set(defaultEnableDeferredDeleterFlag);
}

TEST_F(MockWddmMemoryManagerTest, givenDisabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsFalseAndDeleterIsNullptr) {
    wddm->init();
    bool defaultEnableDeferredDeleterFlag = DebugManager.flags.EnableDeferredDeleter.get();
    DebugManager.flags.EnableDeferredDeleter.set(false);
    WddmMemoryManager memoryManager(*executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    DebugManager.flags.EnableDeferredDeleter.set(defaultEnableDeferredDeleterFlag);
}

TEST_F(MockWddmMemoryManagerTest, givenPageTableManagerWhenMapAuxGpuVaCalledThenUseWddmToMap) {
    if (!HwInfoConfig::get(defaultHwInfo->platform.eProductFamily)->isPageTableManagerSupported(*defaultHwInfo)) {
        GTEST_SKIP();
    }
    wddm->init();
    WddmMemoryManager memoryManager(*executionEnvironment);
    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(*executionEnvironment, 1u, 1));
    auto hwInfo = *defaultHwInfo;
    EngineInstancesContainer regularEngines = {
        {aub_stream::ENGINE_CCS, EngineUsage::Regular}};

    memoryManager.createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[0],
                                                                                                     PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

    auto mockMngr = new MockGmmPageTableMngr();
    for (auto engine : memoryManager.getRegisteredEngines()) {
        engine.commandStreamReceiver->pageTableManager.reset(mockMngr);
    }

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(AllocationProperties(1, MemoryConstants::pageSize, AllocationType::INTERNAL_HOST_MEMORY, mockDeviceBitfield));

    GMM_DDI_UPDATEAUXTABLE expectedDdiUpdateAuxTable = {};
    expectedDdiUpdateAuxTable.BaseGpuVA = allocation->getGpuAddress();
    expectedDdiUpdateAuxTable.BaseResInfo = allocation->getDefaultGmm()->gmmResourceInfo->peekGmmResourceInfo();
    expectedDdiUpdateAuxTable.DoNotWait = true;
    expectedDdiUpdateAuxTable.Map = true;

    auto expectedCallCount = static_cast<int>(regularEngines.size());

    auto result = memoryManager.mapAuxGpuVA(allocation);
    EXPECT_TRUE(result);
    EXPECT_TRUE(memcmp(&expectedDdiUpdateAuxTable, &mockMngr->updateAuxTableParamsPassed[0].ddiUpdateAuxTable, sizeof(GMM_DDI_UPDATEAUXTABLE)) == 0);
    memoryManager.freeGraphicsMemory(allocation);
    EXPECT_EQ(expectedCallCount, mockMngr->updateAuxTableCalled);
}

TEST_F(MockWddmMemoryManagerTest, givenCompressedAllocationWhenMappedGpuVaAndPageTableNotSupportedThenMapAuxVa) {
    if (HwInfoConfig::get(defaultHwInfo->platform.eProductFamily)->isPageTableManagerSupported(*defaultHwInfo)) {
        GTEST_SKIP();
    }
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[1].get();
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), reinterpret_cast<void *>(123), 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    gmm->isCompressionEnabled = true;
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    WddmMock wddm(*executionEnvironment->rootDeviceEnvironments[1].get());
    wddm.init();
    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(*executionEnvironment, 1u, 1));
    auto hwInfo = *defaultHwInfo;
    EngineInstancesContainer regularEngines = {
        {aub_stream::ENGINE_CCS, EngineUsage::Regular}};

    executionEnvironment->memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[0],
                                                                                                                            PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

    auto mockMngr = new MockGmmPageTableMngr();
    for (auto engine : executionEnvironment->memoryManager->getRegisteredEngines()) {
        engine.commandStreamReceiver->pageTableManager.reset(mockMngr);
    }

    auto hwInfoMock = hardwareInfoTable[wddm.getGfxPlatform()->eProductFamily];
    ASSERT_NE(nullptr, hwInfoMock);
    auto result = wddm.mapGpuVirtualAddress(gmm.get(), ALLOCATION_HANDLE, wddm.getGfxPartition().Standard.Base, wddm.getGfxPartition().Standard.Limit, 0u, gpuVa);
    ASSERT_TRUE(result);

    auto gmmHelper = rootDeviceEnvironment->getGmmHelper();
    EXPECT_EQ(gmmHelper->canonize(wddm.getGfxPartition().Standard.Base), gpuVa);
    EXPECT_EQ(0u, mockMngr->updateAuxTableCalled);
}

TEST_F(MockWddmMemoryManagerTest, givenCompressedAllocationWhenMappedGpuVaAndPageTableSupportedThenMapAuxVa) {
    if (!HwInfoConfig::get(defaultHwInfo->platform.eProductFamily)->isPageTableManagerSupported(*defaultHwInfo)) {
        GTEST_SKIP();
    }
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[1].get();
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), reinterpret_cast<void *>(123), 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    gmm->isCompressionEnabled = true;
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    WddmMock wddm(*executionEnvironment->rootDeviceEnvironments[1].get());
    wddm.init();
    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(*executionEnvironment, 1u, 1));
    auto hwInfo = *defaultHwInfo;
    EngineInstancesContainer regularEngines = {
        {aub_stream::ENGINE_CCS, EngineUsage::Regular}};

    executionEnvironment->memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[0],
                                                                                                                            PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

    auto mockMngr = new MockGmmPageTableMngr();
    for (auto engine : executionEnvironment->memoryManager->getRegisteredEngines()) {
        engine.commandStreamReceiver->pageTableManager.reset(mockMngr);
    }

    GMM_DDI_UPDATEAUXTABLE expectedDdiUpdateAuxTable = {};
    auto gmmHelper = rootDeviceEnvironment->getGmmHelper();
    expectedDdiUpdateAuxTable.BaseGpuVA = gmmHelper->canonize(wddm.getGfxPartition().Standard.Base);
    expectedDdiUpdateAuxTable.BaseResInfo = gmm->gmmResourceInfo->peekGmmResourceInfo();
    expectedDdiUpdateAuxTable.DoNotWait = true;
    expectedDdiUpdateAuxTable.Map = true;

    auto expectedCallCount = executionEnvironment->memoryManager->getRegisteredEnginesCount();

    auto hwInfoMock = hardwareInfoTable[wddm.getGfxPlatform()->eProductFamily];
    ASSERT_NE(nullptr, hwInfoMock);
    auto result = wddm.mapGpuVirtualAddress(gmm.get(), ALLOCATION_HANDLE, wddm.getGfxPartition().Standard.Base, wddm.getGfxPartition().Standard.Limit, 0u, gpuVa);
    ASSERT_TRUE(result);
    EXPECT_EQ(gmmHelper->canonize(wddm.getGfxPartition().Standard.Base), gpuVa);
    EXPECT_TRUE(memcmp(&expectedDdiUpdateAuxTable, &mockMngr->updateAuxTableParamsPassed[0].ddiUpdateAuxTable, sizeof(GMM_DDI_UPDATEAUXTABLE)) == 0);
    EXPECT_EQ(expectedCallCount, mockMngr->updateAuxTableCalled);
}

TEST_F(MockWddmMemoryManagerTest, givenCompressedAllocationAndPageTableSupportedWhenReleaseingThenUnmapAuxVa) {
    if (!HwInfoConfig::get(defaultHwInfo->platform.eProductFamily)->isPageTableManagerSupported(*defaultHwInfo)) {
        GTEST_SKIP();
    }
    wddm->init();
    WddmMemoryManager memoryManager(*executionEnvironment);
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 123;

    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(*executionEnvironment, 1u, 1));
    auto hwInfo = *defaultHwInfo;
    EngineInstancesContainer regularEngines = {
        {aub_stream::ENGINE_CCS, EngineUsage::Regular}};

    memoryManager.createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[0],
                                                                                                     PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

    auto mockMngr = new MockGmmPageTableMngr();
    for (auto engine : memoryManager.getRegisteredEngines()) {
        engine.commandStreamReceiver->pageTableManager.reset(mockMngr);
    }

    auto wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemoryWithProperties(AllocationProperties(1, MemoryConstants::pageSize, AllocationType::INTERNAL_HOST_MEMORY, mockDeviceBitfield)));
    wddmAlloc->setGpuAddress(gpuVa);
    wddmAlloc->getDefaultGmm()->isCompressionEnabled = true;

    GMM_DDI_UPDATEAUXTABLE expectedDdiUpdateAuxTable = {};
    expectedDdiUpdateAuxTable.BaseGpuVA = gpuVa;
    expectedDdiUpdateAuxTable.BaseResInfo = wddmAlloc->getDefaultGmm()->gmmResourceInfo->peekGmmResourceInfo();
    expectedDdiUpdateAuxTable.DoNotWait = true;
    expectedDdiUpdateAuxTable.Map = false;

    auto expectedCallCount = memoryManager.getRegisteredEnginesCount();

    memoryManager.freeGraphicsMemory(wddmAlloc);

    EXPECT_TRUE(memcmp(&expectedDdiUpdateAuxTable, &mockMngr->updateAuxTableParamsPassed[0].ddiUpdateAuxTable, sizeof(GMM_DDI_UPDATEAUXTABLE)) == 0);
    EXPECT_EQ(expectedCallCount, mockMngr->updateAuxTableCalled);
}

TEST_F(MockWddmMemoryManagerTest, givenNonCompressedAllocationWhenReleaseingThenDontUnmapAuxVa) {
    wddm->init();
    WddmMemoryManager memoryManager(*executionEnvironment);

    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(*executionEnvironment, 1u, 1));
    auto hwInfo = *defaultHwInfo;
    EngineInstancesContainer regularEngines = {
        {aub_stream::ENGINE_CCS, EngineUsage::Regular}};

    memoryManager.createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[0],
                                                                                                     PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

    auto mockMngr = new MockGmmPageTableMngr();
    for (auto engine : memoryManager.getRegisteredEngines()) {
        engine.commandStreamReceiver->pageTableManager.reset(mockMngr);
    }

    auto wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize}));
    wddmAlloc->getDefaultGmm()->isCompressionEnabled = false;

    memoryManager.freeGraphicsMemory(wddmAlloc);
    EXPECT_EQ(0u, mockMngr->updateAuxTableCalled);
}

TEST_F(MockWddmMemoryManagerTest, givenNonCompressedAllocationWhenMappedGpuVaThenDontMapAuxVa) {
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[1].get();
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), reinterpret_cast<void *>(123), 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    gmm->isCompressionEnabled = false;
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    WddmMock wddm(*rootDeviceEnvironment);
    wddm.init();

    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(*executionEnvironment, 1u, 1));
    auto hwInfo = *defaultHwInfo;
    EngineInstancesContainer regularEngines = {
        {aub_stream::ENGINE_CCS, EngineUsage::Regular}};

    executionEnvironment->memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[0],
                                                                                                                            PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

    auto mockMngr = new MockGmmPageTableMngr();
    for (auto engine : executionEnvironment->memoryManager->getRegisteredEngines()) {
        engine.commandStreamReceiver->pageTableManager.reset(mockMngr);
    }

    auto result = wddm.mapGpuVirtualAddress(gmm.get(), ALLOCATION_HANDLE, wddm.getGfxPartition().Standard.Base, wddm.getGfxPartition().Standard.Limit, 0u, gpuVa);
    ASSERT_TRUE(result);
    EXPECT_EQ(0u, mockMngr->updateAuxTableCalled);
}

TEST_F(MockWddmMemoryManagerTest, givenFailingAllocationWhenMappedGpuVaThenReturnFalse) {
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[1].get();
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmHelper(), reinterpret_cast<void *>(123), 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    gmm->isCompressionEnabled = false;
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    WddmMock wddm(*rootDeviceEnvironment);
    wddm.init();

    auto result = wddm.mapGpuVirtualAddress(gmm.get(), 0, 0, 0, 0, gpuVa);
    ASSERT_FALSE(result);
}

TEST_F(MockWddmMemoryManagerTest, givenCompressedFlagSetWhenInternalIsUnsetThenDontUpdateAuxTable) {
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    wddm->init();
    WddmMemoryManager memoryManager(*executionEnvironment);

    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(*executionEnvironment, 1u, 1));
    auto hwInfo = *defaultHwInfo;
    EngineInstancesContainer regularEngines = {
        {aub_stream::ENGINE_CCS, EngineUsage::Regular}};

    memoryManager.createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[0],
                                                                                                     PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

    auto mockMngr = new MockGmmPageTableMngr();
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[1].get();
    for (auto engine : memoryManager.getRegisteredEngines()) {
        engine.commandStreamReceiver->pageTableManager.reset(mockMngr);
    }

    auto myGmm = new Gmm(rootDeviceEnvironment->getGmmHelper(), reinterpret_cast<void *>(123), 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    myGmm->isCompressionEnabled = false;
    myGmm->gmmResourceInfo->getResourceFlags()->Info.RenderCompressed = 1;

    auto wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize}));
    delete wddmAlloc->getDefaultGmm();
    wddmAlloc->setDefaultGmm(myGmm);

    auto result = wddm->mapGpuVirtualAddress(myGmm, ALLOCATION_HANDLE, wddm->getGfxPartition().Standard.Base, wddm->getGfxPartition().Standard.Limit, 0u, gpuVa);
    EXPECT_TRUE(result);
    memoryManager.freeGraphicsMemory(wddmAlloc);
    EXPECT_EQ(0u, mockMngr->updateAuxTableCalled);
}

TEST_F(MockWddmMemoryManagerTest, givenCompressedFlagSetWhenInternalIsSetThenUpdateAuxTable) {
    if (!HwInfoConfig::get(defaultHwInfo->platform.eProductFamily)->isPageTableManagerSupported(*defaultHwInfo)) {
        GTEST_SKIP();
    }
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    wddm->init();
    WddmMemoryManager memoryManager(*executionEnvironment);

    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(*executionEnvironment, 1u, 1));
    auto hwInfo = *defaultHwInfo;
    EngineInstancesContainer regularEngines = {
        {aub_stream::ENGINE_CCS, EngineUsage::Regular}};

    memoryManager.createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[0],
                                                                                                     PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

    auto mockMngr = new MockGmmPageTableMngr();
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[1].get();
    rootDeviceEnvironment->executionEnvironment.initializeMemoryManager();
    for (auto engine : memoryManager.getRegisteredEngines()) {
        engine.commandStreamReceiver->pageTableManager.reset(mockMngr);
    }

    auto myGmm = new Gmm(rootDeviceEnvironment->getGmmHelper(), reinterpret_cast<void *>(123), 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    myGmm->isCompressionEnabled = true;
    myGmm->gmmResourceInfo->getResourceFlags()->Info.RenderCompressed = 1;

    auto wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize}));
    delete wddmAlloc->getDefaultGmm();
    wddmAlloc->setDefaultGmm(myGmm);

    auto expectedCallCount = memoryManager.getRegisteredEnginesCount();

    auto result = wddm->mapGpuVirtualAddress(myGmm, ALLOCATION_HANDLE, wddm->getGfxPartition().Standard.Base, wddm->getGfxPartition().Standard.Limit, 0u, gpuVa);
    EXPECT_TRUE(result);
    memoryManager.freeGraphicsMemory(wddmAlloc);
    EXPECT_EQ(expectedCallCount, mockMngr->updateAuxTableCalled);
}

TEST_F(WddmMemoryManagerTest2, givenReadOnlyMemoryWhenCreateAllocationFailsThenPopulateOsHandlesReturnsInvalidPointer) {
    OsHandleStorage handleStorage;
    handleStorage.fragmentCount = 1;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 0x1000;
    handleStorage.fragmentStorageData[0].freeTheFragment = false;

    wddm->callBaseCreateAllocationsAndMapGpuVa = false;
    wddm->createAllocationsAndMapGpuVaStatus = STATUS_GRAPHICS_NO_VIDEO_MEMORY;

    auto result = memoryManager->populateOsHandles(handleStorage, 0);

    EXPECT_EQ(MemoryManager::AllocationStatus::InvalidHostPointer, result);
    handleStorage.fragmentStorageData[0].freeTheFragment = true;
    memoryManager->cleanOsHandles(handleStorage, 0);
}

TEST_F(WddmMemoryManagerTest2, givenReadOnlyMemoryPassedToPopulateOsHandlesWhenCreateAllocationFailsThenAllocatedFragmentsAreNotStored) {
    OsHandleStorage handleStorage;
    OsHandleWin handle;
    handleStorage.fragmentCount = 2;
    handleStorage.fragmentStorageData[0].osHandleStorage = &handle;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 0x1000;

    handleStorage.fragmentStorageData[1].cpuPtr = reinterpret_cast<void *>(0x2000);
    handleStorage.fragmentStorageData[1].fragmentSize = 0x6000;

    wddm->callBaseCreateAllocationsAndMapGpuVa = false;
    wddm->createAllocationsAndMapGpuVaStatus = STATUS_GRAPHICS_NO_VIDEO_MEMORY;

    auto result = memoryManager->populateOsHandles(handleStorage, mockRootDeviceIndex);
    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());

    EXPECT_EQ(MemoryManager::AllocationStatus::InvalidHostPointer, result);
    auto numberOfStoredFragments = hostPtrManager->getFragmentCount();
    EXPECT_EQ(0u, numberOfStoredFragments);
    EXPECT_EQ(nullptr, hostPtrManager->getFragment({handleStorage.fragmentStorageData[1].cpuPtr, mockRootDeviceIndex}));

    handleStorage.fragmentStorageData[1].freeTheFragment = true;
    memoryManager->cleanOsHandles(handleStorage, mockRootDeviceIndex);
}

TEST(WddmMemoryManagerCleanupTest, givenUsedTagAllocationInWddmMemoryManagerWhenCleanupMemoryManagerThenDontAccessCsr) {
    ExecutionEnvironment &executionEnvironment = *platform()->peekExecutionEnvironment();
    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(executionEnvironment, 0, 1));
    auto wddm = new WddmMock(*executionEnvironment.rootDeviceEnvironments[0].get());
    auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo);
    wddm->init();

    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
    executionEnvironment.memoryManager = std::make_unique<WddmMemoryManager>(executionEnvironment);
    auto osContext = executionEnvironment.memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular},
                                                                                                                                            preemptionMode));
    csr->setupContext(*osContext);

    auto tagAllocator = csr->getEventPerfCountAllocator(100);
    auto allocation = tagAllocator->getTag()->getBaseGraphicsAllocation();
    allocation->getDefaultGraphicsAllocation()->updateTaskCount(1, csr->getOsContext().getContextId());
    csr.reset();
    EXPECT_NO_THROW(executionEnvironment.memoryManager.reset());
}

TEST_F(WddmMemoryManagerSimpleTest, whenDestroyingLockedAllocationThatDoesntNeedMakeResidentBeforeLockThenDontEvictAllocationFromWddmTemporaryResources) {
    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize}));
    memoryManager->lockResource(allocation);
    EXPECT_FALSE(allocation->needsMakeResidentBeforeLock);
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(0u, mockTemporaryResources->evictResourceResult.called);
}
TEST_F(WddmMemoryManagerSimpleTest, whenDestroyingNotLockedAllocationThatDoesntNeedMakeResidentBeforeLockThenDontEvictAllocationFromWddmTemporaryResources) {
    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize}));
    EXPECT_FALSE(allocation->isLocked());
    EXPECT_FALSE(allocation->needsMakeResidentBeforeLock);
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(0u, mockTemporaryResources->evictResourceResult.called);
}
TEST_F(WddmMemoryManagerSimpleTest, whenDestroyingLockedAllocationThatNeedsMakeResidentBeforeLockThenRemoveTemporaryResource) {
    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize}));
    allocation->needsMakeResidentBeforeLock = true;
    memoryManager->lockResource(allocation);
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(1u, mockTemporaryResources->removeResourceResult.called);
}
TEST_F(WddmMemoryManagerSimpleTest, whenDestroyingNotLockedAllocationThatNeedsMakeResidentBeforeLockThenDontEvictAllocationFromWddmTemporaryResources) {
    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize}));
    allocation->needsMakeResidentBeforeLock = true;
    EXPECT_FALSE(allocation->isLocked());
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(0u, mockTemporaryResources->evictResourceResult.called);
}
TEST_F(WddmMemoryManagerSimpleTest, whenDestroyingAllocationWithReservedGpuVirtualAddressThenReleaseTheAddress) {
    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize}));
    uint64_t gpuAddress = 0x123;
    uint64_t sizeForFree = 0x1234;
    allocation->reservedGpuVirtualAddress = gpuAddress;
    allocation->reservedSizeForGpuVirtualAddress = sizeForFree;
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(1u, wddm->freeGpuVirtualAddressResult.called);
    EXPECT_EQ(gpuAddress, wddm->freeGpuVirtualAddressResult.uint64ParamPassed);
    EXPECT_EQ(sizeForFree, wddm->freeGpuVirtualAddressResult.sizePassed);
}

TEST_F(WddmMemoryManagerSimpleTest, givenAllocationWithReservedGpuVirtualAddressWhenMapCallFailsDuringCreateWddmAllocationThenReleasePreferredAddress) {
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper(), 1);
    allocation.setAllocationType(AllocationType::KERNEL_ISA);
    uint64_t gpuAddress = 0x123;
    uint64_t sizeForFree = 0x1234;
    allocation.reservedGpuVirtualAddress = gpuAddress;
    allocation.reservedSizeForGpuVirtualAddress = sizeForFree;

    wddm->callBaseMapGpuVa = false;
    wddm->mapGpuVaStatus = false;

    memoryManager->createWddmAllocation(&allocation, nullptr);
    EXPECT_EQ(1u, wddm->freeGpuVirtualAddressResult.called);
    EXPECT_EQ(gpuAddress, wddm->freeGpuVirtualAddressResult.uint64ParamPassed);
    EXPECT_EQ(sizeForFree, wddm->freeGpuVirtualAddressResult.sizePassed);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMultiHandleAllocationAndPreferredGpuVaIsSpecifiedWhenCreateAllocationIsCalledThenAllocationHasProperGpuAddressAndHeapSvmIsUsed) {
    if (memoryManager->isLimitedRange(0)) {
        GTEST_SKIP();
    }

    uint32_t numGmms = 10;
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper(), numGmms);
    allocation.setAllocationType(AllocationType::BUFFER);
    allocation.storageInfo.multiStorage = true;

    wddm->callBaseMapGpuVa = true;

    uint64_t gpuPreferredVa = 0x20000ull;

    memoryManager->createWddmAllocation(&allocation, reinterpret_cast<void *>(gpuPreferredVa));
    EXPECT_EQ(gpuPreferredVa, allocation.getGpuAddress());
    EXPECT_EQ(numGmms, wddm->mapGpuVirtualAddressResult.called);

    auto gmmSize = allocation.getDefaultGmm()->gmmResourceInfo->getSizeAllocation();
    auto lastRequiredAddress = (numGmms - 1) * gmmSize + gpuPreferredVa;
    EXPECT_EQ(lastRequiredAddress, wddm->mapGpuVirtualAddressResult.uint64ParamPassed);
    EXPECT_GT(lastRequiredAddress, memoryManager->getGfxPartition(0)->getHeapMinimalAddress(HeapIndex::HEAP_SVM));
    EXPECT_LT(lastRequiredAddress, memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::HEAP_SVM));
}

TEST_F(WddmMemoryManagerSimpleTest, givenSvmCpuAllocationWhenSizeAndAlignmentProvidedThenAllocateMemoryReserveGpuVa) {
    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }

    size_t size = 2 * MemoryConstants::megaByte;
    MockAllocationProperties properties{csr->getRootDeviceIndex(), true, size, AllocationType::SVM_CPU, mockDeviceBitfield};
    properties.alignment = size;
    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(properties));
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(allocation->getUnderlyingBuffer(), allocation->getDriverAllocatedCpuPtr());
    // limited platforms will not use heap HeapIndex::HEAP_SVM
    if (executionEnvironment->rootDeviceEnvironments[allocation->getRootDeviceIndex()]->isFullRangeSvm()) {
        EXPECT_EQ(alignUp(allocation->getReservedAddressPtr(), size), reinterpret_cast<void *>(allocation->getGpuAddress()));
    }
    EXPECT_EQ((2 * size), allocation->getReservedAddressSize());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenWriteCombinedAllocationThenCpuAddressIsEqualToGpuAddress) {
    if (is32bit) {
        GTEST_SKIP();
    }
    memoryManager.reset(new MockWddmMemoryManager(true, true, *executionEnvironment));
    size_t size = 2 * MemoryConstants::megaByte;
    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties({0, size, AllocationType::WRITE_COMBINED, mockDeviceBitfield}));
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_NE(nullptr, reinterpret_cast<void *>(allocation->getGpuAddress()));

    if (executionEnvironment->rootDeviceEnvironments[allocation->getRootDeviceIndex()]->isFullRangeSvm()) {
        EXPECT_EQ(allocation->getUnderlyingBuffer(), reinterpret_cast<void *>(allocation->getGpuAddress()));
    }

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenDebugVariableWhenCreatingWddmMemoryManagerThenSetSupportForMultiStorageResources) {
    DebugManagerStateRestore restore;
    EXPECT_TRUE(memoryManager->supportsMultiStorageResources);

    {
        DebugManager.flags.EnableMultiStorageResources.set(0);
        MockWddmMemoryManager memoryManager(true, true, *executionEnvironment);
        EXPECT_FALSE(memoryManager.supportsMultiStorageResources);
    }

    {
        DebugManager.flags.EnableMultiStorageResources.set(1);
        MockWddmMemoryManager memoryManager(true, true, *executionEnvironment);
        EXPECT_TRUE(memoryManager.supportsMultiStorageResources);
    }
}

TEST_F(WddmMemoryManagerSimpleTest, givenBufferHostMemoryAllocationAndLimitedRangeAnd32BitThenAllocationGoesToExternalHeap) {
    if (executionEnvironment->rootDeviceEnvironments[0]->isFullRangeSvm() || !is32bit) {
        GTEST_SKIP();
    }

    memoryManager.reset(new MockWddmMemoryManager(true, true, *executionEnvironment));
    size_t size = 2 * MemoryConstants::megaByte;
    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties({0, size, AllocationType::BUFFER_HOST_MEMORY, mockDeviceBitfield}));
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    uint64_t gpuAddress = allocation->getGpuAddress();
    EXPECT_NE(0ULL, gpuAddress);
    EXPECT_EQ(0ULL, gpuAddress & 0xffFFffF000000000);

    auto gmmHelper = rootDeviceEnvironment->getGmmHelper();
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_EXTERNAL)), gpuAddress);
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::HEAP_EXTERNAL)), gpuAddress);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenDebugModuleAreaTypeWhenCreatingAllocationThen32BitAllocationWithFrontWindowGpuVaIsReturned) {
    const auto size = MemoryConstants::pageSize64k;

    NEO::AllocationProperties properties{0, true, size,
                                         NEO::AllocationType::DEBUG_MODULE_AREA,
                                         false,
                                         mockDeviceBitfield};

    auto moduleDebugArea = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    EXPECT_NE(nullptr, moduleDebugArea);
    EXPECT_NE(nullptr, moduleDebugArea->getUnderlyingBuffer());
    EXPECT_GE(moduleDebugArea->getUnderlyingBufferSize(), size);

    auto address64bit = moduleDebugArea->getGpuAddressToPatch();
    EXPECT_LT(address64bit, MemoryConstants::max32BitAddress);
    EXPECT_TRUE(moduleDebugArea->is32BitAllocation());

    auto gmmHelper = rootDeviceEnvironment->getGmmHelper();
    auto frontWindowBase = gmmHelper->canonize(memoryManager->getGfxPartition(moduleDebugArea->getRootDeviceIndex())->getHeapBase(memoryManager->selectInternalHeap(moduleDebugArea->isAllocatedInLocalMemoryPool())));
    EXPECT_EQ(frontWindowBase, moduleDebugArea->getGpuBaseAddress());
    EXPECT_EQ(frontWindowBase, moduleDebugArea->getGpuAddress());

    memoryManager->freeGraphicsMemory(moduleDebugArea);
}

TEST(WddmMemoryManager, givenMultipleRootDeviceWhenMemoryManagerGetsWddmThenWddmIsFromCorrectRootDevice) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleRootDevices.set(4);
    VariableBackup<UltHwConfig> backup{&ultHwConfig};
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    auto executionEnvironment = platform()->peekExecutionEnvironment();
    prepareDeviceEnvironments(*executionEnvironment);

    MockWddmMemoryManager wddmMemoryManager(*executionEnvironment);
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        auto wddmFromRootDevice = executionEnvironment->rootDeviceEnvironments[i]->osInterface->getDriverModel()->as<Wddm>();
        EXPECT_EQ(wddmFromRootDevice, &wddmMemoryManager.getWddm(i));
    }
}

TEST(WddmMemoryManager, givenMultipleRootDeviceWhenCreateMemoryManagerThenTakeMaxMallocRestrictionAvailable) {
    uint32_t numRootDevices = 4u;
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    VariableBackup<UltHwConfig> backup{&ultHwConfig};
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    auto executionEnvironment = platform()->peekExecutionEnvironment();
    prepareDeviceEnvironments(*executionEnvironment);
    for (auto i = 0u; i < numRootDevices; i++) {
        auto wddm = static_cast<WddmMock *>(executionEnvironment->rootDeviceEnvironments[i]->osInterface->getDriverModel()->as<Wddm>());
        wddm->minAddress = i * (numRootDevices - i);
    }

    MockWddmMemoryManager wddmMemoryManager(*executionEnvironment);

    EXPECT_EQ(4u, wddmMemoryManager.getAlignedMallocRestrictions()->minAddress);
}

TEST(WddmMemoryManager, givenNoLocalMemoryOnAnyDeviceWhenIsCpuCopyRequiredIsCalledThenFalseIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableLocalMemory.set(false);
    VariableBackup<UltHwConfig> backup{&ultHwConfig};
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    auto executionEnvironment = platform()->peekExecutionEnvironment();
    prepareDeviceEnvironments(*executionEnvironment);
    MockWddmMemoryManager wddmMemoryManager(*executionEnvironment);
    EXPECT_FALSE(wddmMemoryManager.isCpuCopyRequired(&restorer));
}

TEST(WddmMemoryManager, givenLocalPointerPassedToIsCpuCopyRequiredThenFalseIsReturned) {
    auto executionEnvironment = platform()->peekExecutionEnvironment();
    VariableBackup<UltHwConfig> backup{&ultHwConfig};
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    prepareDeviceEnvironments(*executionEnvironment);
    MockWddmMemoryManager wddmMemoryManager(*executionEnvironment);
    EXPECT_FALSE(wddmMemoryManager.isCpuCopyRequired(&backup));
    // call multiple times to make sure that result is constant
    EXPECT_FALSE(wddmMemoryManager.isCpuCopyRequired(&backup));
    EXPECT_FALSE(wddmMemoryManager.isCpuCopyRequired(&backup));
    EXPECT_FALSE(wddmMemoryManager.isCpuCopyRequired(&backup));
    EXPECT_FALSE(wddmMemoryManager.isCpuCopyRequired(&backup));
}

TEST_F(WddmMemoryManagerSimpleTest, whenWddmMemoryManagerIsCreatedThenAlignmentSelectorHasExpectedAlignments) {
    std::vector<AlignmentSelector::CandidateAlignment> expectedAlignments = {
        {MemoryConstants::pageSize2Mb, false, 0.1f, HeapIndex::TOTAL_HEAPS},
        {MemoryConstants::pageSize64k, true, AlignmentSelector::anyWastage, HeapIndex::TOTAL_HEAPS},
    };

    MockWddmMemoryManager memoryManager(true, true, *executionEnvironment);
    EXPECT_EQ(expectedAlignments, memoryManager.alignmentSelector.peekCandidateAlignments());
}

TEST_F(WddmMemoryManagerSimpleTest, given2MbPagesDisabledWhenWddmMemoryManagerIsCreatedThenAlignmentSelectorHasExpectedAlignments) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.AlignLocalMemoryVaTo2MB.set(0);

    std::vector<AlignmentSelector::CandidateAlignment> expectedAlignments = {
        {MemoryConstants::pageSize64k, true, AlignmentSelector::anyWastage, HeapIndex::TOTAL_HEAPS},
    };

    MockWddmMemoryManager memoryManager(true, true, *executionEnvironment);
    EXPECT_EQ(expectedAlignments, memoryManager.alignmentSelector.peekCandidateAlignments());
}

TEST_F(WddmMemoryManagerSimpleTest, givenCustomAlignmentWhenWddmMemoryManagerIsCreatedThenAlignmentSelectorHasExpectedAlignments) {
    DebugManagerStateRestore restore{};

    {
        DebugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(MemoryConstants::megaByte);
        std::vector<AlignmentSelector::CandidateAlignment> expectedAlignments = {
            {MemoryConstants::pageSize2Mb, false, 0.1f, HeapIndex::TOTAL_HEAPS},
            {MemoryConstants::megaByte, false, AlignmentSelector::anyWastage, HeapIndex::TOTAL_HEAPS},
            {MemoryConstants::pageSize64k, true, AlignmentSelector::anyWastage, HeapIndex::TOTAL_HEAPS},
        };
        MockWddmMemoryManager memoryManager(true, true, *executionEnvironment);
        EXPECT_EQ(expectedAlignments, memoryManager.alignmentSelector.peekCandidateAlignments());
    }

    {
        DebugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(2 * MemoryConstants::pageSize2Mb);
        std::vector<AlignmentSelector::CandidateAlignment> expectedAlignments = {
            {2 * MemoryConstants::pageSize2Mb, false, AlignmentSelector::anyWastage, HeapIndex::TOTAL_HEAPS},
            {MemoryConstants::pageSize2Mb, false, 0.1f, HeapIndex::TOTAL_HEAPS},
            {MemoryConstants::pageSize64k, true, AlignmentSelector::anyWastage, HeapIndex::TOTAL_HEAPS},
        };
        MockWddmMemoryManager memoryManager(true, true, *executionEnvironment);
        EXPECT_EQ(expectedAlignments, memoryManager.alignmentSelector.peekCandidateAlignments());
    }
}

TEST_F(WddmMemoryManagerSimpleTest, givenWddmMemoryManagerWhenGettingGlobalMemoryPercentThenCorrectValueIsReturned) {
    MockWddmMemoryManager memoryManager(true, true, *executionEnvironment);
    uint32_t rootDeviceIndex = 0u;
    EXPECT_EQ(memoryManager.getPercentOfGlobalMemoryAvailable(rootDeviceIndex), 0.8);
}

TEST_F(WddmMemoryManagerSimpleTest, whenAlignmentRequirementExceedsPageSizeThenAllocateGraphicsMemoryFromSystemPtr) {
    struct MockWddmMemoryManagerAllocateWithAlignment : MockWddmMemoryManager {
        using MockWddmMemoryManager::MockWddmMemoryManager;

        GraphicsAllocation *allocateSystemMemoryAndCreateGraphicsAllocationFromIt(const AllocationData &allocationData) override {
            ++callCount.allocateSystemMemoryAndCreateGraphicsAllocationFromIt;
            return nullptr;
        }
        GraphicsAllocation *allocateGraphicsMemoryUsingKmdAndMapItToCpuVA(const AllocationData &allocationData, bool allowLargePages) override {
            ++callCount.allocateGraphicsMemoryUsingKmdAndMapItToCpuVA;
            return nullptr;
        }

        struct {
            int allocateSystemMemoryAndCreateGraphicsAllocationFromIt = 0;
            int allocateGraphicsMemoryUsingKmdAndMapItToCpuVA = 0;
        } callCount;
    };

    MockWddmMemoryManagerAllocateWithAlignment memoryManager(true, true, *executionEnvironment);

    AllocationData allocData = {};
    allocData.size = 1024;
    allocData.alignment = MemoryConstants::pageSize64k * 4;
    memoryManager.allocateGraphicsMemoryWithAlignment(allocData);
    EXPECT_EQ(1U, memoryManager.callCount.allocateSystemMemoryAndCreateGraphicsAllocationFromIt);
    EXPECT_EQ(0U, memoryManager.callCount.allocateGraphicsMemoryUsingKmdAndMapItToCpuVA);

    memoryManager.callCount.allocateSystemMemoryAndCreateGraphicsAllocationFromIt = 0;
    memoryManager.callCount.allocateGraphicsMemoryUsingKmdAndMapItToCpuVA = 0;

    allocData.size = 1024;
    allocData.alignment = MemoryConstants::pageSize;
    memoryManager.allocateGraphicsMemoryWithAlignment(allocData);
    if (preferredAllocationMethod == GfxMemoryAllocationMethod::AllocateByKmd) {
        EXPECT_EQ(0U, memoryManager.callCount.allocateSystemMemoryAndCreateGraphicsAllocationFromIt);
        EXPECT_EQ(1U, memoryManager.callCount.allocateGraphicsMemoryUsingKmdAndMapItToCpuVA);
    } else {
        EXPECT_EQ(1U, memoryManager.callCount.allocateSystemMemoryAndCreateGraphicsAllocationFromIt);
        EXPECT_EQ(0U, memoryManager.callCount.allocateGraphicsMemoryUsingKmdAndMapItToCpuVA);
    }
}

struct WddmWithMockedLock : public WddmMock {
    using WddmMock::WddmMock;

    void *lockResource(const D3DKMT_HANDLE &handle, bool applyMakeResidentPriorToLock, size_t size) override {
        if (handle < storageLocked.size()) {
            storageLocked.set(handle);
        }
        return storages[handle];
    }
    std::bitset<4> storageLocked{};
    uint8_t storages[EngineLimits::maxHandleCount][MemoryConstants::pageSize64k] = {0u};
};

TEST(WddmMemoryManagerCopyMemoryToAllocationBanksTest, givenAllocationWithMultiTilePlacementWhenCopyDataSpecificMemoryBanksThenLockOnlySpecificStorages) {
    uint8_t sourceData[32]{};
    size_t offset = 3;
    size_t sourceAllocationSize = sizeof(sourceData);
    auto hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrLocalMemory = true;

    MockExecutionEnvironment executionEnvironment(&hwInfo);
    executionEnvironment.initGmm();
    auto wddm = new WddmWithMockedLock(*executionEnvironment.rootDeviceEnvironments[0]);
    wddm->init();
    MemoryManagerCreate<WddmMemoryManager> memoryManager(true, true, executionEnvironment);

    MockWddmAllocation mockAllocation(executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper());

    mockAllocation.storageInfo.memoryBanks = 0b1111;
    DeviceBitfield memoryBanksToCopy = 0b1010;
    mockAllocation.handles.resize(4);
    for (auto index = 0u; index < 4; index++) {
        wddm->storageLocked.set(index, false);
        if (mockAllocation.storageInfo.memoryBanks.test(index)) {
            mockAllocation.handles[index] = index;
        }
    }
    std::vector<uint8_t> dataToCopy(sourceAllocationSize, 1u);
    auto ret = memoryManager.copyMemoryToAllocationBanks(&mockAllocation, offset, dataToCopy.data(), dataToCopy.size(), memoryBanksToCopy);
    EXPECT_TRUE(ret);

    EXPECT_FALSE(wddm->storageLocked.test(0));
    ASSERT_TRUE(wddm->storageLocked.test(1));
    EXPECT_FALSE(wddm->storageLocked.test(2));
    ASSERT_TRUE(wddm->storageLocked.test(3));
    EXPECT_EQ(0, memcmp(ptrOffset(wddm->storages[1], offset), dataToCopy.data(), dataToCopy.size()));
    EXPECT_EQ(0, memcmp(ptrOffset(wddm->storages[3], offset), dataToCopy.data(), dataToCopy.size()));
}

class WddmMemoryManagerMock : public MockWddmMemoryManagerFixture, public ::testing::Test {
  public:
    void SetUp() override {
        MockWddmMemoryManagerFixture::SetUp();
    }
    void TearDown() override {
        MockWddmMemoryManagerFixture::TearDown();
    }
};

TEST_F(WddmMemoryManagerMock, givenAllocationWithReservedGpuVirtualAddressWhenMapCallFailsDuringCreateWddmAllocationThenReleasePreferredAddress) {
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper(), 4);
    allocation.setAllocationType(AllocationType::KERNEL_ISA);
    uint64_t gpuAddress = 0x123;
    uint64_t sizeForFree = 0x1234;
    allocation.reservedGpuVirtualAddress = gpuAddress;
    allocation.reservedSizeForGpuVirtualAddress = sizeForFree;

    wddm->callBaseMapGpuVa = false;
    wddm->mapGpuVaStatus = false;

    memoryManager->createWddmAllocation(&allocation, nullptr);
    EXPECT_EQ(1u, wddm->freeGpuVirtualAddressResult.called);
    EXPECT_EQ(gpuAddress, wddm->freeGpuVirtualAddressResult.uint64ParamPassed);
    EXPECT_EQ(sizeForFree, wddm->freeGpuVirtualAddressResult.sizePassed);
}

struct PlatformWithFourDevicesTest : public ::testing::Test {
    PlatformWithFourDevicesTest() {
        ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    }
    void SetUp() override {
        DebugManager.flags.CreateMultipleSubDevices.set(4);
        initPlatform();
    }

    DebugManagerStateRestore restorer;

    VariableBackup<UltHwConfig> backup{&ultHwConfig};
};

TEST_F(PlatformWithFourDevicesTest, whenCreateColoredAllocationAndWddmReturnsCanonizedAddressDuringMapingVAThenAddressIsBeingDecanonizedAndAbortIsNotThrownFromUnrecoverableIfStatement) {
    struct CanonizeAddressMockWddm : public WddmMock {
        using WddmMock::WddmMock;

        bool mapGpuVirtualAddress(Gmm *gmm, D3DKMT_HANDLE handle, D3DGPU_VIRTUAL_ADDRESS minimumAddress, D3DGPU_VIRTUAL_ADDRESS maximumAddress, D3DGPU_VIRTUAL_ADDRESS preferredAddress, D3DGPU_VIRTUAL_ADDRESS &gpuPtr) override {
            auto gmmHelper = rootDeviceEnvironment.getGmmHelper();
            gpuPtr = gmmHelper->canonize(preferredAddress);
            return mapGpuVaStatus;
        }
    };

    auto wddm = new CanonizeAddressMockWddm(*platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]);
    wddm->init();
    auto osInterfaceMock = new OSInterface();
    auto callBaseDestroyBackup = wddm->callBaseDestroyAllocations;
    wddm->callBaseDestroyAllocations = false;
    wddm->mapGpuVaStatus = true;
    osInterfaceMock->setDriverModel(std::unique_ptr<DriverModel>(wddm));
    auto osInterfaceBackUp = platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.release();
    platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(osInterfaceMock);

    MockWddmMemoryManager memoryManager(true, true, *platform()->peekExecutionEnvironment());
    memoryManager.supportsMultiStorageResources = true;

    platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->featureTable.flags.ftrLocalMemory = true;
    platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->featureTable.flags.ftrMultiTileArch = true;

    GraphicsAllocation *allocation = nullptr;
    EXPECT_NO_THROW(allocation = memoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, true, 4 * MemoryConstants::pageSize64k, AllocationType::BUFFER, true, mockDeviceBitfield}));
    EXPECT_NE(nullptr, allocation);

    memoryManager.freeGraphicsMemory(allocation);
    wddm->callBaseDestroyAllocations = callBaseDestroyBackup;
    platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(osInterfaceBackUp);
}

TEST_F(PlatformWithFourDevicesTest, givenDifferentAllocationSizesWhenColourAllocationThenResourceIsSpreadProperly) {
    auto wddm = reinterpret_cast<WddmMock *>(platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Wddm>());
    wddm->mapGpuVaStatus = true;
    VariableBackup<bool> restorer{&wddm->callBaseMapGpuVa, false};

    MockWddmMemoryManager memoryManager(true, true, *platform()->peekExecutionEnvironment());
    memoryManager.supportsMultiStorageResources = true;

    platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->featureTable.flags.ftrLocalMemory = true;
    platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->featureTable.flags.ftrMultiTileArch = true;

    // We are allocating memory from 4 to 12 pages and want to check if remainders (1, 2 or 3 pages in case of 4 devices) are spread equally.
    for (int additionalSize = 0; additionalSize <= 8; additionalSize++) {
        auto allocation = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, true, (4 + additionalSize) * MemoryConstants::pageSize64k, AllocationType::BUFFER, true, 0b1111}));
        auto handles = allocation->getNumGmms();

        EXPECT_EQ(4u, handles);
        auto size = allocation->getAlignedSize() / MemoryConstants::pageSize64k;
        switch (size % handles) {
        case 0:
            EXPECT_EQ(size / handles * MemoryConstants::pageSize64k, allocation->getGmm(0)->gmmResourceInfo->getSizeAllocation());
            EXPECT_EQ(size / handles * MemoryConstants::pageSize64k, allocation->getGmm(1)->gmmResourceInfo->getSizeAllocation());
            EXPECT_EQ(size / handles * MemoryConstants::pageSize64k, allocation->getGmm(2)->gmmResourceInfo->getSizeAllocation());
            EXPECT_EQ(size / handles * MemoryConstants::pageSize64k, allocation->getGmm(3)->gmmResourceInfo->getSizeAllocation());
            break;
        case 1:
            EXPECT_EQ((size / handles + 1) * MemoryConstants::pageSize64k, allocation->getGmm(0)->gmmResourceInfo->getSizeAllocation());
            EXPECT_EQ(size / handles * MemoryConstants::pageSize64k, allocation->getGmm(1)->gmmResourceInfo->getSizeAllocation());
            EXPECT_EQ(size / handles * MemoryConstants::pageSize64k, allocation->getGmm(2)->gmmResourceInfo->getSizeAllocation());
            EXPECT_EQ(size / handles * MemoryConstants::pageSize64k, allocation->getGmm(3)->gmmResourceInfo->getSizeAllocation());
            break;
        case 2:
            EXPECT_EQ((size / handles + 1) * MemoryConstants::pageSize64k, allocation->getGmm(0)->gmmResourceInfo->getSizeAllocation());
            EXPECT_EQ((size / handles + 1) * MemoryConstants::pageSize64k, allocation->getGmm(1)->gmmResourceInfo->getSizeAllocation());
            EXPECT_EQ(size / handles * MemoryConstants::pageSize64k, allocation->getGmm(2)->gmmResourceInfo->getSizeAllocation());
            EXPECT_EQ(size / handles * MemoryConstants::pageSize64k, allocation->getGmm(3)->gmmResourceInfo->getSizeAllocation());
            break;
        case 3:
            EXPECT_EQ((size / handles + 1) * MemoryConstants::pageSize64k, allocation->getGmm(0)->gmmResourceInfo->getSizeAllocation());
            EXPECT_EQ((size / handles + 1) * MemoryConstants::pageSize64k, allocation->getGmm(1)->gmmResourceInfo->getSizeAllocation());
            EXPECT_EQ((size / handles + 1) * MemoryConstants::pageSize64k, allocation->getGmm(2)->gmmResourceInfo->getSizeAllocation());
            EXPECT_EQ(size / handles * MemoryConstants::pageSize64k, allocation->getGmm(3)->gmmResourceInfo->getSizeAllocation());
        default:
            break;
        }
        memoryManager.freeGraphicsMemory(allocation);
    }
}

TEST_F(PlatformWithFourDevicesTest, whenCreateScratchSpaceInSingleTileQueueThenTheAllocationHasOneHandle) {
    MemoryManagerCreate<WddmMemoryManager> memoryManager(true, true, *platform()->peekExecutionEnvironment());

    AllocationProperties properties{mockRootDeviceIndex, true, 1u, AllocationType::SCRATCH_SURFACE, false, false, mockDeviceBitfield};
    auto allocation = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemoryWithProperties(properties));
    EXPECT_EQ(1u, allocation->getNumGmms());
    memoryManager.freeGraphicsMemory(allocation);
}
