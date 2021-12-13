/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"
#include "shared/test/common/mocks/ult_device_factory.h"

#include "test.h"

#include "level_zero/core/test/unit_tests/fixtures/aub_csr_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

namespace L0 {
namespace ult {

using ContextCreateCommandQueueTest = Test<ContextFixture>;

TEST_F(ContextCreateCommandQueueTest, givenCallToContextCreateCommandQueueThenCallSucceeds) {
    ze_command_queue_desc_t desc = {};
    desc.ordinal = 0u;

    ze_command_queue_handle_t commandQueue = {};

    ze_result_t res = context->createCommandQueue(device, &desc, &commandQueue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, commandQueue);

    L0::CommandQueue::fromHandle(commandQueue)->destroy();
}

HWTEST_F(ContextCreateCommandQueueTest, givenEveryPossibleGroupIndexWhenCreatingCommandQueueThenCommandQueueIsCreated) {
    ze_command_queue_handle_t commandQueue = {};
    auto &engineGroups = neoDevice->getRegularEngineGroups();
    for (uint32_t ordinal = 0; ordinal < engineGroups.size(); ordinal++) {
        for (uint32_t index = 0; index < engineGroups[ordinal].engines.size(); index++) {
            ze_command_queue_desc_t desc = {};
            desc.ordinal = ordinal;
            desc.index = index;
            ze_result_t res = context->createCommandQueue(device, &desc, &commandQueue);
            EXPECT_EQ(ZE_RESULT_SUCCESS, res);
            EXPECT_NE(nullptr, commandQueue);

            L0::CommandQueue::fromHandle(commandQueue)->destroy();
        }
    }
}

HWTEST_F(ContextCreateCommandQueueTest, givenOrdinalBiggerThanAvailableEnginesWhenCreatingCommandQueueThenInvalidArgumentErrorIsReturned) {
    ze_command_queue_handle_t commandQueue = {};
    auto &engineGroups = neoDevice->getRegularEngineGroups();
    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(engineGroups.size());
    desc.index = 0;
    ze_result_t res = context->createCommandQueue(device, &desc, &commandQueue);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);
    EXPECT_EQ(nullptr, commandQueue);

    desc.ordinal = 0;
    desc.index = 0x1000;
    res = context->createCommandQueue(device, &desc, &commandQueue);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);
    EXPECT_EQ(nullptr, commandQueue);
}

HWTEST_F(ContextCreateCommandQueueTest, givenRootDeviceAndImplicitScalingDisabledWhenCreatingCommandQueueThenValidateQueueOrdinalUsingSubDeviceEngines) {
    NEO::UltDeviceFactory deviceFactory{1, 2};
    auto &rootDevice = *deviceFactory.rootDevices[0];
    auto &subDevice0 = *deviceFactory.subDevices[0];
    rootDevice.regularEngineGroups.resize(1);
    subDevice0.getRegularEngineGroups().push_back(NEO::Device::EngineGroupT{});
    subDevice0.getRegularEngineGroups().back().engineGroupType = EngineGroupType::Compute;
    subDevice0.getRegularEngineGroups().back().engines.resize(1);
    subDevice0.getRegularEngineGroups().back().engines[0].commandStreamReceiver = &rootDevice.getGpgpuCommandStreamReceiver();
    auto ordinal = static_cast<uint32_t>(subDevice0.getRegularEngineGroups().size() - 1);
    Mock<L0::DeviceImp> l0RootDevice(&rootDevice, rootDevice.getExecutionEnvironment());

    ze_command_queue_handle_t commandQueue = nullptr;
    ze_command_queue_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    desc.ordinal = ordinal;
    desc.index = 0;

    l0RootDevice.implicitScalingCapable = true;
    ze_result_t res = context->createCommandQueue(l0RootDevice.toHandle(), &desc, &commandQueue);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);
    EXPECT_EQ(nullptr, commandQueue);

    l0RootDevice.implicitScalingCapable = false;
    res = context->createCommandQueue(l0RootDevice.toHandle(), &desc, &commandQueue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, commandQueue);
    L0::CommandQueue::fromHandle(commandQueue)->destroy();
}

