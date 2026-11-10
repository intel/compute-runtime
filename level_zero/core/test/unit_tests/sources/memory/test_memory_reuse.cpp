/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/svm_allocation_cache.h"
#include "shared/source/memory_manager/unified_memory_pooling.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/test_macros/test_base.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"
namespace L0 {
namespace ult {
template <int hostUsmReuseFlag = -1, int deviceUsmReuseFlag = -1, uint16_t numRootDevices = 1, bool usmReuseSupported = false>
struct UsmReuseMemoryTest : public ::testing::Test {
    void SetUp() override {
        NEO::debugManager.flags.ExperimentalEnableHostAllocationCache.set(hostUsmReuseFlag);
        NEO::debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(deviceUsmReuseFlag);

        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        auto hwInfo = *NEO::defaultHwInfo;
        hwInfo.caps.sharedUsmAllocationReuseSupported = usmReuseSupported;
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            mockProductHelpers.push_back(new MockProductHelper);
            mockProductHelpers[i]->isDeviceUsmAllocationReuseSupportedResult = usmReuseSupported;
            mockProductHelpers[i]->isHostUsmAllocationReuseSupportedResult = usmReuseSupported;
            executionEnvironment->rootDeviceEnvironments[i]->productHelper.reset(mockProductHelpers[i]);
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(&hwInfo);
            executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        }
        std::vector<std::unique_ptr<NEO::Device>> devices;
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            auto device = std::unique_ptr<NEO::MockDevice>(NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(&hwInfo,
                                                                                                                            executionEnvironment, i));
            device->deviceInfo.localMemSize = 4 * MemoryConstants::gigaByte;
            device->deviceInfo.globalMemSize = 4 * MemoryConstants::gigaByte;
            devices.push_back(std::move(device));
        }

        driverHandle = std::make_unique<Mock<L0::DriverHandle>>();
        driverHandle->initialize(std::move(devices));

        svmAllocsManager = reinterpret_cast<MockSVMAllocsManager *>(driverHandle->svmAllocsManager);

        ze_context_handle_t hContext;
        ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
        ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        context = Context::fromHandle(hContext);
    }

    void TearDown() override {
        context->destroy();
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandle>> driverHandle;
    L0::Context *context = nullptr;
    std::vector<MockProductHelper *> mockProductHelpers;
    NEO::ExecutionEnvironment *executionEnvironment;
    NEO::MockSVMAllocsManager *svmAllocsManager;
};

using MultiDeviceReuseTest = UsmReuseMemoryTest<2, 8, 2>;

TEST_F(MultiDeviceReuseTest, givenUsmReuseForcedByDebugKeysWhenInitializingMultiDeviceDriverHandleThenInitializeUsmReuse) {
    EXPECT_NE(nullptr, svmAllocsManager->usmHostAllocationsCache.get());
    EXPECT_NE(nullptr, svmAllocsManager->usmDeviceAllocationsCache.get());
}

using MultiDeviceDefaultReuseTest = UsmReuseMemoryTest<-1, -1, 2, true>;

TEST_F(MultiDeviceDefaultReuseTest, givenUsmReuseSupportedAndDebugKeysNotSetWhenInitializingMultiDeviceDriverHandleThenDoNotInitializeUsmReuse) {
    EXPECT_EQ(nullptr, svmAllocsManager->usmHostAllocationsCache.get());
    EXPECT_EQ(nullptr, svmAllocsManager->usmDeviceAllocationsCache.get());
    EXPECT_EQ(nullptr, svmAllocsManager->usmSharedAllocationsCache.get());
}

using SingleDeviceDefaultReuseTest = UsmReuseMemoryTest<-1, -1, 1, true>;

TEST_F(SingleDeviceDefaultReuseTest, givenUsmReuseSupportedAndDebugKeysNotSetWhenInitializingSingleDeviceDriverHandleThenInitializeUsmReuse) {
    EXPECT_NE(nullptr, svmAllocsManager->usmHostAllocationsCache.get());
    EXPECT_NE(nullptr, svmAllocsManager->usmDeviceAllocationsCache.get());
}

template <int hostUsmReuseFlag, int deviceUsmReuseFlag>
struct MultiDeviceWithoutPoolingTest : public UsmReuseMemoryTest<hostUsmReuseFlag, deviceUsmReuseFlag, 2> {
    using BaseClass = UsmReuseMemoryTest<hostUsmReuseFlag, deviceUsmReuseFlag, 2>;

