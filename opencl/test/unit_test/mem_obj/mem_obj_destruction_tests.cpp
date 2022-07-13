/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/wait_status.h"
#include "shared/source/memory_manager/allocations_list.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/api/api.h"
#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/mem_obj.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

using namespace NEO;

template <typename Family>
class MyCsr : public UltCommandStreamReceiver<Family> {
  public:
    MyCsr(const ExecutionEnvironment &executionEnvironment, const DeviceBitfield deviceBitfield)
        : UltCommandStreamReceiver<Family>(const_cast<ExecutionEnvironment &>(executionEnvironment), 0, deviceBitfield) {}

    WaitStatus waitForCompletionWithTimeout(const WaitParams &params, uint32_t taskCountToWait) override {
        waitForCompletionWithTimeoutCalled++;
        waitForCompletionWithTimeoutParamsPassed.push_back({params.enableTimeout, params.waitTimeout, taskCountToWait});
        *this->getTagAddress() = getTagAddressValue;
        return waitForCompletionWithTimeoutResult;
    }

    struct WaitForCompletionWithTimeoutParams {
        bool enableTimeout;
        int64_t timeoutMs;
        uint32_t taskCountToWait;
    };

    uint32_t waitForCompletionWithTimeoutCalled = 0u;
    WaitStatus waitForCompletionWithTimeoutResult = NEO::WaitStatus::Ready;
    StackVec<WaitForCompletionWithTimeoutParams, 2> waitForCompletionWithTimeoutParamsPassed{};
    uint32_t getTagAddressValue{};
};

void CL_CALLBACK emptyDestructorCallback(cl_mem memObj, void *userData) {
}

template <bool useMultiGraphicsAllocation = false>
class MemObjDestructionTest : public ::testing::TestWithParam<bool> {
  public:
    void SetUp() override {
        executionEnvironment = platform()->peekExecutionEnvironment();
        memoryManager = new MockMemoryManager(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
        device = std::make_unique<MockClDevice>(MockDevice::create<MockDevice>(executionEnvironment, 0));
        context.reset(new MockContext(device.get()));
        allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), size});
        if constexpr (useMultiGraphicsAllocation) {
            MultiGraphicsAllocation multiAllocation(1u);
            multiAllocation.addAllocation(allocation);
            memObj = new MemObj(context.get(), CL_MEM_OBJECT_BUFFER,
                                ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &device->getDevice()),
                                CL_MEM_READ_WRITE, 0, size,
                                nullptr, nullptr, std::move(multiAllocation), true, false, false);
        } else {
            memObj = new MemObj(context.get(), CL_MEM_OBJECT_BUFFER,
                                ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &device->getDevice()),
                                CL_MEM_READ_WRITE, 0, size,
                                nullptr, nullptr, GraphicsAllocationHelper::toMultiGraphicsAllocation(allocation), true, false, false);
        }
        csr = device->getDefaultEngine().commandStreamReceiver;
        *csr->getTagAddress() = 0;
        contextId = device->getDefaultEngine().osContext->getContextId();
    }

    void TearDown() override {
        context.reset();
    }

    void makeMemObjUsed() {
        memObj->getGraphicsAllocation(device->getRootDeviceIndex())->updateTaskCount(taskCountReady, contextId);
    }

    void makeMemObjNotReady() {
        makeMemObjUsed();
        *device->getDefaultEngine().commandStreamReceiver->getTagAddress() = taskCountReady - 1;
    }

    void makeMemObjReady() {
        makeMemObjUsed();
        *device->getDefaultEngine().commandStreamReceiver->getTagAddress() = taskCountReady;
    }

    constexpr static uint32_t taskCountReady = 3u;
    ExecutionEnvironment *executionEnvironment = nullptr;
    std::unique_ptr<MockClDevice> device;
    uint32_t contextId = 0;
    MockMemoryManager *memoryManager = nullptr;
    std::unique_ptr<MockContext> context;
    GraphicsAllocation *allocation = nullptr;
    MemObj *memObj = nullptr;
    CommandStreamReceiver *csr = nullptr;
    size_t size = MemoryConstants::pageSize;
};