using AubCsrTest = Test<AubCsrFixture>;

HWTEST_TEMPLATED_F(AubCsrTest, givenAubCsrWhenCallingExecuteCommandListsThenPollForCompletionIsCalled) {
    auto csr = neoDevice->getDefaultEngine().commandStreamReceiver;
    ze_result_t returnValue;
    ze_command_queue_desc_t desc = {};
    ze_command_queue_handle_t commandQueue = {};
    ze_result_t res = context->createCommandQueue(device, &desc, &commandQueue);
    ASSERT_EQ(ZE_RESULT_SUCCESS, res);
    ASSERT_NE(nullptr, commandQueue);

    auto aub_csr = static_cast<NEO::UltAubCommandStreamReceiver<FamilyType> *>(csr);
    CommandQueue *queue = static_cast<CommandQueue *>(L0::CommandQueue::fromHandle(commandQueue));
    queue->setCommandQueuePreemptionMode(PreemptionMode::Disabled);
    EXPECT_EQ(aub_csr->pollForCompletionCalled, 0u);

    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::Compute, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    auto commandListHandle = commandList->toHandle();
    queue->executeCommandLists(1, &commandListHandle, nullptr, false);
    EXPECT_EQ(aub_csr->pollForCompletionCalled, 1u);

    L0::CommandQueue::fromHandle(commandQueue)->destroy();
}

using CommandQueueSynchronizeTest = Test<ContextFixture>;
using MultiTileCommandQueueSynchronizeTest = Test<SingleRootMultiSubDeviceFixture>;

template <typename GfxFamily>
struct SynchronizeCsr : public NEO::UltCommandStreamReceiver<GfxFamily> {

    SynchronizeCsr(const NEO::ExecutionEnvironment &executionEnvironment, const DeviceBitfield deviceBitfield)
        : NEO::UltCommandStreamReceiver<GfxFamily>(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment), 0, deviceBitfield) {
        CommandStreamReceiver::tagAddress = &tagAddressData[0];
        memset(const_cast<uint32_t *>(CommandStreamReceiver::tagAddress), 0xFFFFFFFF, tagSize * sizeof(uint32_t));
    }

    bool waitForCompletionWithTimeout(bool enableTimeout, int64_t timeoutMs, uint32_t taskCountToWait) override {
        enableTimeoutSet = enableTimeout;
        waitForComplitionCalledTimes++;
        partitionCountSet = this->activePartitions;
        return true;
    }

    void waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool quickKmdSleep, bool forcePowerSavingMode) override {
        waitForTaskCountWithKmdNotifyFallbackCalled++;
        NEO::UltCommandStreamReceiver<GfxFamily>::waitForTaskCountWithKmdNotifyFallback(taskCountToWait, flushStampToWait, quickKmdSleep, forcePowerSavingMode);
    }

    static constexpr size_t tagSize = 128;
    static volatile uint32_t tagAddressData[tagSize];
    uint32_t waitForComplitionCalledTimes = 0;
    uint32_t waitForTaskCountWithKmdNotifyFallbackCalled = 0;
    uint32_t partitionCountSet = 0;
    bool enableTimeoutSet = false;
};

template <typename GfxFamily>
volatile uint32_t SynchronizeCsr<GfxFamily>::tagAddressData[SynchronizeCsr<GfxFamily>::tagSize];