    void SetUp() override {
        NEO::debugManager.flags.EnableHostUsmAllocationPool.set(0);
        NEO::debugManager.flags.EnableDeviceUsmAllocationPool.set(0);
        BaseClass::SetUp();
        for (auto &rootDeviceEnvironment : this->executionEnvironment->rootDeviceEnvironments) {
            rootDeviceEnvironment->memoryOperationsInterface = std::make_unique<NEO::MockMemoryOperations>();
        }
        memoryManager = static_cast<NEO::MockMemoryManager *>(this->driverHandle->getMemoryManager());
        peerDevice = this->driverHandle->devices[1];
        peerCsr = peerDevice->getNEODevice()->getDefaultEngine().commandStreamReceiver;
    }

    void TearDown() override {
        completePeerWork();
        BaseClass::TearDown();
    }

    void *allocateOnDevice(L0::Device *device) {
        void *ptr = nullptr;
        ze_device_mem_alloc_desc_t deviceDesc = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, this->context->allocDeviceMem(device->toHandle(), &deviceDesc, MemoryConstants::pageSize, 0u, &ptr));
        return ptr;
    }

    void *allocateOnOwnerDevice() {
        return allocateOnDevice(this->driverHandle->devices[0]);
    }

    ze_result_t deferFree(void *ptr) {
        ze_memory_free_ext_desc_t freeDesc = {ZE_STRUCTURE_TYPE_MEMORY_FREE_EXT_DESC};
        freeDesc.freePolicy = ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_DEFER_FREE;
        return this->context->freeMemExt(&freeDesc, ptr);
    }

    void markPendingOnPeer(NEO::GraphicsAllocation &allocation) {
        allocation.updateTaskCount(pendingPeerTaskCount, peerCsr->getOsContext().getContextId());
        *peerCsr->getTagAddress() = pendingPeerTaskCount - 1;
    }

    void completePeerWork() {
        *peerCsr->getTagAddress() = pendingPeerTaskCount;
    }

    static constexpr TaskCountType pendingPeerTaskCount = 1u;
    NEO::MockMemoryManager *memoryManager = nullptr;
    L0::Device *peerDevice = nullptr;
    NEO::CommandStreamReceiver *peerCsr = nullptr;
};

using MultiDeviceReuseWithoutPoolingTest = MultiDeviceWithoutPoolingTest<2, 8>;

TEST_F(MultiDeviceReuseWithoutPoolingTest, givenDeviceAllocationWithoutPeerImportWhenDeferFreeingThenAllocationIsSavedForReuse) {
    void *ptr = allocateOnOwnerDevice();
    ASSERT_NE(nullptr, ptr);

    ASSERT_EQ(ZE_RESULT_SUCCESS, deferFree(ptr));

    EXPECT_EQ(1u, svmAllocsManager->usmDeviceAllocationsCache->allocations.size());
}

TEST_F(MultiDeviceReuseWithoutPoolingTest, givenFreedDeviceAllocationWhenAllocatingOnSameDeviceThenAllocationIsReused) {
    void *ptr = allocateOnOwnerDevice();
    ASSERT_NE(nullptr, ptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->freeMem(ptr));
    ASSERT_EQ(1u, svmAllocsManager->usmDeviceAllocationsCache->allocations.size());

    void *reusedPtr = allocateOnOwnerDevice();

    EXPECT_EQ(ptr, reusedPtr);
    EXPECT_TRUE(svmAllocsManager->usmDeviceAllocationsCache->allocations.empty());
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->freeMem(reusedPtr));
}

