/*
 * Copyright (C) 2017-2020 Intel Corporation
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
#include "shared/source/os_interface/os_library.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/wddm_residency_controller.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/helpers/ult_hw_config.h"
#include "shared/test/unit_test/mocks/mock_device.h"
#include "shared/test/unit_test/utilities/base_object_utils.h"

#include "opencl/source/helpers/memory_properties_helpers.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/helpers/execution_environment_helper.h"
#include "opencl/test/unit_test/helpers/unit_test_helper.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_deferred_deleter.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "opencl/test/unit_test/mocks/mock_os_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/os_interface/windows/mock_wddm_allocation.h"

using namespace NEO;
using namespace ::testing;

void WddmMemoryManagerFixture::SetUp() {
    GdiDllFixture::SetUp();

    executionEnvironment = platform()->peekExecutionEnvironment();
    rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex].get();
    wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
    if (defaultHwInfo->capabilityTable.ftrRenderCompressedBuffers || defaultHwInfo->capabilityTable.ftrRenderCompressedImages) {
        GMM_TRANSLATIONTABLE_CALLBACKS dummyTTCallbacks = {};
        rootDeviceEnvironment->pageTableManager.reset(GmmPageTableMngr::create(nullptr, 0, &dummyTTCallbacks));
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
    MockWddmAllocation allocation;
    MockOsContext osContext(1u, 1, aub_stream::ENGINE_RCS,
                            PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo),
                            false, false, false);
    allocation.setTrimCandidateListPosition(osContext.getContextId(), 700u);
    EXPECT_EQ(trimListUnusedPosition, allocation.getTrimCandidateListPosition(0u));
    EXPECT_EQ(700u, allocation.getTrimCandidateListPosition(1u));
}

TEST(WddmAllocationTest, givenAllocationCreatedWithOsContextCountOneWhenItIsCreatedThenMaxOsContextCountIsUsedInstead) {
    MockWddmAllocation allocation;
    allocation.setTrimCandidateListPosition(1u, 700u);
    EXPECT_EQ(700u, allocation.getTrimCandidateListPosition(1u));
    EXPECT_EQ(trimListUnusedPosition, allocation.getTrimCandidateListPosition(0u));
}

TEST(WddmAllocationTest, givenRequestedContextIdTooLargeWhenGettingTrimCandidateListPositionThenReturnUnusedPosition) {
    MockWddmAllocation allocation;
    EXPECT_EQ(trimListUnusedPosition, allocation.getTrimCandidateListPosition(1u));
    EXPECT_EQ(trimListUnusedPosition, allocation.getTrimCandidateListPosition(1000u));
}

TEST(WddmAllocationTest, givenAllocationTypeWhenPassedToWddmAllocationConstructorThenAllocationTypeIsStored) {
    WddmAllocation allocation{0, GraphicsAllocation::AllocationType::COMMAND_BUFFER, nullptr, 0, nullptr, MemoryPool::MemoryNull, 0u, 1u};
    EXPECT_EQ(GraphicsAllocation::AllocationType::COMMAND_BUFFER, allocation.getAllocationType());
}

TEST(WddmAllocationTest, givenMemoryPoolWhenPassedToWddmAllocationConstructorThenMemoryPoolIsStored) {
    WddmAllocation allocation{0, GraphicsAllocation::AllocationType::COMMAND_BUFFER, nullptr, 0, nullptr, MemoryPool::System64KBPages, 0u, 1u};
    EXPECT_EQ(MemoryPool::System64KBPages, allocation.getMemoryPool());

    WddmAllocation allocation2{0, GraphicsAllocation::AllocationType::COMMAND_BUFFER, nullptr, 0, 0u, MemoryPool::SystemCpuInaccessible, 0u, 1u};
    EXPECT_EQ(MemoryPool::SystemCpuInaccessible, allocation2.getMemoryPool());
}

TEST(WddmMemoryManagerExternalHeapTest, WhenExternalHeapIsCreatedThenItHasCorrectBase) {
    HardwareInfo *hwInfo;
    auto executionEnvironment = getExecutionEnvironmentImpl(hwInfo, 1);
    std::unique_ptr<WddmMock> wddm(static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment->rootDeviceEnvironments[0].get())));
    wddm->init();
    uint64_t base = 0x56000;
    uint64_t size = 0x9000;
    wddm->setHeap32(base, size);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->get()->setWddm(wddm.release());

    std::unique_ptr<WddmMemoryManager> memoryManager = std::unique_ptr<WddmMemoryManager>(new WddmMemoryManager(*executionEnvironment));

    EXPECT_EQ(base, memoryManager->getExternalHeapBaseAddress(0, false));
}

TEST(WddmMemoryManagerWithDeferredDeleterTest, givenWmmWhenAsyncDeleterIsEnabledAndWaitForDeletionsIsCalledThenDeleterInWddmIsSetToNullptr) {
    HardwareInfo *hwInfo;
    auto executionEnvironment = getExecutionEnvironmentImpl(hwInfo, 1);
    auto wddm = std::make_unique<WddmMock>(*executionEnvironment->rootDeviceEnvironments[0].get());
    wddm->init();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->get()->setWddm(wddm.release());
    bool actualDeleterFlag = DebugManager.flags.EnableDeferredDeleter.get();
    DebugManager.flags.EnableDeferredDeleter.set(true);
    MockWddmMemoryManager memoryManager(*executionEnvironment);
    EXPECT_NE(nullptr, memoryManager.getDeferredDeleter());
    memoryManager.waitForDeletions();
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    DebugManager.flags.EnableDeferredDeleter.set(actualDeleterFlag);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWhenAllocateGraphicsMemoryIsCalledThenMemoryPoolIsSystem4KBPages) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, *executionEnvironment));

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());
    EXPECT_TRUE(allocation->getDefaultGmm()->useSystemMemoryPool);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWith64KBPagesEnabledWhenAllocateGraphicsMemory64kbIsCalledThenMemoryPoolIsSystem64KBPages) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, *executionEnvironment));
    AllocationData allocationData;
    allocationData.size = 4096u;
    auto allocation = memoryManager->allocateGraphicsMemory64kb(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System64KBPages, allocation->getMemoryPool());
    EXPECT_TRUE(allocation->getDefaultGmm()->useSystemMemoryPool);
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
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = 4096u;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, size}, ptr);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());
    for (size_t i = 0; i < allocation->fragmentsStorage.fragmentCount; i++) {
        EXPECT_TRUE(allocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->gmm->useSystemMemoryPool);
    }
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWhenAllocate32BitGraphicsMemoryWithPtrIsCalledThenMemoryPoolIsSystem4KBPagesWith32BitGpuAddressing) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, *executionEnvironment));
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize;

    auto allocation = memoryManager->allocate32BitGraphicsMemory(csr->getRootDeviceIndex(), size, ptr, GraphicsAllocation::AllocationType::BUFFER);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPagesWith32BitGpuAddressing, allocation->getMemoryPool());
    EXPECT_TRUE(allocation->getDefaultGmm()->useSystemMemoryPool);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWith64KBPagesDisabledWhenAllocateGraphicsMemoryForSVMThen4KBGraphicsAllocationIsReturned) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, *executionEnvironment));
    auto size = MemoryConstants::pageSize;

    auto svmAllocation = memoryManager->allocateGraphicsMemoryWithProperties({csr->getRootDeviceIndex(), size, GraphicsAllocation::AllocationType::SVM_ZERO_COPY, mockDeviceBitfield});
    EXPECT_NE(nullptr, svmAllocation);
    EXPECT_EQ(MemoryPool::System4KBPages, svmAllocation->getMemoryPool());
    EXPECT_TRUE(svmAllocation->getDefaultGmm()->useSystemMemoryPool);
    memoryManager->freeGraphicsMemory(svmAllocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWith64KBPagesEnabledWhenAllocateGraphicsMemoryForSVMThenMemoryPoolIsSystem64KBPages) {
    memoryManager.reset(new MockWddmMemoryManager(true, false, *executionEnvironment));
    auto size = MemoryConstants::pageSize;

    auto svmAllocation = memoryManager->allocateGraphicsMemoryWithProperties({csr->getRootDeviceIndex(), size, GraphicsAllocation::AllocationType::SVM_ZERO_COPY, mockDeviceBitfield});
    EXPECT_NE(nullptr, svmAllocation);
    EXPECT_EQ(MemoryPool::System64KBPages, svmAllocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(svmAllocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWhenCreateAllocationFromHandleIsCalledThenMemoryPoolIsSystemCpuInaccessible) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, *executionEnvironment));
    auto osHandle = 1u;
    gdi->getQueryResourceInfoArgOut().NumAllocations = 1;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmClientContext(), nullptr, 0, false));

    D3DDDI_OPENALLOCATIONINFO allocationInfo;
    allocationInfo.pPrivateDriverData = gmm->gmmResourceInfo->peekHandle();
    allocationInfo.hAllocation = ALLOCATION_HANDLE;
    allocationInfo.PrivateDriverDataSize = sizeof(GMM_RESOURCE_INFO);

    gdi->getOpenResourceArgOut().pOpenAllocationInfo = &allocationInfo;

    AllocationProperties properties(0, false, 0, GraphicsAllocation::AllocationType::SHARED_BUFFER, false, false, 0);
    auto allocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, properties, false);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::SystemCpuInaccessible, allocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenAllocationPropertiesWhenCreateAllocationFromHandleIsCalledThenCorrectAllocationTypeIsSet) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, *executionEnvironment));
    auto osHandle = 1u;
    gdi->getQueryResourceInfoArgOut().NumAllocations = 1;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmClientContext(), nullptr, 0, false));

    D3DDDI_OPENALLOCATIONINFO allocationInfo;
    allocationInfo.pPrivateDriverData = gmm->gmmResourceInfo->peekHandle();
    allocationInfo.hAllocation = ALLOCATION_HANDLE;
    allocationInfo.PrivateDriverDataSize = sizeof(GMM_RESOURCE_INFO);

    gdi->getOpenResourceArgOut().pOpenAllocationInfo = &allocationInfo;

    AllocationProperties propertiesBuffer(0, false, 0, GraphicsAllocation::AllocationType::SHARED_BUFFER, false, false, 0);
    AllocationProperties propertiesImage(0, false, 0, GraphicsAllocation::AllocationType::SHARED_IMAGE, false, false, 0);

    AllocationProperties *propertiesArray[2] = {&propertiesBuffer, &propertiesImage};

    for (auto properties : propertiesArray) {
        auto allocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, *properties, false);
        EXPECT_NE(nullptr, allocation);
        EXPECT_EQ(properties->allocationType, allocation->getAllocationType());
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(WddmMemoryManagerSimpleTest, whenCreateAllocationFromHandleAndMapCallFailsThenFreeGraphicsMemoryIsCalled) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, *executionEnvironment));
    auto osHandle = 1u;
    gdi->getQueryResourceInfoArgOut().NumAllocations = 1;
    auto gmm = std::make_unique<Gmm>(rootDeviceEnvironment->getGmmClientContext(), nullptr, 0, false);

    D3DDDI_OPENALLOCATIONINFO allocationInfo;
    allocationInfo.pPrivateDriverData = gmm->gmmResourceInfo->peekHandle();
    allocationInfo.hAllocation = ALLOCATION_HANDLE;
    allocationInfo.PrivateDriverDataSize = sizeof(GMM_RESOURCE_INFO);
    wddm->mapGpuVaStatus = false;
    wddm->callBaseMapGpuVa = false;

    gdi->getOpenResourceArgOut().pOpenAllocationInfo = &allocationInfo;
    EXPECT_EQ(0u, memoryManager->freeGraphicsMemoryImplCalled);

    AllocationProperties properties(0, false, 0, GraphicsAllocation::AllocationType::SHARED_BUFFER, false, false, 0);

    auto allocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, properties, false);
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

TEST_F(WddmMemoryManagerSimpleTest, GivenShareableEnabledWhenAskedToCreateGrahicsAllocationThenValidAllocationIsReturned) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, *executionEnvironment));
    AllocationData allocationData;
    allocationData.size = 4096u;
    allocationData.flags.shareable = true;
    auto allocation = memoryManager->allocateShareableMemory(allocationData);
    EXPECT_NE(nullptr, allocation);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenZeroFenceValueOnSingleEngineRegisteredWhenHandleFenceCompletionIsCalledThenDoNotWaitOnCpu) {
    ASSERT_EQ(1u, memoryManager->getRegisteredEnginesCount());

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties({0, 32, GraphicsAllocation::AllocationType::BUFFER, mockDeviceBitfield}));
    allocation->getResidencyData().updateCompletionData(0u, 0u);

    memoryManager->handleFenceCompletion(allocation);
    EXPECT_EQ(0u, wddm->waitFromCpuResult.called);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenNonZeroFenceValueOnSingleEngineRegisteredWhenHandleFenceCompletionIsCalledThenWaitOnCpuOnce) {
    ASSERT_EQ(1u, memoryManager->getRegisteredEnginesCount());

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties({0, 32, GraphicsAllocation::AllocationType::BUFFER, mockDeviceBitfield}));
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
    }
    const uint32_t rootDeviceIndex = 1;
    std::unique_ptr<CommandStreamReceiver> csr(createCommandStream(*executionEnvironment, rootDeviceIndex));

    auto wddm2 = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex].get()));
    wddm2->init();
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm2);

    auto hwInfo = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
    memoryManager->createAndRegisterOsContext(csr.get(), HwHelper::get(hwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*hwInfo)[1].first,
                                              2, PreemptionHelper::getDefaultPreemptionMode(*hwInfo), false, false, false);
    ASSERT_EQ(2u, memoryManager->getRegisteredEnginesCount());

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties({0u, 32, GraphicsAllocation::AllocationType::BUFFER, mockDeviceBitfield}));
    auto lastEngineFence = &static_cast<OsContextWin *>(memoryManager->getRegisteredEngines()[1].osContext)->getResidencyController().getMonitoredFence();
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
    }
    std::unique_ptr<CommandStreamReceiver> csr(createCommandStream(*executionEnvironment, rootDeviceIndex));

    auto wddm2 = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex].get()));
    wddm2->init();
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm2);

    auto hwInfo = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
    memoryManager->createAndRegisterOsContext(csr.get(), HwHelper::get(hwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*hwInfo)[1].first,
                                              2, PreemptionHelper::getDefaultPreemptionMode(*hwInfo), false, false, false);
    ASSERT_EQ(2u, memoryManager->getRegisteredEnginesCount());

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties({0u, 32, GraphicsAllocation::AllocationType::BUFFER, mockDeviceBitfield}));
    auto lastEngineFence = &static_cast<OsContextWin *>(memoryManager->getRegisteredEngines()[0].osContext)->getResidencyController().getMonitoredFence();
    allocation->getResidencyData().updateCompletionData(129u, 0u);
    allocation->getResidencyData().updateCompletionData(0, 1u);

    memoryManager->handleFenceCompletion(allocation);
    EXPECT_EQ(1u, wddm->waitFromCpuResult.called);
    EXPECT_EQ(129, wddm->waitFromCpuResult.uint64ParamPassed);
    EXPECT_EQ(lastEngineFence, wddm->waitFromCpuResult.monitoredFence);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenWddmMemoryManagerWhenGpuAddressIsReservedAndFreedThenAddressRangeIsZero) {
    auto addressRange = memoryManager->reserveGpuAddress(MemoryConstants::pageSize, 0);
    EXPECT_EQ(0u, GmmHelper::decanonize(addressRange.address));
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

TEST_F(WddmMemoryManagerTest, givenDefaultWddmMemoryManagerWhenAskedForVirtualPaddingSupportThenFalseIsReturned) {
    EXPECT_FALSE(memoryManager->peekVirtualPaddingSupport());
}

TEST_F(WddmMemoryManagerTest, GivenGraphicsAllocationWhenAddAndRemoveAllocationToHostPtrManagerThenFragmentHasCorrectValues) {
    void *cpuPtr = (void *)0x30000;
    size_t size = 0x1000;
    uint64_t gpuPtr = 0x123;

    MockWddmAllocation gfxAllocation;
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
    EXPECT_EQ(fragment->osInternalStorage->gmm, gfxAllocation.getDefaultGmm());
    EXPECT_EQ(fragment->osInternalStorage->gpuPtr, gpuPtr);
    EXPECT_EQ(fragment->osInternalStorage->handle, gfxAllocation.handle);
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

    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmClientContext(), pSysMem, 4096u, false));
    setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    AllocationProperties properties(0, false, 4096u, GraphicsAllocation::AllocationType::SHARED_BUFFER, false, false, mockDeviceBitfield);

    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, properties, false);
    auto wddmAlloc = static_cast<WddmAllocation *>(gpuAllocation);
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(RESOURCE_HANDLE, wddmAlloc->resourceHandle);
    EXPECT_EQ(ALLOCATION_HANDLE, wddmAlloc->getDefaultHandle());

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerSimpleTest, whenAllocationCreatedFromSharedHandleIsDestroyedThenNullAllocationHandleAndZeroAllocationCountArePassedTodestroyAllocation) {
    gdi->getQueryResourceInfoArgOut().NumAllocations = 1;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmClientContext(), nullptr, 0, false));

    D3DDDI_OPENALLOCATIONINFO allocationInfo;
    allocationInfo.pPrivateDriverData = gmm->gmmResourceInfo->peekHandle();
    allocationInfo.hAllocation = ALLOCATION_HANDLE;
    allocationInfo.PrivateDriverDataSize = sizeof(GMM_RESOURCE_INFO);

    gdi->getOpenResourceArgOut().pOpenAllocationInfo = &allocationInfo;

    AllocationProperties properties(0, false, 0, GraphicsAllocation::AllocationType::SHARED_BUFFER, false, false, 0);

    auto allocation = memoryManager->createGraphicsAllocationFromSharedHandle(1, properties, false);
    EXPECT_NE(nullptr, allocation);

    memoryManager->setDeferredDeleter(nullptr);
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(1u, memoryManager->freeGraphicsMemoryImplCalled);

    auto destroyArg = gdi->getDestroyArg();
    EXPECT_EQ(nullptr, destroyArg.phAllocationList);
    EXPECT_EQ(0, destroyArg.AllocationCount);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenCreateFromNTHandleIsCalledThenNonNullGraphicsAllocationIsReturned) {
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmClientContext(), pSysMem, 4096u, false));
    setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromNTHandle(reinterpret_cast<void *>(1), 0);
    auto wddmAlloc = static_cast<WddmAllocation *>(gpuAllocation);
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(NT_RESOURCE_HANDLE, wddmAlloc->resourceHandle);
    EXPECT_EQ(NT_ALLOCATION_HANDLE, wddmAlloc->getDefaultHandle());
    EXPECT_EQ(GraphicsAllocation::AllocationType::SHARED_IMAGE, wddmAlloc->getAllocationType());

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

    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmClientContext(), pSysMem, 4096u, false));
    setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    memoryManager->setForce32BitAllocations(true);

    AllocationProperties properties(0, false, 4096u, GraphicsAllocation::AllocationType::SHARED_BUFFER, false, false, 0);

    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, properties, true);
    ASSERT_NE(nullptr, gpuAllocation);
    if (is64bit) {
        EXPECT_TRUE(gpuAllocation->is32BitAllocation());

        uint64_t base = memoryManager->getExternalHeapBaseAddress(gpuAllocation->getRootDeviceIndex(), gpuAllocation->isAllocatedInLocalMemoryPool());
        EXPECT_EQ(GmmHelper::canonize(base), gpuAllocation->getGpuBaseAddress());
    }

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, GivenForce32bitAddressingAndNotRequiredSpecificBitnessWhenCreatingAllocationFromSharedHandleThenNon32BitAllocationIsReturned) {
    auto osHandle = 1u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmClientContext(), pSysMem, 4096u, false));
    setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    memoryManager->setForce32BitAllocations(true);

    AllocationProperties properties(0, false, 4096u, GraphicsAllocation::AllocationType::SHARED_BUFFER, false, false, 0);
    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, properties, false);
    ASSERT_NE(nullptr, gpuAllocation);

    EXPECT_FALSE(gpuAllocation->is32BitAllocation());
    if (is64bit) {
        uint64_t base = 0;
        EXPECT_EQ(base, gpuAllocation->getGpuBaseAddress());
    }

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenFreeAllocFromSharedHandleIsCalledThenDestroyResourceHandle) {
    auto osHandle = 1u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmClientContext(), pSysMem, 4096u, false));
    setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    AllocationProperties properties(0, false, 4096u, GraphicsAllocation::AllocationType::SHARED_BUFFER, false, false, 0);
    auto gpuAllocation = (WddmAllocation *)memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, properties, false);
    EXPECT_NE(nullptr, gpuAllocation);
    auto expectedDestroyHandle = gpuAllocation->resourceHandle;
    EXPECT_NE(0u, expectedDestroyHandle);

    auto lastDestroyed = getMockLastDestroyedResHandleFcn();
    EXPECT_EQ(0u, lastDestroyed);

    memoryManager->freeGraphicsMemory(gpuAllocation);
    lastDestroyed = getMockLastDestroyedResHandleFcn();
    EXPECT_EQ(lastDestroyed, expectedDestroyHandle);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerSizeZeroWhenCreateFromSharedHandleIsCalledThenUpdateSize) {
    auto osHandle = 1u;
    auto size = 4096u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmClientContext(), pSysMem, size, false));
    setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    AllocationProperties properties(0, false, size, GraphicsAllocation::AllocationType::SHARED_BUFFER, false, false, 0);
    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, properties, false);
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(size, gpuAllocation->getUnderlyingBufferSize());
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenCreateFromSharedHandleFailsThenReturnNull) {
    auto osHandle = 1u;
    auto size = 4096u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmClientContext(), pSysMem, size, false));
    setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    wddm->failOpenSharedHandle = true;

    AllocationProperties properties(0, false, size, GraphicsAllocation::AllocationType::SHARED_BUFFER, false, false, 0);
    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, properties, false);
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
        Image::create(&context, MemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
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
        Image::create(&context, MemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
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
        Image::create(&context, MemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                      flags, 0, surfaceFormat, &imageDesc, data, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);

    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex());
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_EQ(GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE, imageGraphicsAllocation->getDefaultGmm()->resourceParams.Usage);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenNonTiledImgWithMipCountZeroisBeingCreatedThenAllocateGraphicsMemoryIsUsed) {
    MockContext context;
    context.memoryManager = memoryManager.get();

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 64u;

    char data[64u * 4 * 8];

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    std::unique_ptr<Image> dstImage(
        Image::create(&context, MemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                      flags, 0, surfaceFormat, &imageDesc, data, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);

    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex());
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_EQ(GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_BUFFER, imageGraphicsAllocation->getDefaultGmm()->resourceParams.Usage);
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
        Image::create(&context, MemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                      flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);
    EXPECT_EQ(static_cast<int>(imageDesc.num_mip_levels), dstImage->peekMipCount());

    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex());
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_EQ(GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE, imageGraphicsAllocation->getDefaultGmm()->resourceParams.Usage);
}

TEST_F(WddmMemoryManagerTest, GivenOffsetsWhenAllocatingGpuMemHostThenAllocatedOnlyIfInBounds) {
    MockWddmAllocation alloc, allocOffseted;
    // three pages
    void *ptr = alignedMalloc(4 * 4096, 4096);
    ASSERT_NE(nullptr, ptr);

    size_t baseOffset = 1024;
    // misalligned buffer spanning accross 3 pages
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
    MockWddmAllocation allocation;
    // three pages
    void *ptr = alignedMalloc(3 * 4096, 4096);
    auto *gpuAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, false, 3 * MemoryConstants::pageSize}, ptr);
    // Should be same cpu ptr and gpu ptr
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(ptr, gpuAllocation->getUnderlyingBuffer());

    auto fragment = memoryManager->getHostPtrManager()->getFragment({ptr, rootDeviceIndex});
    ASSERT_NE(nullptr, fragment);
    EXPECT_TRUE(fragment->refCount == 1);
    EXPECT_NE(fragment->osInternalStorage->handle, 0);
    EXPECT_NE(fragment->osInternalStorage->gmm, nullptr);
    memoryManager->freeGraphicsMemory(gpuAllocation);
    alignedFree(ptr);
}

TEST_F(WddmMemoryManagerTest, GivenAlignedPointerWhenAllocate32BitMemoryThenGmmCalledWithCorrectPointerAndSize) {
    MockWddmAllocation allocation;
    uint32_t size = 4096;
    void *ptr = reinterpret_cast<void *>(4096);
    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, size, ptr, GraphicsAllocation::AllocationType::BUFFER);
    EXPECT_EQ(ptr, reinterpret_cast<void *>(gpuAllocation->getDefaultGmm()->resourceParams.pExistingSysMem));
    EXPECT_EQ(size, gpuAllocation->getDefaultGmm()->resourceParams.ExistingSysMemSize);
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, GivenUnAlignedPointerAndSizeWhenAllocate32BitMemoryThenGmmCalledWithCorrectPointerAndSize) {
    MockWddmAllocation allocation;
    uint32_t size = 0x1001;
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, size, ptr, GraphicsAllocation::AllocationType::BUFFER);
    EXPECT_EQ(reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(gpuAllocation->getDefaultGmm()->resourceParams.pExistingSysMem));
    EXPECT_EQ(0x2000, gpuAllocation->getDefaultGmm()->resourceParams.ExistingSysMemSize);
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
    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, 3 * MemoryConstants::pageSize, nullptr, GraphicsAllocation::AllocationType::BUFFER);

    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_LT(GmmHelper::canonize(memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_EXTERNAL)), gpuAllocation->getGpuAddress());
    EXPECT_GT(GmmHelper::canonize(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::HEAP_EXTERNAL)), gpuAllocation->getGpuAddress() + gpuAllocation->getUnderlyingBufferSize());

    EXPECT_EQ(0u, gpuAllocation->fragmentsStorage.fragmentCount);
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, given32BitAllocationWhenItIsCreatedThenItHasNonZeroGpuAddressToPatch) {
    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, 3 * MemoryConstants::pageSize, nullptr, GraphicsAllocation::AllocationType::BUFFER);

    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_NE(0llu, gpuAllocation->getGpuAddressToPatch());
    EXPECT_LT(GmmHelper::canonize(memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_EXTERNAL)), gpuAllocation->getGpuAddress());
    EXPECT_GT(GmmHelper::canonize(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::HEAP_EXTERNAL)), gpuAllocation->getGpuAddress() + gpuAllocation->getUnderlyingBufferSize());
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, GivenMisalignedHostPtrWhenAllocating32BitMemoryThenTripleAllocationDoesNotOccur) {
    size_t misalignedSize = 0x2500;
    void *misalignedPtr = reinterpret_cast<void *>(0x12500);

    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, misalignedSize, misalignedPtr, GraphicsAllocation::AllocationType::BUFFER);

    ASSERT_NE(nullptr, gpuAllocation);

    EXPECT_EQ(alignSizeWholePage(misalignedPtr, misalignedSize), gpuAllocation->getUnderlyingBufferSize());

    EXPECT_LT(GmmHelper::canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::HEAP_EXTERNAL)), gpuAllocation->getGpuAddress());
    EXPECT_GT(GmmHelper::canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::HEAP_EXTERNAL)), gpuAllocation->getGpuAddress() + gpuAllocation->getUnderlyingBufferSize());

    EXPECT_EQ(0u, gpuAllocation->fragmentsStorage.fragmentCount);

    void *alignedPtr = alignDown(misalignedPtr, MemoryConstants::allocationAlignment);
    uint64_t offset = ptrDiff(misalignedPtr, alignedPtr);

    EXPECT_EQ(offset, gpuAllocation->getAllocationOffset());
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, WhenAllocating32BitMemoryThenGpuBaseAddressIsCannonized) {
    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, 3 * MemoryConstants::pageSize, nullptr, GraphicsAllocation::AllocationType::BUFFER);

    ASSERT_NE(nullptr, gpuAllocation);

    uint64_t cannonizedAddress = GmmHelper::canonize(memoryManager->getGfxPartition(0)->getHeapBase(MemoryManager::selectExternalHeap(gpuAllocation->isAllocatedInLocalMemoryPool())));
    EXPECT_EQ(cannonizedAddress, gpuAllocation->getGpuBaseAddress());

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, GivenThreeOsHandlesWhenAskedForDestroyAllocationsThenAllMarkedAllocationsAreDestroyed) {
    OsHandleStorage storage;
    void *pSysMem = reinterpret_cast<void *>(0x1000);
    uint32_t maxOsContextCount = 1u;

    storage.fragmentStorageData[0].osHandleStorage = new OsHandle;
    storage.fragmentStorageData[0].residency = new ResidencyData(maxOsContextCount);

    storage.fragmentStorageData[0].osHandleStorage->handle = ALLOCATION_HANDLE;
    storage.fragmentStorageData[0].freeTheFragment = true;
    storage.fragmentStorageData[0].osHandleStorage->gmm = new Gmm(rootDeviceEnvironment->getGmmClientContext(), pSysMem, 4096u, false);

    storage.fragmentStorageData[1].osHandleStorage = new OsHandle;
    storage.fragmentStorageData[1].osHandleStorage->handle = ALLOCATION_HANDLE;
    storage.fragmentStorageData[1].residency = new ResidencyData(maxOsContextCount);

    storage.fragmentStorageData[1].freeTheFragment = false;

    storage.fragmentStorageData[2].osHandleStorage = new OsHandle;
    storage.fragmentStorageData[2].osHandleStorage->handle = ALLOCATION_HANDLE;
    storage.fragmentStorageData[2].freeTheFragment = true;
    storage.fragmentStorageData[2].osHandleStorage->gmm = new Gmm(rootDeviceEnvironment->getGmmClientContext(), pSysMem, 4096u, false);
    storage.fragmentStorageData[2].residency = new ResidencyData(maxOsContextCount);

    memoryManager->cleanOsHandles(storage, 0);

    auto destroyWithResourceHandleCalled = 0u;
    D3DKMT_DESTROYALLOCATION2 *ptrToDestroyAlloc2 = nullptr;

    getSizesFcn(destroyWithResourceHandleCalled, ptrToDestroyAlloc2);

    EXPECT_EQ(0u, ptrToDestroyAlloc2->Flags.SynchronousDestroy);
    EXPECT_EQ(1u, ptrToDestroyAlloc2->Flags.AssumeNotInUse);

    EXPECT_EQ(ALLOCATION_HANDLE, storage.fragmentStorageData[1].osHandleStorage->handle);

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
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmClientContext(), ptr, size, false));

    memoryManager->setDeferredDeleter(nullptr);
    setMapGpuVaFailConfigFcn(0, 1);

    WddmAllocation allocation(0, GraphicsAllocation::AllocationType::BUFFER, ptr, size, nullptr, MemoryPool::MemoryNull, 0u, 1u);
    allocation.setDefaultGmm(gmm.get());
    bool ret = memoryManager->createWddmAllocation(&allocation, allocation.getAlignedCpuPtr());
    EXPECT_FALSE(ret);
}

TEST_F(WddmMemoryManagerTest, givenManagerWithEnabledDeferredDeleterWhenFirstMapGpuVaFailSecondAfterDrainSuccessThenCreateAllocation) {
    void *ptr = reinterpret_cast<void *>(0x10000);
    size_t size = 0x1000;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmClientContext(), ptr, size, false));

    MockDeferredDeleter *deleter = new MockDeferredDeleter;
    memoryManager->setDeferredDeleter(deleter);

    setMapGpuVaFailConfigFcn(0, 1);

    WddmAllocation allocation(0, GraphicsAllocation::AllocationType::BUFFER, ptr, size, nullptr, MemoryPool::MemoryNull, 0u, 1u);
    allocation.setDefaultGmm(gmm.get());
    bool ret = memoryManager->createWddmAllocation(&allocation, allocation.getAlignedCpuPtr());
    EXPECT_TRUE(ret);
}

TEST_F(WddmMemoryManagerTest, givenManagerWithEnabledDeferredDeleterWhenFirstAndMapGpuVaFailSecondAfterDrainFailThenFailToCreateAllocation) {
    void *ptr = reinterpret_cast<void *>(0x1000);
    size_t size = 0x1000;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmClientContext(), ptr, size, false));

    MockDeferredDeleter *deleter = new MockDeferredDeleter;
    memoryManager->setDeferredDeleter(deleter);

    setMapGpuVaFailConfigFcn(0, 2);

    WddmAllocation allocation(0, GraphicsAllocation::AllocationType::BUFFER, ptr, size, nullptr, MemoryPool::MemoryNull, 0u, 1u);
    allocation.setDefaultGmm(gmm.get());
    bool ret = memoryManager->createWddmAllocation(&allocation, allocation.getAlignedCpuPtr());
    EXPECT_FALSE(ret);
}

TEST_F(WddmMemoryManagerTest, givenNullPtrAndSizePassedToCreateInternalAllocationWhenCallIsMadeThenAllocationIsCreatedIn32BitHeapInternal) {
    auto wddmAllocation = static_cast<WddmAllocation *>(memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, MemoryConstants::pageSize, nullptr, GraphicsAllocation::AllocationType::INTERNAL_HEAP));
    ASSERT_NE(nullptr, wddmAllocation);
    EXPECT_EQ(wddmAllocation->getGpuBaseAddress(), GmmHelper::canonize(memoryManager->getInternalHeapBaseAddress(wddmAllocation->getRootDeviceIndex(), wddmAllocation->isAllocatedInLocalMemoryPool())));
    EXPECT_NE(nullptr, wddmAllocation->getUnderlyingBuffer());
    EXPECT_EQ(4096u, wddmAllocation->getUnderlyingBufferSize());
    EXPECT_NE((uint64_t)wddmAllocation->getUnderlyingBuffer(), wddmAllocation->getGpuAddress());
    auto cannonizedHeapBase = GmmHelper::canonize(memoryManager->getInternalHeapBaseAddress(rootDeviceIndex, wddmAllocation->isAllocatedInLocalMemoryPool()));
    auto cannonizedHeapEnd = GmmHelper::canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(MemoryManager::selectInternalHeap(wddmAllocation->isAllocatedInLocalMemoryPool())));

    EXPECT_GT(wddmAllocation->getGpuAddress(), cannonizedHeapBase);
    EXPECT_LT(wddmAllocation->getGpuAddress() + wddmAllocation->getUnderlyingBufferSize(), cannonizedHeapEnd);

    EXPECT_NE(nullptr, wddmAllocation->getDriverAllocatedCpuPtr());
    EXPECT_TRUE(wddmAllocation->is32BitAllocation());
    memoryManager->freeGraphicsMemory(wddmAllocation);
}

TEST_F(WddmMemoryManagerTest, givenPtrAndSizePassedToCreateInternalAllocationWhenCallIsMadeThenAllocationIsCreatedIn32BitHeapInternal) {
    auto ptr = reinterpret_cast<void *>(0x1000000);
    auto wddmAllocation = static_cast<WddmAllocation *>(memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, MemoryConstants::pageSize, ptr, GraphicsAllocation::AllocationType::INTERNAL_HEAP));
    ASSERT_NE(nullptr, wddmAllocation);
    EXPECT_EQ(wddmAllocation->getGpuBaseAddress(), GmmHelper::canonize(memoryManager->getInternalHeapBaseAddress(rootDeviceIndex, wddmAllocation->isAllocatedInLocalMemoryPool())));
    EXPECT_EQ(ptr, wddmAllocation->getUnderlyingBuffer());
    EXPECT_EQ(4096u, wddmAllocation->getUnderlyingBufferSize());
    EXPECT_NE((uint64_t)wddmAllocation->getUnderlyingBuffer(), wddmAllocation->getGpuAddress());
    auto cannonizedHeapBase = GmmHelper::canonize(memoryManager->getInternalHeapBaseAddress(rootDeviceIndex, wddmAllocation->isAllocatedInLocalMemoryPool()));
    auto cannonizedHeapEnd = GmmHelper::canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(MemoryManager::selectInternalHeap(wddmAllocation->isAllocatedInLocalMemoryPool())));

    EXPECT_GT(wddmAllocation->getGpuAddress(), cannonizedHeapBase);
    EXPECT_LT(wddmAllocation->getGpuAddress() + wddmAllocation->getUnderlyingBufferSize(), cannonizedHeapEnd);

    EXPECT_EQ(nullptr, wddmAllocation->getDriverAllocatedCpuPtr());
    EXPECT_TRUE(wddmAllocation->is32BitAllocation());
    memoryManager->freeGraphicsMemory(wddmAllocation);
}

TEST_F(BufferWithWddmMemory, WhenCreatingBufferThenBufferIsCreatedCorrectly) {
    flags = CL_MEM_USE_HOST_PTR | CL_MEM_FORCE_SHARED_PHYSICAL_MEMORY_INTEL;

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
    EXPECT_NE(nullptr, storage.fragmentStorageData[0].osHandleStorage->gmm);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[2].osHandleStorage);
    storage.fragmentStorageData[0].freeTheFragment = true;
    memoryManager->cleanOsHandles(storage, 0);
}

TEST_F(BufferWithWddmMemory, GivenMisalignedHostPtrAndMultiplePagesSizeWhenAskedForGraphicsAllocationThenItContainsAllFragmentsWithProperGpuAdrresses) {
    auto ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1001);
    auto size = MemoryConstants::pageSize * 10;
    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{context.getDevice(0)->getRootDeviceIndex(), false, size, context.getDevice(0)->getDeviceBitfield()}, ptr);

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());

    ASSERT_EQ(3u, hostPtrManager->getFragmentCount());

    auto reqs = MockHostPtrManager::getAllocationRequirements(context.getDevice(0)->getRootDeviceIndex(), ptr, size);

    for (int i = 0; i < maxFragmentsCount; i++) {
        EXPECT_NE((D3DKMT_HANDLE) nullptr, graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->handle);

        EXPECT_NE(nullptr, graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->gmm);
        EXPECT_EQ(reqs.allocationFragments[i].allocationPtr,
                  reinterpret_cast<void *>(graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->gmm->resourceParams.pExistingSysMem));
        EXPECT_EQ(reqs.allocationFragments[i].allocationSize,
                  graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->gmm->resourceParams.BaseWidth);
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

    handleStorage.fragmentStorageData[0].cpuPtr = ptr;
    handleStorage.fragmentStorageData[0].fragmentSize = size;
    handleStorage.fragmentStorageData[0].osHandleStorage = new OsHandle();
    handleStorage.fragmentStorageData[0].residency = new ResidencyData(maxOsContextCount);
    handleStorage.fragmentStorageData[0].freeTheFragment = true;
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    handleStorage.fragmentStorageData[0].osHandleStorage->gmm = new Gmm(rootDeviceEnvironment->getGmmClientContext(), ptr, size, false);
    handleStorage.fragmentCount = 1;

    FragmentStorage fragment = {};
    fragment.driverAllocation = true;
    fragment.fragmentCpuPointer = ptr;
    fragment.fragmentSize = size;
    fragment.osInternalStorage = handleStorage.fragmentStorageData[0].osHandleStorage;
    fragment.osInternalStorage->gpuPtr = gpuAdress;
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

    handleStorage.fragmentStorageData[0].cpuPtr = ptr;
    handleStorage.fragmentStorageData[0].fragmentSize = size;
    handleStorage.fragmentStorageData[0].osHandleStorage = new OsHandle();
    handleStorage.fragmentStorageData[0].residency = new ResidencyData(maxOsContextCount);
    handleStorage.fragmentStorageData[0].freeTheFragment = true;
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    handleStorage.fragmentStorageData[0].osHandleStorage->gmm = new Gmm(rootDeviceEnvironment->getGmmClientContext(), ptr, size, false);
    handleStorage.fragmentCount = 1;

    FragmentStorage fragment = {};
    fragment.driverAllocation = true;
    fragment.fragmentCpuPointer = ptr;
    fragment.fragmentSize = size;
    fragment.osInternalStorage = handleStorage.fragmentStorageData[0].osHandleStorage;
    fragment.osInternalStorage->gpuPtr = gpuAdress;
    memoryManager->getHostPtrManager()->storeFragment(rootDeviceIndex, fragment);

    auto offset = 80;
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
    void SetUp() {
        executionEnvironment = getExecutionEnvironmentImpl(hwInfo, 1);
        wddm = static_cast<WddmMock *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->get()->getWddm());
        wddm->resetGdi(new MockGdi());
        wddm->callBaseDestroyAllocations = false;
        wddm->init();
        deleter = new MockDeferredDeleter;
        memoryManager = std::make_unique<MockWddmMemoryManager>(*executionEnvironment);
        memoryManager->setDeferredDeleter(deleter);
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
    cl_image_desc imgDesc = {};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    ImageInfo imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    wddm->createAllocationStatus = STATUS_GRAPHICS_NO_VIDEO_MEMORY;
    EXPECT_EQ(0, deleter->drainCalled);
    EXPECT_EQ(0u, wddm->createAllocationResult.called);
    deleter->expectDrainBlockingValue(true);
    AllocationProperties allocProperties = MemObjHelper::getAllocationPropertiesWithImageInfo(0, imgInfo, true, {}, *hwInfo, mockDeviceBitfield);

    memoryManager->allocateGraphicsMemoryInPreferredPool(allocProperties, nullptr);
    EXPECT_EQ(1, deleter->drainCalled);
    EXPECT_EQ(2u, wddm->createAllocationResult.called);
}

TEST_F(WddmMemoryManagerWithAsyncDeleterTest, givenMemoryManagerWithAsyncDeleterWhenCanAllocateMemoryForTiledImageThenDrainIsNotCalledAndCreateAllocationIsCalledOnce) {
    cl_image_desc imgDesc;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    ImageInfo imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    wddm->createAllocationStatus = STATUS_SUCCESS;
    wddm->mapGpuVaStatus = true;
    wddm->callBaseMapGpuVa = false;
    EXPECT_EQ(0, deleter->drainCalled);
    EXPECT_EQ(0u, wddm->createAllocationResult.called);
    EXPECT_EQ(0u, wddm->mapGpuVirtualAddressResult.called);

    AllocationProperties allocProperties = MemObjHelper::getAllocationPropertiesWithImageInfo(0, imgInfo, true, {}, *hwInfo, mockDeviceBitfield);

    auto allocation = memoryManager->allocateGraphicsMemoryInPreferredPool(allocProperties, nullptr);
    EXPECT_EQ(0, deleter->drainCalled);
    EXPECT_EQ(1u, wddm->createAllocationResult.called);
    EXPECT_EQ(1u, wddm->mapGpuVirtualAddressResult.called);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerWithAsyncDeleterTest, givenMemoryManagerWithoutAsyncDeleterWhenCannotAllocateMemoryForTiledImageThenCreateAllocationIsCalledOnce) {
    memoryManager->setDeferredDeleter(nullptr);
    cl_image_desc imgDesc;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    ImageInfo imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    wddm->createAllocationStatus = STATUS_GRAPHICS_NO_VIDEO_MEMORY;
    EXPECT_EQ(0u, wddm->createAllocationResult.called);

    AllocationProperties allocProperties = MemObjHelper::getAllocationPropertiesWithImageInfo(0, imgInfo, true, {}, *hwInfo, mockDeviceBitfield);

    memoryManager->allocateGraphicsMemoryInPreferredPool(allocProperties, nullptr);
    EXPECT_EQ(1u, wddm->createAllocationResult.called);
}

TEST(WddmMemoryManagerDefaults, givenDefaultWddmMemoryManagerWhenItIsQueriedForInternalHeapBaseThenHeapInternalBaseIsReturned) {
    HardwareInfo *hwInfo;
    auto executionEnvironment = getExecutionEnvironmentImpl(hwInfo, 1);
    auto wddm = new WddmMock(*executionEnvironment->rootDeviceEnvironments[0].get());
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->get()->setWddm(wddm);
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
    wddm->init();
    MockWddmMemoryManager memoryManager(*executionEnvironment);
    auto heapBase = wddm->getGfxPartition().Heap32[static_cast<uint32_t>(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY)].Base;
    EXPECT_EQ(heapBase, memoryManager.getInternalHeapBaseAddress(0, true));
}

TEST_F(MockWddmMemoryManagerTest, givenValidateAllocationFunctionWhenItIsCalledWithTripleAllocationThenSuccessIsReturned) {
    wddm->init();
    MockWddmMemoryManager memoryManager(*executionEnvironment);

    auto wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, false, MemoryConstants::pageSize}, reinterpret_cast<void *>(0x1000)));

    EXPECT_TRUE(memoryManager.validateAllocationMock(wddmAlloc));

    memoryManager.freeGraphicsMemory(wddmAlloc);
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

TEST_F(MockWddmMemoryManagerTest, givenEnabled64kbpagesWhenCreatingGraphicsMemoryForBufferWithoutHostPtrThen64kbAdressIsAllocated) {
    DebugManagerStateRestore dbgRestore;
    wddm->init();
    DebugManager.flags.Enable64kbpages.set(true);
    MemoryManagerCreate<WddmMemoryManager> memoryManager64k(true, false, *executionEnvironment);
    EXPECT_EQ(0, wddm->createAllocationResult.called);

    GraphicsAllocation *galloc = memoryManager64k.allocateGraphicsMemoryWithProperties({rootDeviceIndex, MemoryConstants::pageSize64k, GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, mockDeviceBitfield});
    EXPECT_EQ(1, wddm->createAllocationResult.called);
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

    DebugManager.flags.Enable64kbpages.set(true);
    MemoryManagerCreate<MockWddmMemoryManager> memoryManager(true, false, *executionEnvironment);
    EXPECT_EQ(0, wddm->createAllocationResult.called);

    memoryManager.hugeGfxMemoryChunkSize = MemoryConstants::pageSize64k - MemoryConstants::pageSize;

    WddmAllocation *wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemoryWithProperties({rootDeviceIndex, MemoryConstants::pageSize64k * 3, GraphicsAllocation::AllocationType::BUFFER, mockDeviceBitfield}));
    EXPECT_NE(nullptr, wddmAlloc);
    EXPECT_EQ(4, wddmAlloc->getNumGmms());
    EXPECT_EQ(4, wddm->createAllocationResult.called);
    EXPECT_EQ(wddmAlloc->getGpuAddressToModify(), GmmHelper::canonize(wddmAlloc->reservedGpuVirtualAddress));

    memoryManager.freeGraphicsMemory(wddmAlloc);
}

TEST_F(MockWddmMemoryManagerTest, givenDefaultMemoryManagerWhenItIsCreatedThenCorrectHugeGfxMemoryChunkIsSet) {
    MockWddmMemoryManager memoryManager(*executionEnvironment);
    EXPECT_EQ(memoryManager.getHugeGfxMemoryChunkSize(), 4 * MemoryConstants::gigaByte - MemoryConstants::pageSize64k);
}

TEST_F(MockWddmMemoryManagerTest, givenAllocateGraphicsMemoryForHostBufferAndRequestedSizeIsHugeThenResultAllocationIsSplitted) {
    DebugManagerStateRestore dbgRestore;

    wddm->init();
    wddm->mapGpuVaStatus = true;
    VariableBackup<bool> restorer{&wddm->callBaseMapGpuVa, false};

    DebugManager.flags.Enable64kbpages.set(true);
    MemoryManagerCreate<MockWddmMemoryManager> memoryManager(true, false, *executionEnvironment);
    EXPECT_EQ(0, wddm->createAllocationResult.called);

    memoryManager.hugeGfxMemoryChunkSize = MemoryConstants::pageSize64k - MemoryConstants::pageSize;

    std::vector<uint8_t> hostPtr(MemoryConstants::pageSize64k * 3);
    AllocationProperties allocProps{rootDeviceIndex, MemoryConstants::pageSize64k * 3, GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, mockDeviceBitfield};
    allocProps.flags.allocateMemory = false;
    WddmAllocation *wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemoryWithProperties(allocProps, hostPtr.data()));

    EXPECT_NE(nullptr, wddmAlloc);
    EXPECT_EQ(4, wddmAlloc->getNumGmms());
    EXPECT_EQ(4, wddm->createAllocationResult.called);
    EXPECT_EQ(wddmAlloc->getGpuAddressToModify(), GmmHelper::canonize(wddmAlloc->reservedGpuVirtualAddress));

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
    }
    executionEnvironment->initializeMemoryManager();
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->osInterface.reset();
        auto wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment->rootDeviceEnvironments[i].get()));
        wddm->init();
        executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
    }
    std::unique_ptr<CommandStreamReceiver> csr(createCommandStream(*executionEnvironment, 0u));
    std::unique_ptr<CommandStreamReceiver> csr1(createCommandStream(*executionEnvironment, 1u));
    std::unique_ptr<CommandStreamReceiver> csr2(createCommandStream(*executionEnvironment, 2u));
    memoryManager->createAndRegisterOsContext(csr.get(), aub_stream::ENGINE_RCS, 1,
                                              PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo), false, false, false);
    memoryManager->createAndRegisterOsContext(csr1.get(), aub_stream::ENGINE_RCS, 2,
                                              PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo), false, false, false);
    memoryManager->createAndRegisterOsContext(csr2.get(), aub_stream::ENGINE_RCS, 3,
                                              PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo), false, false, false);
    EXPECT_FALSE(memoryManager->isMemoryBudgetExhausted());
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWithRegisteredOsContextWithExhaustedMemoryBudgetWhenCallingIsMemoryBudgetExhaustedThenReturnTrue) {
    executionEnvironment->prepareRootDeviceEnvironments(3u);
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
    }
    executionEnvironment->initializeMemoryManager();
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->osInterface.reset();
        auto wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment->rootDeviceEnvironments[i].get()));
        wddm->init();
        executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
    }
    std::unique_ptr<CommandStreamReceiver> csr(createCommandStream(*executionEnvironment, 0u));
    std::unique_ptr<CommandStreamReceiver> csr1(createCommandStream(*executionEnvironment, 1u));
    std::unique_ptr<CommandStreamReceiver> csr2(createCommandStream(*executionEnvironment, 2u));
    memoryManager->createAndRegisterOsContext(csr.get(), aub_stream::ENGINE_RCS, 1,
                                              PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo), false, false, false);
    memoryManager->createAndRegisterOsContext(csr1.get(), aub_stream::ENGINE_RCS, 2,
                                              PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo), false, false, false);
    memoryManager->createAndRegisterOsContext(csr2.get(), aub_stream::ENGINE_RCS, 3,
                                              PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo), false, false, false);
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
    wddm->init();
    WddmMemoryManager memoryManager(*executionEnvironment);

    auto mockMngr = new NiceMock<MockGmmPageTableMngr>();
    executionEnvironment->rootDeviceEnvironments[1]->pageTableManager.reset(mockMngr);

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(AllocationProperties(1, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY, mockDeviceBitfield));

    GMM_DDI_UPDATEAUXTABLE givenDdiUpdateAuxTable = {};
    GMM_DDI_UPDATEAUXTABLE expectedDdiUpdateAuxTable = {};
    expectedDdiUpdateAuxTable.BaseGpuVA = allocation->getGpuAddress();
    expectedDdiUpdateAuxTable.BaseResInfo = allocation->getDefaultGmm()->gmmResourceInfo->peekHandle();
    expectedDdiUpdateAuxTable.DoNotWait = true;
    expectedDdiUpdateAuxTable.Map = true;

    EXPECT_CALL(*mockMngr, updateAuxTable(_)).Times(1).WillOnce(Invoke([&](const GMM_DDI_UPDATEAUXTABLE *arg) {givenDdiUpdateAuxTable = *arg; return GMM_SUCCESS; }));

    auto result = memoryManager.mapAuxGpuVA(allocation);
    EXPECT_TRUE(result);
    EXPECT_TRUE(memcmp(&expectedDdiUpdateAuxTable, &givenDdiUpdateAuxTable, sizeof(GMM_DDI_UPDATEAUXTABLE)) == 0);
    memoryManager.freeGraphicsMemory(allocation);
}

TEST_F(MockWddmMemoryManagerTest, givenRenderCompressedAllocationWhenMappedGpuVaThenMapAuxVa) {
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[1].get();
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmClientContext(), reinterpret_cast<void *>(123), 4096u, false));
    gmm->isRenderCompressed = true;
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    WddmMock wddm(*executionEnvironment->rootDeviceEnvironments[1].get());
    wddm.init();

    auto mockMngr = new NiceMock<MockGmmPageTableMngr>();
    rootDeviceEnvironment->pageTableManager.reset(mockMngr);

    GMM_DDI_UPDATEAUXTABLE givenDdiUpdateAuxTable = {};
    GMM_DDI_UPDATEAUXTABLE expectedDdiUpdateAuxTable = {};
    expectedDdiUpdateAuxTable.BaseGpuVA = GmmHelper::canonize(wddm.getGfxPartition().Standard.Base);
    expectedDdiUpdateAuxTable.BaseResInfo = gmm->gmmResourceInfo->peekHandle();
    expectedDdiUpdateAuxTable.DoNotWait = true;
    expectedDdiUpdateAuxTable.Map = true;

    EXPECT_CALL(*mockMngr, updateAuxTable(_)).Times(1).WillOnce(Invoke([&](const GMM_DDI_UPDATEAUXTABLE *arg) {givenDdiUpdateAuxTable = *arg; return GMM_SUCCESS; }));

    auto hwInfoMock = hardwareInfoTable[wddm.getGfxPlatform()->eProductFamily];
    ASSERT_NE(nullptr, hwInfoMock);
    auto result = wddm.mapGpuVirtualAddress(gmm.get(), ALLOCATION_HANDLE, wddm.getGfxPartition().Standard.Base, wddm.getGfxPartition().Standard.Limit, 0u, gpuVa);
    ASSERT_TRUE(result);

    EXPECT_EQ(GmmHelper::canonize(wddm.getGfxPartition().Standard.Base), gpuVa);
    EXPECT_TRUE(memcmp(&expectedDdiUpdateAuxTable, &givenDdiUpdateAuxTable, sizeof(GMM_DDI_UPDATEAUXTABLE)) == 0);
}

TEST_F(MockWddmMemoryManagerTest, givenRenderCompressedAllocationWhenReleaseingThenUnmapAuxVa) {
    wddm->init();
    WddmMemoryManager memoryManager(*executionEnvironment);
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 123;

    auto mockMngr = new NiceMock<MockGmmPageTableMngr>();
    executionEnvironment->rootDeviceEnvironments[1]->pageTableManager.reset(mockMngr);

    auto wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemoryWithProperties(AllocationProperties(1, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY, mockDeviceBitfield)));
    wddmAlloc->setGpuAddress(gpuVa);
    wddmAlloc->getDefaultGmm()->isRenderCompressed = true;

    GMM_DDI_UPDATEAUXTABLE givenDdiUpdateAuxTable = {};
    GMM_DDI_UPDATEAUXTABLE expectedDdiUpdateAuxTable = {};
    expectedDdiUpdateAuxTable.BaseGpuVA = gpuVa;
    expectedDdiUpdateAuxTable.BaseResInfo = wddmAlloc->getDefaultGmm()->gmmResourceInfo->peekHandle();
    expectedDdiUpdateAuxTable.DoNotWait = true;
    expectedDdiUpdateAuxTable.Map = false;

    EXPECT_CALL(*mockMngr, updateAuxTable(_)).Times(1).WillOnce(Invoke([&](const GMM_DDI_UPDATEAUXTABLE *arg) {givenDdiUpdateAuxTable = *arg; return GMM_SUCCESS; }));
    memoryManager.freeGraphicsMemory(wddmAlloc);

    EXPECT_TRUE(memcmp(&expectedDdiUpdateAuxTable, &givenDdiUpdateAuxTable, sizeof(GMM_DDI_UPDATEAUXTABLE)) == 0);
}

TEST_F(MockWddmMemoryManagerTest, givenNonRenderCompressedAllocationWhenReleaseingThenDontUnmapAuxVa) {
    wddm->init();
    WddmMemoryManager memoryManager(*executionEnvironment);

    auto mockMngr = new NiceMock<MockGmmPageTableMngr>();
    executionEnvironment->rootDeviceEnvironments[1]->pageTableManager.reset(mockMngr);

    auto wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize}));
    wddmAlloc->getDefaultGmm()->isRenderCompressed = false;

    EXPECT_CALL(*mockMngr, updateAuxTable(_)).Times(0);

    memoryManager.freeGraphicsMemory(wddmAlloc);
}

TEST_F(MockWddmMemoryManagerTest, givenNonRenderCompressedAllocationWhenMappedGpuVaThenDontMapAuxVa) {
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[1].get();
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmClientContext(), reinterpret_cast<void *>(123), 4096u, false));
    gmm->isRenderCompressed = false;
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    WddmMock wddm(*rootDeviceEnvironment);
    wddm.init();

    auto mockMngr = new NiceMock<MockGmmPageTableMngr>();
    rootDeviceEnvironment->pageTableManager.reset(mockMngr);

    EXPECT_CALL(*mockMngr, updateAuxTable(_)).Times(0);

    auto result = wddm.mapGpuVirtualAddress(gmm.get(), ALLOCATION_HANDLE, wddm.getGfxPartition().Standard.Base, wddm.getGfxPartition().Standard.Limit, 0u, gpuVa);
    ASSERT_TRUE(result);
}

TEST_F(MockWddmMemoryManagerTest, givenFailingAllocationWhenMappedGpuVaThenReturnFalse) {
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[1].get();
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmClientContext(), reinterpret_cast<void *>(123), 4096u, false));
    gmm->isRenderCompressed = false;
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    WddmMock wddm(*rootDeviceEnvironment);
    wddm.init();

    auto result = wddm.mapGpuVirtualAddress(gmm.get(), 0, 0, 0, 0, gpuVa);
    ASSERT_FALSE(result);
}

TEST_F(MockWddmMemoryManagerTest, givenRenderCompressedFlagSetWhenInternalIsUnsetThenDontUpdateAuxTable) {
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    wddm->init();
    WddmMemoryManager memoryManager(*executionEnvironment);

    auto mockMngr = new NiceMock<MockGmmPageTableMngr>();
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[1].get();
    rootDeviceEnvironment->pageTableManager.reset(mockMngr);

    auto myGmm = new Gmm(rootDeviceEnvironment->getGmmClientContext(), reinterpret_cast<void *>(123), 4096u, false);
    myGmm->isRenderCompressed = false;
    myGmm->gmmResourceInfo->getResourceFlags()->Info.RenderCompressed = 1;

    auto wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize}));
    delete wddmAlloc->getDefaultGmm();
    wddmAlloc->setDefaultGmm(myGmm);

    EXPECT_CALL(*mockMngr, updateAuxTable(_)).Times(0);

    auto result = wddm->mapGpuVirtualAddress(myGmm, ALLOCATION_HANDLE, wddm->getGfxPartition().Standard.Base, wddm->getGfxPartition().Standard.Limit, 0u, gpuVa);
    EXPECT_TRUE(result);
    memoryManager.freeGraphicsMemory(wddmAlloc);
}

TEST_F(MockWddmMemoryManagerTest, givenRenderCompressedFlagSetWhenInternalIsSetThenUpdateAuxTable) {
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    wddm->init();
    WddmMemoryManager memoryManager(*executionEnvironment);

    auto mockMngr = new NiceMock<MockGmmPageTableMngr>();
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[1].get();
    rootDeviceEnvironment->pageTableManager.reset(mockMngr);

    auto myGmm = new Gmm(rootDeviceEnvironment->getGmmClientContext(), reinterpret_cast<void *>(123), 4096u, false);
    myGmm->isRenderCompressed = true;
    myGmm->gmmResourceInfo->getResourceFlags()->Info.RenderCompressed = 1;

    auto wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize}));
    delete wddmAlloc->getDefaultGmm();
    wddmAlloc->setDefaultGmm(myGmm);

    EXPECT_CALL(*mockMngr, updateAuxTable(_)).Times(1);

    auto result = wddm->mapGpuVirtualAddress(myGmm, ALLOCATION_HANDLE, wddm->getGfxPartition().Standard.Base, wddm->getGfxPartition().Standard.Limit, 0u, gpuVa);
    EXPECT_TRUE(result);
    memoryManager.freeGraphicsMemory(wddmAlloc);
}

TEST_F(WddmMemoryManagerTest2, givenReadOnlyMemoryWhenCreateAllocationFailsThenPopulateOsHandlesReturnsInvalidPointer) {
    OsHandleStorage handleStorage;
    handleStorage.fragmentCount = 1;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 0x1000;
    handleStorage.fragmentStorageData[0].freeTheFragment = false;

    EXPECT_CALL(*wddm, createAllocationsAndMapGpuVa(::testing::_)).WillOnce(::testing::Return(STATUS_GRAPHICS_NO_VIDEO_MEMORY));

    auto result = memoryManager->populateOsHandles(handleStorage, 0);

    EXPECT_EQ(MemoryManager::AllocationStatus::InvalidHostPointer, result);
    handleStorage.fragmentStorageData[0].freeTheFragment = true;
    memoryManager->cleanOsHandles(handleStorage, 0);
}

TEST_F(WddmMemoryManagerTest2, givenReadOnlyMemoryPassedToPopulateOsHandlesWhenCreateAllocationFailsThenAllocatedFragmentsAreNotStored) {
    OsHandleStorage handleStorage;
    OsHandle handle;
    handleStorage.fragmentCount = 2;
    handleStorage.fragmentStorageData[0].osHandleStorage = &handle;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 0x1000;

    handleStorage.fragmentStorageData[1].cpuPtr = reinterpret_cast<void *>(0x2000);
    handleStorage.fragmentStorageData[1].fragmentSize = 0x6000;

    EXPECT_CALL(*wddm, createAllocationsAndMapGpuVa(::testing::_)).WillOnce(::testing::Return(STATUS_GRAPHICS_NO_VIDEO_MEMORY));

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
    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(executionEnvironment, 0));
    auto wddm = new WddmMock(*executionEnvironment.rootDeviceEnvironments[0].get());
    auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo);
    wddm->init();

    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
    executionEnvironment.memoryManager = std::make_unique<WddmMemoryManager>(executionEnvironment);
    auto osContext = executionEnvironment.memoryManager->createAndRegisterOsContext(csr.get(), aub_stream::ENGINE_RCS, 1, preemptionMode,
                                                                                    false, false, false);
    csr->setupContext(*osContext);

    auto tagAllocator = csr->getEventPerfCountAllocator(100);
    auto allocation = tagAllocator->getTag()->getBaseGraphicsAllocation();
    allocation->updateTaskCount(1, csr->getOsContext().getContextId());
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
    MockWddmAllocation allocation;
    allocation.setAllocationType(GraphicsAllocation::AllocationType::KERNEL_ISA);
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
    MockWddmAllocation allocation(numGmms);
    allocation.setAllocationType(GraphicsAllocation::AllocationType::BUFFER);
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
    size_t size = 2 * MemoryConstants::megaByte;
    MockAllocationProperties properties{csr->getRootDeviceIndex(), true, size, GraphicsAllocation::AllocationType::SVM_CPU, mockDeviceBitfield};
    properties.alignment = size;
    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(properties));
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(allocation->getUnderlyingBuffer(), allocation->getDriverAllocatedCpuPtr());
    //limited platforms will not use heap HeapIndex::HEAP_SVM
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
    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties({0, size, GraphicsAllocation::AllocationType::WRITE_COMBINED, mockDeviceBitfield}));
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

TEST_F(WddmMemoryManagerSimpleTest, givenBufferHostMemoryAllocationAndLimitedRangeAnd32BitThenAllocationGoesToSvmHeap) {
    if (executionEnvironment->rootDeviceEnvironments[0]->isFullRangeSvm()) {
        GTEST_SKIP();
    }

    memoryManager.reset(new MockWddmMemoryManager(true, true, *executionEnvironment));
    size_t size = 2 * MemoryConstants::megaByte;
    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties({0, size, GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, mockDeviceBitfield}));
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_NE(nullptr, reinterpret_cast<void *>(allocation->getGpuAddress()));

    auto heap = is32bit ? HeapIndex::HEAP_SVM : HeapIndex::HEAP_STANDARD;

    EXPECT_LT(GmmHelper::canonize(memoryManager->getGfxPartition(0)->getHeapBase(heap)), allocation->getGpuAddress());
    EXPECT_GT(GmmHelper::canonize(memoryManager->getGfxPartition(0)->getHeapLimit(heap)), allocation->getGpuAddress());

    memoryManager->freeGraphicsMemory(allocation);
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
        auto wddmFromRootDevice = executionEnvironment->rootDeviceEnvironments[i]->osInterface->get()->getWddm();
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
        auto wddm = static_cast<WddmMock *>(executionEnvironment->rootDeviceEnvironments[i]->osInterface->get()->getWddm());
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
    //call multiple times to make sure that result is constant
    EXPECT_FALSE(wddmMemoryManager.isCpuCopyRequired(&backup));
    EXPECT_FALSE(wddmMemoryManager.isCpuCopyRequired(&backup));
    EXPECT_FALSE(wddmMemoryManager.isCpuCopyRequired(&backup));
    EXPECT_FALSE(wddmMemoryManager.isCpuCopyRequired(&backup));
}