HWTEST_F(CommandQueueSynchronizeTest, givenCallToSynchronizeThenCorrectEnableTimeoutAndTimeoutValuesAreUsed) {
    auto csr = std::unique_ptr<SynchronizeCsr<FamilyType>>(new SynchronizeCsr<FamilyType>(*device->getNEODevice()->getExecutionEnvironment(),
                                                                                          device->getNEODevice()->getDeviceBitfield()));

    ze_command_queue_desc_t desc = {};
    ze_command_queue_handle_t commandQueue = {};
    ze_result_t res = context->createCommandQueue(device, &desc, &commandQueue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, commandQueue);

    CommandQueue *queue = reinterpret_cast<CommandQueue *>(L0::CommandQueue::fromHandle(commandQueue));
    queue->csr = csr.get();

    uint64_t timeout = 10;
    int64_t timeoutMicrosecondsExpected = timeout;

    queue->synchronize(timeout);

    EXPECT_EQ(1u, csr->waitForComplitionCalledTimes);
    EXPECT_EQ(0u, csr->waitForTaskCountWithKmdNotifyFallbackCalled);
    EXPECT_TRUE(csr->enableTimeoutSet);

    timeout = std::numeric_limits<uint64_t>::max();
    timeoutMicrosecondsExpected = NEO::TimeoutControls::maxTimeout;

    queue->synchronize(timeout);

    EXPECT_EQ(2u, csr->waitForComplitionCalledTimes);
    EXPECT_EQ(0u, csr->waitForTaskCountWithKmdNotifyFallbackCalled);
    EXPECT_FALSE(csr->enableTimeoutSet);

    L0::CommandQueue::fromHandle(commandQueue)->destroy();
}

HWTEST_F(CommandQueueSynchronizeTest, givenDebugOverrideEnabledWhenCallToSynchronizeThenCorrectEnableTimeoutAndTimeoutValuesAreUsed) {
    DebugManagerStateRestore restore;
    NEO::DebugManager.flags.OverrideUseKmdWaitFunction.set(1);

    auto csr = std::unique_ptr<SynchronizeCsr<FamilyType>>(new SynchronizeCsr<FamilyType>(*device->getNEODevice()->getExecutionEnvironment(),
                                                                                          device->getNEODevice()->getDeviceBitfield()));

    ze_command_queue_desc_t desc = {};
    ze_command_queue_handle_t commandQueue = {};
    ze_result_t res = context->createCommandQueue(device, &desc, &commandQueue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, commandQueue);

    CommandQueue *queue = reinterpret_cast<CommandQueue *>(L0::CommandQueue::fromHandle(commandQueue));
    queue->csr = csr.get();

    uint64_t timeout = 10;
    bool enableTimeoutExpected = true;
    int64_t timeoutMicrosecondsExpected = timeout;

    queue->synchronize(timeout);

    EXPECT_EQ(1u, csr->waitForComplitionCalledTimes);
    EXPECT_EQ(0u, csr->waitForTaskCountWithKmdNotifyFallbackCalled);
    EXPECT_TRUE(csr->enableTimeoutSet);

    timeout = std::numeric_limits<uint64_t>::max();
    enableTimeoutExpected = false;
    timeoutMicrosecondsExpected = NEO::TimeoutControls::maxTimeout;

    queue->synchronize(timeout);

    EXPECT_EQ(2u, csr->waitForComplitionCalledTimes);
    EXPECT_EQ(1u, csr->waitForTaskCountWithKmdNotifyFallbackCalled);
    EXPECT_FALSE(csr->enableTimeoutSet);

    L0::CommandQueue::fromHandle(commandQueue)->destroy();
}

HWTEST2_F(MultiTileCommandQueueSynchronizeTest, givenMultiplePartitionCountWhenCallingSynchronizeThenExpectTheSameNumberCsrSynchronizeCalls, IsAtLeastXeHpCore) {
    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;

    auto csr = reinterpret_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(neoDevice->getDefaultEngine().commandStreamReceiver);
    if (device->getNEODevice()->getPreemptionMode() == PreemptionMode::MidThread || device->getNEODevice()->isDebuggerActive()) {
        csr->createPreemptionAllocation();
    }
    EXPECT_NE(0u, csr->getPostSyncWriteOffset());
    volatile uint32_t *tagAddress = csr->getTagAddress();
    for (uint32_t i = 0; i < 2; i++) {
        *tagAddress = 0xFF;
        tagAddress = ptrOffset(tagAddress, csr->getPostSyncWriteOffset());
    }
    csr->activePartitions = 2u;
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                           device,
                                                           neoDevice->getDefaultEngine().commandStreamReceiver,
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
    ASSERT_NE(nullptr, commandQueue);
    EXPECT_EQ(2u, commandQueue->activeSubDevices);

    auto commandList = std::unique_ptr<CommandList>(whitebox_cast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    ASSERT_NE(nullptr, commandList);
    commandList->partitionCount = 2;

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    returnValue = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, false);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    uint64_t timeout = std::numeric_limits<uint64_t>::max();
    commandQueue->synchronize(timeout);

    L0::CommandQueue::fromHandle(commandQueue)->destroy();
}