TEST_F(MultiDeviceReuseWithoutPoolingTest, givenFreedDeviceAllocationWhenAllocatingOnOtherDeviceThenAllocationIsNotReused) {
    void *ptr = allocateOnOwnerDevice();
    ASSERT_NE(nullptr, ptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->freeMem(ptr));

    void *otherDevicePtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->allocDeviceMem(driverHandle->devices[1]->toHandle(), &deviceDesc, MemoryConstants::pageSize, 0u, &otherDevicePtr));

    EXPECT_NE(ptr, otherDevicePtr);
    EXPECT_EQ(1u, svmAllocsManager->usmDeviceAllocationsCache->allocations.size());
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->freeMem(otherDevicePtr));
}

struct MultiDeviceReuseWithPeerImportTest : public MultiDeviceReuseWithoutPoolingTest {
    void SetUp() override {
        MultiDeviceReuseWithoutPoolingTest::SetUp();
        ptr = allocateOnOwnerDevice();
        ASSERT_NE(nullptr, ptr);
        ASSERT_EQ(ZE_RESULT_SUCCESS, context->makeMemoryResident(peerDevice->toHandle(), ptr, MemoryConstants::pageSize));
        peerAllocation = driverHandle->findPeerAllocation(peerDevice, ptr);
        ASSERT_NE(nullptr, peerAllocation);
    }

    void *ptr = nullptr;
    NEO::GraphicsAllocation *peerAllocation = nullptr;
};

TEST_F(MultiDeviceReuseWithPeerImportTest, givenPeerImportedDeviceAllocationWhenFreedThenAllocationIsSavedForReuseWithPeerImportKept) {
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->freeMem(ptr));

    EXPECT_EQ(1u, svmAllocsManager->usmDeviceAllocationsCache->allocations.size());
    EXPECT_EQ(peerAllocation, driverHandle->findPeerAllocation(peerDevice, ptr));
}

TEST_F(MultiDeviceReuseWithPeerImportTest, givenCachedAllocationWithPeerImportWhenReusedThenPeerImportIsReused) {
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->freeMem(ptr));

    void *reusedPtr = allocateOnOwnerDevice();
    ASSERT_EQ(ptr, reusedPtr);

    EXPECT_EQ(peerAllocation, driverHandle->resolveMemoryAllocation(peerDevice, reusedPtr, MemoryConstants::pageSize, true));
    EXPECT_EQ(1u, peerDevice->peerAllocations.allocations.size());
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->freeMem(reusedPtr));
}

TEST_F(MultiDeviceReuseWithPeerImportTest, givenCachedAllocationWithPeerImportWhenCacheIsTrimmedThenPeerImportIsReleased) {
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->freeMem(ptr));

    svmAllocsManager->trimUSMDeviceAllocCache();

    EXPECT_TRUE(svmAllocsManager->usmDeviceAllocationsCache->allocations.empty());
    EXPECT_EQ(nullptr, driverHandle->findPeerAllocation(peerDevice, ptr));
}