class MemObjAsyncDestructionTest : public MemObjDestructionTest<> {
  public:
    void SetUp() override {
        DebugManager.flags.EnableAsyncDestroyAllocations.set(true);
        MemObjDestructionTest::SetUp();
    }
    void TearDown() override {
        MemObjDestructionTest::TearDown();
    }
    DebugManagerStateRestore restorer;
};

class MemObjMulitAllocationAsyncDestructionTest : public MemObjDestructionTest<true> {
  public:
    void SetUp() override {
        DebugManager.flags.EnableAsyncDestroyAllocations.set(true);
        MemObjDestructionTest::SetUp();
    }
    void TearDown() override {
        MemObjDestructionTest::TearDown();
    }
    DebugManagerStateRestore restorer;
};

class MemObjSyncDestructionTest : public MemObjDestructionTest<> {
  public:
    void SetUp() override {
        DebugManager.flags.EnableAsyncDestroyAllocations.set(false);
        MemObjDestructionTest::SetUp();
    }
    void TearDown() override {
        MemObjDestructionTest::TearDown();
    }
    DebugManagerStateRestore restorer;
};

TEST_P(MemObjAsyncDestructionTest, givenMemObjWithDestructableAllocationWhenAsyncDestructionsAreEnabledAndAllocationIsNotReadyAndMemObjectIsDestructedThenAllocationIsDeferred) {
    bool isMemObjReady;
    bool expectedDeferration;
    isMemObjReady = GetParam();
    expectedDeferration = !isMemObjReady;

    if (isMemObjReady) {
        makeMemObjReady();
    } else {
        makeMemObjNotReady();
    }
    auto &allocationList = csr->getDeferredAllocations();
    EXPECT_TRUE(allocationList.peekIsEmpty());

    delete memObj;

    EXPECT_EQ(!expectedDeferration, allocationList.peekIsEmpty());
    if (expectedDeferration) {
        EXPECT_EQ(allocation, allocationList.peekHead());
    }
}