HWTEST2_F(MultiTileCommandQueueSynchronizeTest, givenCsrHasMultipleActivePartitionWhenExecutingCmdListOnNewCmdQueueThenExpectCmdPartitionCountMatchCsrActivePartitions, IsAtLeastXeHpCore) {
    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;

    auto csr = reinterpret_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(neoDevice->getDefaultEngine().commandStreamReceiver);
    if (device->getNEODevice()->getPreemptionMode() == PreemptionMode::MidThread || device->getNEODevice()->isDebuggerActive()) {
        csr->createPreemptionAllocation();
    }
    EXPECT_NE(0u, csr->getPostSyncWriteOffset());
    volatile uint32_t *tagAddress = csr->getTagAddress();
    for (uint32_t i = 0; i < 2; i++) {
        *tagAddress = 0xFF;
        tagAddress = ptrOffset(tagAddress, csr->getPostSyncWriteOffset());
    }
    csr->activePartitions = 2u;
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                           device,
                                                           neoDevice->getDefaultEngine().commandStreamReceiver,
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
    ASSERT_NE(nullptr, commandQueue);
    EXPECT_EQ(2u, commandQueue->activeSubDevices);

    auto commandList = std::unique_ptr<CommandList>(whitebox_cast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    ASSERT_NE(nullptr, commandList);

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, false);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    EXPECT_EQ(2u, commandQueue->partitionCount);

    L0::CommandQueue::fromHandle(commandQueue)->destroy();
}

HWTEST_F(CommandQueueSynchronizeTest, givenSingleTileCsrWhenExecutingMultiTileCommandListThenExpectErrorOnExecute) {
    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;

    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                           device,
                                                           neoDevice->getDefaultEngine().commandStreamReceiver,
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
    ASSERT_NE(nullptr, commandQueue);
    EXPECT_EQ(1u, commandQueue->activeSubDevices);

    auto commandList = std::unique_ptr<CommandList>(whitebox_cast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    ASSERT_NE(nullptr, commandList);
    commandList->partitionCount = 2;

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    returnValue = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, false);
    EXPECT_EQ(returnValue, ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE);

    L0::CommandQueue::fromHandle(commandQueue)->destroy();
}

template <typename GfxFamily>
struct TestCmdQueueCsr : public NEO::UltCommandStreamReceiver<GfxFamily> {
    TestCmdQueueCsr(const NEO::ExecutionEnvironment &executionEnvironment, const DeviceBitfield deviceBitfield)
        : NEO::UltCommandStreamReceiver<GfxFamily>(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment), 0, deviceBitfield) {
    }
    MOCK_METHOD3(waitForCompletionWithTimeout, bool(bool enableTimeout, int64_t timeoutMs, uint32_t taskCountToWait));
};