TEST_F(MultiDeviceReuseWithPeerImportTest, givenPeerImportWithPendingWorkWhenDeferFreedAllocationIsRequestedThenItIsNotReusedUntilPeerWorkCompletes) {
    memoryManager->callBaseAllocInUse = true;
    markPendingOnPeer(*peerAllocation);

    ASSERT_EQ(ZE_RESULT_SUCCESS, deferFree(ptr));
    ASSERT_EQ(1u, svmAllocsManager->usmDeviceAllocationsCache->allocations.size());

    void *newPtr = allocateOnOwnerDevice();
    EXPECT_NE(ptr, newPtr);

    completePeerWork();
    void *reusedPtr = allocateOnOwnerDevice();
    EXPECT_EQ(ptr, reusedPtr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, context->freeMem(newPtr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->freeMem(reusedPtr));
}

TEST_F(MultiDeviceReuseWithPeerImportTest, givenDeferFreedAllocationWithPendingPeerWorkWhenCacheIsTrimmedThenAllocationAndPeerImportAreKept) {
    memoryManager->callBaseAllocInUse = true;
    markPendingOnPeer(*peerAllocation);
    ASSERT_EQ(ZE_RESULT_SUCCESS, deferFree(ptr));

    svmAllocsManager->trimUSMDeviceAllocCache();

    EXPECT_EQ(1u, svmAllocsManager->usmDeviceAllocationsCache->allocations.size());
    EXPECT_EQ(peerAllocation, driverHandle->findPeerAllocation(peerDevice, ptr));

    completePeerWork();
    svmAllocsManager->trimUSMDeviceAllocCache();

    EXPECT_TRUE(svmAllocsManager->usmDeviceAllocationsCache->allocations.empty());
    EXPECT_EQ(nullptr, driverHandle->findPeerAllocation(peerDevice, ptr));
}

struct MultiDevicePeerImportDeferFreeTest : public MultiDeviceWithoutPoolingTest<-1, -1> {
    void SetUp() override {
        MultiDeviceWithoutPoolingTest<-1, -1>::SetUp();
        memoryManager->callBaseAllocInUse = true;
    }

    NEO::GraphicsAllocation *importMappedOnPeer(void *ptr) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, context->makeMemoryResident(peerDevice->toHandle(), ptr, MemoryConstants::pageSize));
        return driverHandle->findPeerAllocation(peerDevice, ptr);
    }

    void *importNonMappedOnPeer(void *ptr) {
        void *importPtr = allocateOnDevice(peerDevice);
        auto importData = svmAllocsManager->getSVMAlloc(importPtr);
        importData->isImportedAllocation = true;
        peerDevice->peerAllocations.allocations.emplace(ptr, *importData);
        return importPtr;
    }
};