HWTEST_F(MemObjMulitAllocationAsyncDestructionTest, givenUsedMemObjWithAsyncDestructionsEnabledThatHasMultiGraphicsAllocationWhenItIsDestroyedThenDestructorWaitsOnTaskCount) {
    auto rootDeviceIndex = device->getRootDeviceIndex();
    auto mockCsr0 = new MyCsr<FamilyType>(*device->executionEnvironment, device->getDeviceBitfield());
    auto mockCsr1 = new MyCsr<FamilyType>(*device->executionEnvironment, device->getDeviceBitfield());
    device->resetCommandStreamReceiver(mockCsr0, 0);
    device->resetCommandStreamReceiver(mockCsr1, 1);
    *mockCsr0->getTagAddress() = 0;
    *mockCsr1->getTagAddress() = 0;
    mockCsr0->getTagAddressValue = taskCountReady;
    mockCsr1->getTagAddressValue = taskCountReady;
    auto osContextId0 = mockCsr0->getOsContext().getContextId();
    auto osContextId1 = mockCsr1->getOsContext().getContextId();
    memObj->getGraphicsAllocation(rootDeviceIndex)->updateTaskCount(taskCountReady, osContextId0);
    memObj->getGraphicsAllocation(rootDeviceIndex)->updateTaskCount(taskCountReady, osContextId1);
    auto expectedTaskCount0 = allocation->getTaskCount(osContextId0);
    auto expectedTaskCount1 = allocation->getTaskCount(osContextId1);

    delete memObj;

    EXPECT_EQ(1u, mockCsr0->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(TimeoutControls::maxTimeout, mockCsr0->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
    EXPECT_EQ(expectedTaskCount0, mockCsr0->waitForCompletionWithTimeoutParamsPassed[0].taskCountToWait);

    EXPECT_EQ(1u, mockCsr1->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(TimeoutControls::maxTimeout, mockCsr1->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
    EXPECT_EQ(expectedTaskCount1, mockCsr1->waitForCompletionWithTimeoutParamsPassed[0].taskCountToWait);
}

HWTEST_P(MemObjAsyncDestructionTest, givenUsedMemObjWithAsyncDestructionsEnabledThatHasDestructorCallbacksWhenItIsDestroyedThenDestructorWaitsOnTaskCount) {
    bool hasCallbacks = GetParam();

    if (hasCallbacks) {
        memObj->setDestructorCallback(emptyDestructorCallback, nullptr);
    }

    auto rootDeviceIndex = device->getRootDeviceIndex();

    auto mockCsr0 = new MyCsr<FamilyType>(*device->executionEnvironment, device->getDeviceBitfield());
    auto mockCsr1 = new MyCsr<FamilyType>(*device->executionEnvironment, device->getDeviceBitfield());
    device->resetCommandStreamReceiver(mockCsr0, 0);
    device->resetCommandStreamReceiver(mockCsr1, 1);
    *mockCsr0->getTagAddress() = 0;
    *mockCsr1->getTagAddress() = 0;

    mockCsr0->getTagAddressValue = taskCountReady;
    mockCsr1->getTagAddressValue = taskCountReady;

    auto osContextId0 = mockCsr0->getOsContext().getContextId();
    auto osContextId1 = mockCsr1->getOsContext().getContextId();

    memObj->getGraphicsAllocation(rootDeviceIndex)->updateTaskCount(taskCountReady, osContextId0);
    memObj->getGraphicsAllocation(rootDeviceIndex)->updateTaskCount(taskCountReady, osContextId1);

    uint32_t expectedTaskCount0{};
    uint32_t expectedTaskCount1{};

    if (hasCallbacks) {
        expectedTaskCount0 = allocation->getTaskCount(osContextId0);
        expectedTaskCount1 = allocation->getTaskCount(osContextId1);

    } else {
        *mockCsr0->getTagAddress() = taskCountReady;
        *mockCsr1->getTagAddress() = taskCountReady;
    }

    delete memObj;

    if (hasCallbacks) {
        EXPECT_EQ(1u, mockCsr0->waitForCompletionWithTimeoutCalled);
        EXPECT_EQ(TimeoutControls::maxTimeout, mockCsr0->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
        EXPECT_EQ(expectedTaskCount0, mockCsr0->waitForCompletionWithTimeoutParamsPassed[0].taskCountToWait);

        EXPECT_EQ(1u, mockCsr1->waitForCompletionWithTimeoutCalled);
        EXPECT_EQ(TimeoutControls::maxTimeout, mockCsr1->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
        EXPECT_EQ(expectedTaskCount1, mockCsr1->waitForCompletionWithTimeoutParamsPassed[0].taskCountToWait);
    } else {
        EXPECT_EQ(0u, mockCsr0->waitForCompletionWithTimeoutCalled);
        EXPECT_EQ(0u, mockCsr1->waitForCompletionWithTimeoutCalled);
    }
}

HWTEST_P(MemObjAsyncDestructionTest, givenUsedMemObjWithAsyncDestructionsEnabledThatHasAllocatedMappedPtrWhenItIsDestroyedThenDestructorWaitsOnTaskCount) {
    makeMemObjUsed();

    bool hasAllocatedMappedPtr = GetParam();

    if (hasAllocatedMappedPtr) {
        auto allocatedPtr = alignedMalloc(size, MemoryConstants::pageSize);
        memObj->setAllocatedMapPtr(allocatedPtr);
    }

    auto mockCsr = new MyCsr<FamilyType>(*device->executionEnvironment, device->getDeviceBitfield());
    device->resetCommandStreamReceiver(mockCsr);
    *mockCsr->getTagAddress() = 0;
    auto osContextId = mockCsr->getOsContext().getContextId();

    uint32_t expectedTaskCount{};

    if (hasAllocatedMappedPtr) {
        expectedTaskCount = allocation->getTaskCount(osContextId);
    }

    delete memObj;

    if (hasAllocatedMappedPtr) {
        EXPECT_EQ(1u, mockCsr->waitForCompletionWithTimeoutCalled);
        EXPECT_EQ(TimeoutControls::maxTimeout, mockCsr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
        EXPECT_EQ(expectedTaskCount, mockCsr->waitForCompletionWithTimeoutParamsPassed[0].taskCountToWait);
    } else {
        EXPECT_EQ(0u, mockCsr->waitForCompletionWithTimeoutCalled);
    }
}

HWTEST_P(MemObjAsyncDestructionTest, givenUsedMemObjWithAsyncDestructionsEnabledThatHasDestructableMappedPtrWhenItIsDestroyedThenDestructorWaitsOnTaskCount) {
    auto storage = alignedMalloc(size, MemoryConstants::pageSize);

    bool hasAllocatedMappedPtr = GetParam();

    if (!hasAllocatedMappedPtr) {
        delete memObj;
        allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{context->getDevice(0)->getRootDeviceIndex(), size});
        MemObjOffsetArray origin = {{0, 0, 0}};
        MemObjSizeArray region = {{1, 1, 1}};
        cl_map_flags mapFlags = CL_MAP_READ;
        memObj = new MemObj(context.get(), CL_MEM_OBJECT_BUFFER,
                            ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &context->getDevice(0)->getDevice()),
                            CL_MEM_READ_WRITE, 0, size,
                            storage, nullptr, GraphicsAllocationHelper::toMultiGraphicsAllocation(allocation), true, false, false);
        memObj->addMappedPtr(storage, 1, mapFlags, region, origin, 0, nullptr);
    } else {
        memObj->setAllocatedMapPtr(storage);
    }

    makeMemObjUsed();
    auto mockCsr = new MyCsr<FamilyType>(*device->executionEnvironment, device->getDeviceBitfield());
    device->resetCommandStreamReceiver(mockCsr);
    *mockCsr->getTagAddress() = 0;

    auto osContextId = mockCsr->getOsContext().getContextId();

    uint32_t expectedTaskCount{};

    if (hasAllocatedMappedPtr) {
        expectedTaskCount = allocation->getTaskCount(osContextId);
    }

    delete memObj;

    if (!hasAllocatedMappedPtr) {
        alignedFree(storage);
    }

    if (hasAllocatedMappedPtr) {
        EXPECT_EQ(1u, mockCsr->waitForCompletionWithTimeoutCalled);
        EXPECT_EQ(TimeoutControls::maxTimeout, mockCsr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
        EXPECT_EQ(expectedTaskCount, mockCsr->waitForCompletionWithTimeoutParamsPassed[0].taskCountToWait);
    } else {
        EXPECT_EQ(0u, mockCsr->waitForCompletionWithTimeoutCalled);
    }
}

HWTEST_P(MemObjSyncDestructionTest, givenMemObjWithDestructableAllocationWhenAsyncDestructionsAreDisabledThenDestructorWaitsOnTaskCount) {
    bool isMemObjReady;
    isMemObjReady = GetParam();

    if (isMemObjReady) {
        makeMemObjReady();
    } else {
        makeMemObjNotReady();
    }
    auto mockCsr = new MyCsr<FamilyType>(*device->executionEnvironment, device->getDeviceBitfield());
    device->resetCommandStreamReceiver(mockCsr);
    *mockCsr->getTagAddress() = 0;

    auto osContextId = mockCsr->getOsContext().getContextId();

    uint32_t expectedTaskCount = allocation->getTaskCount(osContextId);

    delete memObj;

    EXPECT_EQ(1u, mockCsr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(TimeoutControls::maxTimeout, mockCsr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
    EXPECT_EQ(expectedTaskCount, mockCsr->waitForCompletionWithTimeoutParamsPassed[0].taskCountToWait);
}

HWTEST_P(MemObjSyncDestructionTest, givenMemObjWithDestructableAllocationWhenAsyncDestructionsAreDisabledThenAllocationIsNotDeferred) {
    bool isMemObjReady;
    isMemObjReady = GetParam();

    if (isMemObjReady) {
        makeMemObjReady();
    } else {
        makeMemObjNotReady();
    }
    auto mockCsr = new MyCsr<FamilyType>(*device->executionEnvironment, device->getDeviceBitfield());
    device->resetCommandStreamReceiver(mockCsr);
    *mockCsr->getTagAddress() = 0;

    delete memObj;
    auto &allocationList = mockCsr->getTemporaryAllocations();
    EXPECT_TRUE(allocationList.peekIsEmpty());
}

HWTEST_P(MemObjSyncDestructionTest, givenMemObjWithMapAllocationWhenAsyncDestructionsAreDisabledThenWaitForCompletionWithTimeoutOnMapAllocation) {
    auto isMapAllocationUsed = GetParam();

    auto mockCsr = new MyCsr<FamilyType>(*device->executionEnvironment, device->getDeviceBitfield());
    device->resetCommandStreamReceiver(mockCsr);
    *mockCsr->getTagAddress() = 0;

    GraphicsAllocation *mapAllocation = nullptr;
    AllocationProperties properties{device->getRootDeviceIndex(),
                                    true,
                                    MemoryConstants::pageSize,
                                    AllocationType::MAP_ALLOCATION,
                                    false,
                                    context->getDeviceBitfieldForAllocation(device->getRootDeviceIndex())};
    mapAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, nullptr);
    memObj->setMapAllocation(mapAllocation);

    if (isMapAllocationUsed) {
        memObj->getMapAllocation(device->getRootDeviceIndex())->updateTaskCount(taskCountReady, contextId);
    }

    auto osContextId = mockCsr->getOsContext().getContextId();

    uint32_t expectedTaskCount{};

    if (isMapAllocationUsed) {
        expectedTaskCount = mapAllocation->getTaskCount(osContextId);
    }

    delete memObj;

    if (isMapAllocationUsed) {
        EXPECT_EQ(1u, mockCsr->waitForCompletionWithTimeoutCalled);
        EXPECT_EQ(TimeoutControls::maxTimeout, mockCsr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
        EXPECT_EQ(expectedTaskCount, mockCsr->waitForCompletionWithTimeoutParamsPassed[0].taskCountToWait);
    } else {
        EXPECT_EQ(0u, mockCsr->waitForCompletionWithTimeoutCalled);
    }
}

HWTEST_P(MemObjSyncDestructionTest, givenMemObjWithMapAllocationWhenAsyncDestructionsAreDisabledThenMapAllocationIsNotDeferred) {
    auto hasMapAllocation = GetParam();

    auto mockCsr = new MyCsr<FamilyType>(*device->executionEnvironment, device->getDeviceBitfield());
    device->resetCommandStreamReceiver(mockCsr);
    *mockCsr->getTagAddress() = 0;

    GraphicsAllocation *mapAllocation = nullptr;
    if (hasMapAllocation) {
        AllocationProperties properties{device->getRootDeviceIndex(),
                                        true,
                                        MemoryConstants::pageSize,
                                        AllocationType::MAP_ALLOCATION,
                                        false,
                                        context->getDeviceBitfieldForAllocation(device->getRootDeviceIndex())};
        mapAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, nullptr);
        memObj->setMapAllocation(mapAllocation);

        memObj->getMapAllocation(device->getRootDeviceIndex())->updateTaskCount(taskCountReady, contextId);
    }

    makeMemObjUsed();

    auto &allocationList = mockCsr->getTemporaryAllocations();
    EXPECT_TRUE(allocationList.peekIsEmpty());

    delete memObj;

    EXPECT_TRUE(allocationList.peekIsEmpty());
}

HWTEST_P(MemObjAsyncDestructionTest, givenMemObjWithMapAllocationWithoutMemUseHostPtrFlagWhenAsyncDestructionsAreEnabledThenMapAllocationIsDeferred) {
    auto hasMapAllocation = GetParam();

    auto mockCsr = new MyCsr<FamilyType>(*device->executionEnvironment, device->getDeviceBitfield());
    device->resetCommandStreamReceiver(mockCsr);
    *mockCsr->getTagAddress() = 0;

    GraphicsAllocation *mapAllocation = nullptr;
    if (hasMapAllocation) {
        AllocationProperties properties{device->getRootDeviceIndex(),
                                        true,
                                        MemoryConstants::pageSize,
                                        AllocationType::MAP_ALLOCATION,
                                        false,
                                        context->getDeviceBitfieldForAllocation(device->getRootDeviceIndex())};
        mapAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, nullptr);
        memObj->setMapAllocation(mapAllocation);

        memObj->getMapAllocation(device->getRootDeviceIndex())->updateTaskCount(taskCountReady, contextId);
    }

    makeMemObjUsed();

    auto &allocationList = mockCsr->getDeferredAllocations();
    EXPECT_TRUE(allocationList.peekIsEmpty());

    delete memObj;

    EXPECT_FALSE(allocationList.peekIsEmpty());

    if (hasMapAllocation) {
        EXPECT_EQ(allocation, allocationList.peekHead());
        EXPECT_EQ(mapAllocation, allocationList.peekTail());
    } else {
        EXPECT_EQ(allocation, allocationList.peekHead());
        EXPECT_EQ(allocation, allocationList.peekTail());
    }
}

HWTEST_P(MemObjAsyncDestructionTest, givenMemObjWithMapAllocationWithMemUseHostPtrFlagWhenAsyncDestructionsAreEnabledThenMapAllocationIsNotDeferred) {
    auto hasMapAllocation = GetParam();

    auto mockCsr = new MyCsr<FamilyType>(*device->executionEnvironment, device->getDeviceBitfield());
    device->resetCommandStreamReceiver(mockCsr);
    *mockCsr->getTagAddress() = 0;

    GraphicsAllocation *mapAllocation = nullptr;
    char *hostPtr = (char *)alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize64k);
    if (hasMapAllocation) {
        AllocationProperties properties{device->getRootDeviceIndex(),
                                        false,
                                        MemoryConstants::pageSize,
                                        AllocationType::MAP_ALLOCATION,
                                        false,
                                        context->getDeviceBitfieldForAllocation(device->getRootDeviceIndex())};
        mapAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, hostPtr);
        memObj->setMapAllocation(mapAllocation);

        memObj->getMapAllocation(device->getRootDeviceIndex())->updateTaskCount(taskCountReady, contextId);
    }

    makeMemObjUsed();

    auto &allocationList = mockCsr->getDeferredAllocations();
    EXPECT_TRUE(allocationList.peekIsEmpty());

    delete memObj;

    EXPECT_FALSE(allocationList.peekIsEmpty());

    if (hasMapAllocation) {
        EXPECT_EQ(allocation, allocationList.peekHead());
        EXPECT_EQ(allocation, allocationList.peekHead());
    } else {
        EXPECT_EQ(allocation, allocationList.peekHead());
        EXPECT_EQ(allocation, allocationList.peekTail());
    }

    alignedFree(hostPtr);
}

INSTANTIATE_TEST_CASE_P(
    MemObjTests,
    MemObjAsyncDestructionTest,
    testing::Bool());

INSTANTIATE_TEST_CASE_P(
    MemObjTests,
    MemObjSyncDestructionTest,
    testing::Bool());

using UsmDestructionTests = ::testing::Test;

HWTEST_F(UsmDestructionTests, givenSharedUsmAllocationWhenBlockingFreeIsCalledThenWaitForCompletionIsCalled) {
    MockDevice mockDevice;
    mockDevice.incRefInternal();
    MockClDevice mockClDevice(&mockDevice);
    MockContext mockContext(&mockClDevice, false);

    if (mockContext.getDevice(0u)->getHardwareInfo().capabilityTable.supportsOcl21Features == false) {
        GTEST_SKIP();
    }

    auto mockCsr = new MyCsr<FamilyType>(*mockDevice.executionEnvironment, 1);
    mockDevice.resetCommandStreamReceiver(mockCsr);
    *mockCsr->getTagAddress() = 5u;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::SHARED_UNIFIED_MEMORY, mockContext.getRootDeviceIndices(), mockContext.getDeviceBitfields());

    auto svmAllocationsManager = mockContext.getSVMAllocsManager();
    auto sharedMemory = svmAllocationsManager->createUnifiedAllocationWithDeviceStorage(4096u, {}, unifiedMemoryProperties);
    ASSERT_NE(nullptr, sharedMemory);

    auto svmEntry = svmAllocationsManager->getSVMAlloc(sharedMemory);

    svmEntry->gpuAllocations.getGraphicsAllocation(mockDevice.getRootDeviceIndex())->updateTaskCount(6u, 0u);
    svmEntry->cpuAllocation->updateTaskCount(6u, 0u);

    clMemBlockingFreeINTEL(&mockContext, sharedMemory);

    EXPECT_EQ(2u, mockCsr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(TimeoutControls::maxTimeout, mockCsr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
    EXPECT_EQ(TimeoutControls::maxTimeout, mockCsr->waitForCompletionWithTimeoutParamsPassed[1].timeoutMs);
    EXPECT_EQ(6u, mockCsr->waitForCompletionWithTimeoutParamsPassed[0].taskCountToWait);
    EXPECT_EQ(6u, mockCsr->waitForCompletionWithTimeoutParamsPassed[1].taskCountToWait);
}

HWTEST_F(UsmDestructionTests, givenUsmAllocationWhenBlockingFreeIsCalledThenWaitForCompletionIsCalled) {
    MockDevice mockDevice;
    mockDevice.incRefInternal();
    MockClDevice mockClDevice(&mockDevice);
    MockContext mockContext(&mockClDevice, false);

    if (mockContext.getDevice(0u)->getHardwareInfo().capabilityTable.supportsOcl21Features == false) {
        GTEST_SKIP();
    }

    auto mockCsr = new MyCsr<FamilyType>(*mockDevice.executionEnvironment, 1);
    mockDevice.resetCommandStreamReceiver(mockCsr);
    *mockCsr->getTagAddress() = 5u;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::HOST_UNIFIED_MEMORY, mockContext.getRootDeviceIndices(), mockContext.getDeviceBitfields());

    auto svmAllocationsManager = mockContext.getSVMAllocsManager();
    auto hostMemory = svmAllocationsManager->createUnifiedMemoryAllocation(4096u, unifiedMemoryProperties);
    ASSERT_NE(nullptr, hostMemory);

    auto svmEntry = svmAllocationsManager->getSVMAlloc(hostMemory);

    svmEntry->gpuAllocations.getGraphicsAllocation(mockDevice.getRootDeviceIndex())->updateTaskCount(6u, 0u);

    clMemBlockingFreeINTEL(&mockContext, hostMemory);

    EXPECT_EQ(1u, mockCsr->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(TimeoutControls::maxTimeout, mockCsr->waitForCompletionWithTimeoutParamsPassed[0].timeoutMs);
    EXPECT_EQ(6u, mockCsr->waitForCompletionWithTimeoutParamsPassed[0].taskCountToWait);
}