HWTEST_F(CommandQueueSynchronizeTest, givenSinglePartitionCountWhenWaitFunctionFailsThenReturnNotReady) {
    auto csr = std::unique_ptr<TestCmdQueueCsr<FamilyType>>(new TestCmdQueueCsr<FamilyType>(*device->getNEODevice()->getExecutionEnvironment(),
                                                                                            device->getNEODevice()->getDeviceBitfield()));
    csr->setupContext(*device->getNEODevice()->getDefaultEngine().osContext);

    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                           device,
                                                           csr.get(),
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
    ASSERT_NE(nullptr, commandQueue);

    EXPECT_CALL(*csr, waitForCompletionWithTimeout(::testing::_,
                                                   ::testing::_,
                                                   ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(false));

    uint64_t timeout = std::numeric_limits<uint64_t>::max();
    returnValue = commandQueue->synchronize(timeout);
    EXPECT_EQ(returnValue, ZE_RESULT_NOT_READY);

    commandQueue->destroy();
}

struct MemoryManagerCommandQueueCreateNegativeTest : public NEO::MockMemoryManager {
    MemoryManagerCommandQueueCreateNegativeTest(NEO::ExecutionEnvironment &executionEnvironment) : NEO::MockMemoryManager(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment)) {}
    NEO::GraphicsAllocation *allocateGraphicsMemoryWithProperties(const NEO::AllocationProperties &properties) override {
        if (forceFailureInPrimaryAllocation) {
            return nullptr;
        }
        return NEO::MemoryManager::allocateGraphicsMemoryWithProperties(properties);
    }
    bool forceFailureInPrimaryAllocation = false;
};

struct CommandQueueCreateNegativeTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (uint32_t i = 0; i < numRootDevices; i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(NEO::defaultHwInfo.get());
        }

        memoryManager = new MemoryManagerCommandQueueCreateNegativeTest(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);

        std::vector<std::unique_ptr<NEO::Device>> devices;
        for (uint32_t i = 0; i < numRootDevices; i++) {
            neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, i);
            devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        }

        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));

        device = driverHandle->devices[0];
    }
    void TearDown() override {
    }

    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    MemoryManagerCommandQueueCreateNegativeTest *memoryManager = nullptr;
    const uint32_t numRootDevices = 1u;
};

TEST_F(CommandQueueCreateNegativeTest, whenDeviceAllocationFailsDuringCommandQueueCreateThenAppropriateValueIsReturned) {
    const ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);
    memoryManager->forceFailureInPrimaryAllocation = true;

    ze_result_t returnValue;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          csr.get(),
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, returnValue);
    ASSERT_EQ(nullptr, commandQueue);
}

struct CommandQueueInitTests : public ::testing::Test {
    class MyMemoryManager : public OsAgnosticMemoryManager {
      public:
        using OsAgnosticMemoryManager::OsAgnosticMemoryManager;

        NEO::GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) override {
            storedAllocationProperties.push_back(properties);
            return OsAgnosticMemoryManager::allocateGraphicsMemoryWithProperties(properties);
        }

        std::vector<AllocationProperties> storedAllocationProperties;
    };

    void SetUp() override {
        DebugManager.flags.CreateMultipleSubDevices.set(numSubDevices);

        auto executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());

        memoryManager = new MyMemoryManager(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);

        neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0);
        std::vector<std::unique_ptr<NEO::Device>> devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));

        device = driverHandle->devices[0];
    }

    VariableBackup<bool> mockDeviceFlagBackup{&NEO::MockDevice::createSingleDevice, false};
    DebugManagerStateRestore restore;

    NEO::MockDevice *neoDevice = nullptr;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    L0::Device *device = nullptr;
    MyMemoryManager *memoryManager = nullptr;
    const uint32_t numRootDevices = 1;
    const uint32_t numSubDevices = 4;
};

TEST_F(CommandQueueInitTests, givenMultipleSubDevicesWhenInitializingThenAllocateForAllSubDevices) {
    ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t returnValue;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily, device, csr.get(), &desc, false, false, returnValue);
    EXPECT_NE(nullptr, commandQueue);

    const uint64_t expectedBitfield = maxNBitValue(numSubDevices);

    uint32_t cmdBufferAllocationsFound = 0;
    for (auto &allocationProperties : memoryManager->storedAllocationProperties) {
        if (allocationProperties.allocationType == NEO::GraphicsAllocation::AllocationType::COMMAND_BUFFER) {
            cmdBufferAllocationsFound++;
            EXPECT_EQ(expectedBitfield, allocationProperties.subDevicesBitfield.to_ulong());
            EXPECT_EQ(1u, allocationProperties.flags.multiOsContextCapable);
        }
    }

    EXPECT_EQ(static_cast<uint32_t>(CommandQueueImp::CommandBufferManager::BUFFER_ALLOCATION::COUNT), cmdBufferAllocationsFound);

    commandQueue->destroy();
}