TEST_F(MultiDevicePeerImportDeferFreeTest, givenImportedAllocationWithMappedPeerImportWhenIpcHandleIsClosedThenPeerImportIsReleased) {
    void *ptr = allocateOnOwnerDevice();
    ASSERT_NE(nullptr, ptr);
    svmAllocsManager->getSVMAlloc(ptr)->isImportedAllocation = true;
    ASSERT_NE(nullptr, importMappedOnPeer(ptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, context->closeIpcMemHandle(ptr));

    EXPECT_EQ(nullptr, svmAllocsManager->getSVMAlloc(ptr));
    EXPECT_EQ(nullptr, driverHandle->findPeerAllocation(peerDevice, ptr));
}

TEST_F(MultiDevicePeerImportDeferFreeTest, givenMappedPeerImportWithPendingPeerWorkWhenOwnerIsDeferFreedThenOwnerAndImportAreKeptUntilPeerWorkCompletes) {
    void *ptr = allocateOnDevice(driverHandle->devices[0]);
    ASSERT_NE(nullptr, ptr);
    auto peerAllocation = importMappedOnPeer(ptr);
    ASSERT_NE(nullptr, peerAllocation);
    ASSERT_TRUE(peerDevice->peerAllocations.allocations.at(ptr).mappedAllocData);
    markPendingOnPeer(*peerAllocation);

    ASSERT_EQ(ZE_RESULT_SUCCESS, deferFree(ptr));

    EXPECT_NE(nullptr, svmAllocsManager->getSVMAlloc(ptr));
    EXPECT_EQ(peerAllocation, driverHandle->findPeerAllocation(peerDevice, ptr));

    completePeerWork();
    svmAllocsManager->freeSVMAllocDeferImpl();

    EXPECT_EQ(nullptr, svmAllocsManager->getSVMAlloc(ptr));
    EXPECT_EQ(nullptr, driverHandle->findPeerAllocation(peerDevice, ptr));
}

TEST_F(MultiDevicePeerImportDeferFreeTest, givenMappedPeerImportWithPendingPeerWorkWhenDeferredFreesAreDrainedBlockingThenImportIsReleasedAfterPeerWorkIsWaitedFor) {
    void *ptr = allocateOnDevice(driverHandle->devices[0]);
    ASSERT_NE(nullptr, ptr);
    auto peerAllocation = importMappedOnPeer(ptr);
    ASSERT_NE(nullptr, peerAllocation);
    markPendingOnPeer(*peerAllocation);
    ASSERT_EQ(ZE_RESULT_SUCCESS, deferFree(ptr));
    memoryManager->waitForEnginesCompletionCallback = [this]() { completePeerWork(); };
    const auto waitsBefore = memoryManager->waitForEnginesCompletionCalled;

    svmAllocsManager->drainAllDeferFreeAllocsBlocking();

    EXPECT_LT(waitsBefore, memoryManager->waitForEnginesCompletionCalled);
    EXPECT_EQ(nullptr, svmAllocsManager->getSVMAlloc(ptr));
    EXPECT_EQ(nullptr, driverHandle->findPeerAllocation(peerDevice, ptr));
}

TEST_F(MultiDevicePeerImportDeferFreeTest, givenNonMappedPeerImportWithPendingPeerWorkWhenOwnerIsDeferFreedThenOwnerAndImportAreFreedOnlyAfterPeerWorkCompletes) {
    void *ptr = allocateOnDevice(driverHandle->devices[0]);
    ASSERT_NE(nullptr, ptr);
    void *importPtr = importNonMappedOnPeer(ptr);
    ASSERT_NE(nullptr, importPtr);
    markPendingOnPeer(*svmAllocsManager->getSVMAlloc(importPtr)->gpuAllocations.getDefaultGraphicsAllocation());

    ASSERT_EQ(ZE_RESULT_SUCCESS, deferFree(ptr));

    EXPECT_NE(nullptr, svmAllocsManager->getSVMAlloc(ptr));
    EXPECT_NE(nullptr, svmAllocsManager->getSVMAlloc(importPtr));

    completePeerWork();
    svmAllocsManager->freeSVMAllocDeferImpl();

    EXPECT_EQ(nullptr, svmAllocsManager->getSVMAlloc(ptr));
    EXPECT_EQ(nullptr, svmAllocsManager->getSVMAlloc(importPtr));
    EXPECT_EQ(nullptr, driverHandle->findPeerAllocation(peerDevice, ptr));
}

TEST_F(MultiDevicePeerImportDeferFreeTest, givenQueuedDeferredFreeOfAllocationWithPeerImportWhenNonMappedPeerImportIsReleasedThenItIsFreedAndQueuedFreeStaysDeferred) {
    void *queuedPtr = allocateOnDevice(driverHandle->devices[0]);
    ASSERT_NE(nullptr, queuedPtr);
    auto queuedPeerAllocation = importMappedOnPeer(queuedPtr);
    ASSERT_NE(nullptr, queuedPeerAllocation);
    markPendingOnPeer(*queuedPeerAllocation);
    ASSERT_EQ(ZE_RESULT_SUCCESS, deferFree(queuedPtr));
    ASSERT_EQ(1u, svmAllocsManager->getNumClaimableDeferFreeAllocs());
    void *ptr = allocateOnDevice(driverHandle->devices[0]);
    ASSERT_NE(nullptr, ptr);
    void *importPtr = importNonMappedOnPeer(ptr);
    ASSERT_NE(nullptr, importPtr);

    ASSERT_EQ(ZE_RESULT_SUCCESS, context->freeMem(ptr));

    EXPECT_EQ(nullptr, svmAllocsManager->getSVMAlloc(importPtr));
    EXPECT_EQ(1u, svmAllocsManager->getNumClaimableDeferFreeAllocs());

    completePeerWork();
    svmAllocsManager->freeSVMAllocDeferImpl();

    EXPECT_EQ(nullptr, svmAllocsManager->getSVMAlloc(queuedPtr));
}

struct MultiDeviceSharedReuseWithPeerImportTest : public MultiDeviceReuseWithoutPoolingTest {
    void SetUp() override {
        NEO::debugManager.flags.ExperimentalEnableSharedAllocationCache.set(1);
        MultiDeviceReuseWithoutPoolingTest::SetUp();
        ASSERT_NE(nullptr, svmAllocsManager->usmSharedAllocationsCache.get());
        ze_device_mem_alloc_desc_t deviceDesc = {};
        ze_host_mem_alloc_desc_t hostDesc = {};
        ASSERT_EQ(ZE_RESULT_SUCCESS, context->allocSharedMem(driverHandle->devices[0]->toHandle(), &deviceDesc, &hostDesc, MemoryConstants::pageSize, 0u, &ptr));
        auto allocData = svmAllocsManager->getSVMAlloc(ptr);
        ASSERT_NE(nullptr, allocData);
        ASSERT_TRUE(driverHandle->isRemoteResourceNeeded(*allocData, peerDevice));
        peerAllocation = driverHandle->getPeerAllocation(peerDevice, allocData, ptr, nullptr, nullptr, false);
        ASSERT_NE(nullptr, peerAllocation);
    }

    void *ptr = nullptr;
    NEO::GraphicsAllocation *peerAllocation = nullptr;
};

TEST_F(MultiDeviceSharedReuseWithPeerImportTest, givenCachedSharedAllocationWithPeerImportWhenCacheIsTrimmedThenPeerImportIsReleased) {
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->freeMem(ptr));
    ASSERT_EQ(1u, svmAllocsManager->usmSharedAllocationsCache->allocations.size());
    EXPECT_EQ(peerAllocation, driverHandle->findPeerAllocation(peerDevice, ptr));

    svmAllocsManager->trimUSMSharedAllocCache();

    EXPECT_TRUE(svmAllocsManager->usmSharedAllocationsCache->allocations.empty());
    EXPECT_EQ(nullptr, driverHandle->findPeerAllocation(peerDevice, ptr));
}

using SingleDeviceReuseTest = UsmReuseMemoryTest<2, 8, 1>;

TEST_F(SingleDeviceReuseTest, givenUsmReuseEnabledWhenInitializingDriverHandleThenInitializeUsmReuse) {
    EXPECT_NE(nullptr, svmAllocsManager->usmHostAllocationsCache.get());
    EXPECT_NE(nullptr, svmAllocsManager->usmDeviceAllocationsCache.get());
}

struct SingleDeviceReuseWithoutPoolingTest : public UsmReuseMemoryTest<2, 8, 1> {
    void SetUp() override {
        NEO::debugManager.flags.EnableHostUsmAllocationPool.set(0);
        NEO::debugManager.flags.EnableDeviceUsmAllocationPool.set(0);
        UsmReuseMemoryTest<2, 8, 1>::SetUp();
    }
};

TEST_F(SingleDeviceReuseWithoutPoolingTest, givenMemAdvisedAllocationWhenAllocationIsReusedFromCacheThenMemAdviseStateIsNotInherited) {
    constexpr size_t size = MemoryConstants::pageSize;
    auto device = driverHandle->devices[0];

    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(), &deviceDesc, size, 0u, &ptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, ptr);

    auto allocData = svmAllocsManager->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, allocData);
    auto gfxAlloc = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, gfxAlloc);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    result = commandList->executeMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_BIAS_UNCACHED);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_EQ(0, gfxAlloc->getMemAdviseFlags().cachedMemory);

    result = context->freeMem(ptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    void *reusedPtr = nullptr;
    result = context->allocDeviceMem(device->toHandle(), &deviceDesc, size, 0u, &reusedPtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_EQ(allocData, svmAllocsManager->getSVMAlloc(reusedPtr));
    ASSERT_EQ(gfxAlloc, allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex()));

    EXPECT_EQ(NEO::MemAdviseFlags{}.allFlags, gfxAlloc->getMemAdviseFlags().allFlags);

    result = context->freeMem(reusedPtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(SingleDeviceReuseWithoutPoolingTest, givenIpcExportedDeviceAllocationWhenFreeingThenAllocationIsNotSavedForReuse) {
    constexpr size_t size = MemoryConstants::pageSize;
    auto device = driverHandle->devices[0];
    context->settings.enableIpcHandleSharingByDefault = true;

    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(), &deviceDesc, size, 0u, &ptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, ptr);

    ze_ipc_mem_handle_t ipcHandle = {};
    result = context->getIpcMemHandle(ptr, nullptr, &ipcHandle);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(svmAllocsManager->usmDeviceAllocationsCache->allocations.empty());
    EXPECT_EQ(nullptr, svmAllocsManager->getSVMAlloc(ptr));
}

TEST_F(SingleDeviceReuseWithoutPoolingTest, givenIpcExportedDeviceAllocationWhenDeferFreeingThenAllocationIsNotSavedForReuse) {
    constexpr size_t size = MemoryConstants::pageSize;
    auto device = driverHandle->devices[0];
    context->settings.enableIpcHandleSharingByDefault = true;

    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(), &deviceDesc, size, 0u, &ptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, ptr);

    ze_ipc_mem_handle_t ipcHandle = {};
    result = context->getIpcMemHandle(ptr, nullptr, &ipcHandle);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_memory_free_ext_desc_t freeDesc = {ZE_STRUCTURE_TYPE_MEMORY_FREE_EXT_DESC};
    freeDesc.freePolicy = ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_DEFER_FREE;
    result = context->freeMemExt(&freeDesc, ptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(svmAllocsManager->usmDeviceAllocationsCache->allocations.empty());
}

TEST_F(SingleDeviceReuseWithoutPoolingTest, givenIpcExportedHostAllocationWhenFreeingThenAllocationIsNotSavedForReuse) {
    constexpr size_t size = MemoryConstants::pageSize;
    context->settings.enableIpcHandleSharingByDefault = true;

    void *ptr = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->allocHostMem(&hostDesc, size, 0u, &ptr));
    ASSERT_NE(nullptr, ptr);

    ze_ipc_mem_handle_t ipcHandle = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->getIpcMemHandle(ptr, nullptr, &ipcHandle));
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->freeMem(ptr));

    EXPECT_TRUE(svmAllocsManager->usmHostAllocationsCache->allocations.empty());
}