TEST_F(CommandQueueInitTests, whenDestroyCommandQueueThenStoreCommandBuffersAsReusableAllocations) {
    ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t returnValue;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily, device, csr.get(), &desc, false, false, returnValue);
    EXPECT_NE(nullptr, commandQueue);
    auto deviceImp = static_cast<DeviceImp *>(device);
    EXPECT_TRUE(deviceImp->allocationsForReuse->peekIsEmpty());

    commandQueue->destroy();

    EXPECT_FALSE(deviceImp->allocationsForReuse->peekIsEmpty());
}

struct DeviceWithDualStorage : Test<DeviceFixture> {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        DebugManager.flags.EnableLocalMemory.set(1);
        DebugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.set(1);
        DeviceFixture::SetUp();
    }
    void TearDown() override {
        DeviceFixture::TearDown();
    }
    DebugManagerStateRestore restorer;
};

HWTEST2_F(DeviceWithDualStorage, givenCmdListWithAppendedKernelAndUsmTransferAndBlitterDisabledWhenExecuteCmdListThenCfeStateOnceProgrammed, IsAtLeastXeHpCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<MockMemoryOperationsHandler>();
    ze_result_t res = ZE_RESULT_SUCCESS;

    const ze_command_queue_desc_t desc = {};
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                           device,
                                                           neoDevice->getInternalEngine().commandStreamReceiver,
                                                           &desc,
                                                           false,
                                                           false,
                                                           res));
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    ASSERT_NE(nullptr, commandQueue);

    auto commandList = std::unique_ptr<CommandList>(whitebox_cast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, res)));
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    ASSERT_NE(nullptr, commandList);
    Mock<Kernel> kernel;
    kernel.immutableData.device = device;
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    res = context->allocSharedMem(device->toHandle(),
                                  &deviceDesc,
                                  &hostDesc,
                                  size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto gpuAlloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(ptr)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    kernel.residencyContainer.push_back(gpuAlloc);

    ze_group_count_t dispatchFunctionArguments{1, 1, 1};
    commandList->appendLaunchKernel(kernel.toHandle(), &dispatchFunctionArguments, nullptr, 0, nullptr);
    auto deviceImp = static_cast<DeviceImp *>(device);
    auto pageFaultCmdQueue = whitebox_cast(deviceImp->pageFaultCommandList->cmdQImmediate);

    auto sizeBefore = commandQueue->commandStream->getUsed();
    auto pageFaultSizeBefore = pageFaultCmdQueue->commandStream->getUsed();
    auto handle = commandList->toHandle();
    commandQueue->executeCommandLists(1, &handle, nullptr, true);
    auto sizeAfter = commandQueue->commandStream->getUsed();
    auto pageFaultSizeAfter = pageFaultCmdQueue->commandStream->getUsed();
    EXPECT_LT(sizeBefore, sizeAfter);
    EXPECT_LT(pageFaultSizeBefore, pageFaultSizeAfter);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(commandQueue->commandStream->getCpuBase(), 0),
                                             sizeAfter);
    auto count = findAll<CFE_STATE *>(commands.begin(), commands.end()).size();
    EXPECT_EQ(0u, count);

    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(pageFaultCmdQueue->commandStream->getCpuBase(), 0),
                                             pageFaultSizeAfter);
    count = findAll<CFE_STATE *>(commands.begin(), commands.end()).size();
    EXPECT_EQ(1u, count);

    res = context->freeMem(ptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, res);
    commandQueue->destroy();
}

} // namespace ult
} // namespace L0