struct SingleDeviceWithPoolingTest : public UsmReuseMemoryTest<2, 8, 1> {
    void SetUp() override {
        NEO::debugManager.flags.EnableHostUsmAllocationPool.set(1);
        NEO::debugManager.flags.EnableDeviceUsmAllocationPool.set(1);
        UsmReuseMemoryTest<2, 8, 1>::SetUp();
    }
};

TEST_F(SingleDeviceWithPoolingTest, givenPooledAllocationWhenExecuteMemAdviseIsCalledThenAdviseIsIgnored) {
    constexpr size_t size = MemoryConstants::pageSize;
    auto device = driverHandle->devices[0];

    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(), &deviceDesc, size, 0u, &ptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, ptr);

    auto allocData = svmAllocsManager->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, allocData);
    ASSERT_NE(nullptr, context->getUsmPoolOwningPtr(ptr, allocData).pool);
    auto gfxAlloc = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, gfxAlloc);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    result = commandList->executeMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_BIAS_UNCACHED);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(NEO::MemAdviseFlags{}.allFlags, gfxAlloc->getMemAdviseFlags().allFlags);

    result = context->freeMem(ptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

struct SingleDeviceReuseWithSinglePoolTest : public UsmReuseMemoryTest<2, 8, 1> {
    void SetUp() override {
        NEO::debugManager.flags.EnableDeviceUsmAllocationPool.set(1);
        NEO::debugManager.flags.EnableUsmAllocationPoolManager.set(0);
        UsmReuseMemoryTest<2, 8, 1>::SetUp();
    }
};

TEST_F(SingleDeviceReuseWithSinglePoolTest, givenIpcExportedPooledChunkWhenPoolIsCleanedUpThenPoolAllocationIsNotSavedForReuse) {
    auto device = driverHandle->devices[0];
    context->settings.enableIpcHandleSharingByDefault = true;

    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->allocDeviceMem(device->toHandle(), &deviceDesc, 1u, 0u, &ptr));
    auto &poolFacade = device->getNEODevice()->getDeviceUsmMemAllocPoolFacade();
    auto pool = poolFacade.getPool();
    ASSERT_NE(nullptr, pool);
    ASSERT_TRUE(pool->isInPoolRange(ptr));

    ze_ipc_mem_handle_t ipcHandle = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->getIpcMemHandle(ptr, nullptr, &ipcHandle));
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->freeMem(ptr));

    poolFacade.cleanup();

    EXPECT_TRUE(svmAllocsManager->usmDeviceAllocationsCache->allocations.empty());
}
} // namespace ult
} // namespace L0
