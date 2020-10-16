/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/state_base_address.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/helpers/default_hw_info.h"
#include "shared/test/unit_test/mocks/mock_command_stream_receiver.h"

#include "opencl/test/unit_test/libult/ult_command_stream_receiver.h"
#include "test.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"

namespace L0 {
namespace ult {

using CommandQueueCreate = Test<DeviceFixture>;

TEST_F(CommandQueueCreate, whenCreatingCommandQueueThenItIsInitialized) {
    const ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());

    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          csr.get(),
                                                          &desc,
                                                          false);
    ASSERT_NE(nullptr, commandQueue);

    L0::CommandQueueImp *commandQueueImp = reinterpret_cast<L0::CommandQueueImp *>(commandQueue);

    EXPECT_EQ(csr.get(), commandQueueImp->getCsr());
    EXPECT_EQ(device, commandQueueImp->getDevice());
    EXPECT_EQ(0u, commandQueueImp->getTaskCount());

    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, whenCreatingCommandQueueWithInvalidProductFamilyThenFailureIsReturned) {
    const ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());

    L0::CommandQueue *commandQueue = CommandQueue::create(PRODUCT_FAMILY::IGFX_MAX_PRODUCT,
                                                          device,
                                                          csr.get(),
                                                          &desc,
                                                          false);
    ASSERT_EQ(nullptr, commandQueue);
}

using CommandQueueSBASupport = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;

struct MockMemoryManagerCommandQueueSBA : public MemoryManagerMock {
    MockMemoryManagerCommandQueueSBA(NEO::ExecutionEnvironment &executionEnvironment) : MemoryManagerMock(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment)) {}
    MOCK_METHOD2(getInternalHeapBaseAddress, uint64_t(uint32_t rootDeviceIndex, bool useLocalMemory));
};

struct CommandQueueProgramSBATest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (uint32_t i = 0; i < numRootDevices; i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(NEO::defaultHwInfo.get());
        }

        memoryManager = new ::testing::NiceMock<MockMemoryManagerCommandQueueSBA>(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);

        neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, rootDeviceIndex);
        std::vector<std::unique_ptr<NEO::Device>> devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

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
    MockMemoryManagerCommandQueueSBA *memoryManager = nullptr;
    const uint32_t rootDeviceIndex = 1u;
    const uint32_t numRootDevices = 2u;
};

HWTEST2_F(CommandQueueProgramSBATest, whenCreatingCommandQueueThenItIsInitialized, CommandQueueSBASupport) {
    ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    auto commandQueue = new MockCommandQueueHw<gfxCoreFamily>(device, csr.get(), &desc);
    commandQueue->initialize(false);

    uint32_t alignedSize = 4096u;
    NEO::LinearStream child(commandQueue->commandStream->getSpace(alignedSize), alignedSize);

    auto &hwHelper = HwHelper::get(neoDevice->getHardwareInfo().platform.eRenderCoreFamily);
    bool isaInLocalMemory = !hwHelper.useSystemMemoryPlacementForISA(neoDevice->getHardwareInfo());

    if (isaInLocalMemory) {
        EXPECT_CALL(*memoryManager, getInternalHeapBaseAddress(rootDeviceIndex, true))
            .Times(2);

        EXPECT_CALL(*memoryManager, getInternalHeapBaseAddress(rootDeviceIndex, false))
            .Times(0);
    } else {
        EXPECT_CALL(*memoryManager, getInternalHeapBaseAddress(rootDeviceIndex, true))
            .Times(1); // IOH

        EXPECT_CALL(*memoryManager, getInternalHeapBaseAddress(rootDeviceIndex, false))
            .Times(1); // instruction heap
    }

    commandQueue->programGeneralStateBaseAddress(0u, true, child);

    if (isaInLocalMemory) {
        EXPECT_CALL(*memoryManager, getInternalHeapBaseAddress(rootDeviceIndex, false))
            .Times(1); // IOH

        EXPECT_CALL(*memoryManager, getInternalHeapBaseAddress(rootDeviceIndex, true))
            .Times(1); // instruction heap
    } else {
        EXPECT_CALL(*memoryManager, getInternalHeapBaseAddress(rootDeviceIndex, true))
            .Times(0);

        EXPECT_CALL(*memoryManager, getInternalHeapBaseAddress(rootDeviceIndex, false))
            .Times(2);
    }

    commandQueue->programGeneralStateBaseAddress(0u, false, child);

    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, givenCmdQueueWithBlitCopyWhenExecutingNonCopyBlitCommandListThenWrongCommandListStatusReturned) {
    const ze_command_queue_desc_t desc = {};

    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());

    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          csr.get(),
                                                          &desc,
                                                          true);
    ASSERT_NE(nullptr, commandQueue);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue));
    auto commandListHandle = commandList->toHandle();
    auto status = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    EXPECT_EQ(status, ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE);

    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, givenCmdQueueWithBlitCopyWhenExecutingCopyBlitCommandListThenSuccessReturned) {
    const ze_command_queue_desc_t desc = {};

    auto defaultCsr = neoDevice->getDefaultEngine().commandStreamReceiver;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          defaultCsr,
                                                          &desc,
                                                          true);
    ASSERT_NE(nullptr, commandQueue);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, returnValue));
    auto commandListHandle = commandList->toHandle();
    auto status = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    EXPECT_EQ(status, ZE_RESULT_SUCCESS);

    commandQueue->destroy();
}

using CommandQueueDestroySupport = IsAtLeastProduct<IGFX_SKYLAKE>;
using CommandQueueDestroy = Test<DeviceFixture>;

HWTEST2_F(CommandQueueDestroy, whenCommandQueueDestroyIsCalledPrintPrintfOutputIsCalled, CommandQueueDestroySupport) {
    ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    auto commandQueue = new MockCommandQueueHw<gfxCoreFamily>(device, csr.get(), &desc);
    commandQueue->initialize(false);

    Mock<Kernel> kernel;
    commandQueue->printfFunctionContainer.push_back(&kernel);

    EXPECT_EQ(0u, kernel.printPrintfOutputCalledTimes);
    commandQueue->destroy();
    EXPECT_EQ(1u, kernel.printPrintfOutputCalledTimes);
}

using CommandQueueCommands = Test<DeviceFixture>;
HWTEST_F(CommandQueueCommands, givenCommandQueueWhenExecutingCommandListsThenHardwareContextIsProgrammedAndGlobalAllocationResident) {
    const ze_command_queue_desc_t desc = {};

    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0);
    csr.initializeTagAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          &csr,
                                                          &desc,
                                                          true);
    ASSERT_NE(nullptr, commandQueue);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, returnValue));
    auto commandListHandle = commandList->toHandle();
    auto status = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    auto globalFence = csr.getGlobalFenceAllocation();
    if (globalFence) {
        bool found = false;
        for (auto alloc : csr.copyOfAllocations) {
            if (alloc == globalFence) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found);
    }
    EXPECT_EQ(status, ZE_RESULT_SUCCESS);
    EXPECT_TRUE(csr.programHardwareContextCalled);
    commandQueue->destroy();
}

using CommandQueueIndirectAllocations = Test<ModuleFixture>;
HWTEST_F(CommandQueueIndirectAllocations, givenCommandQueueWhenExecutingCommandListsThenExpectedIndirectAllocationsAddedToResidencyContainer) {
    const ze_command_queue_desc_t desc = {};

    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0);
    csr.initializeTagAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          &csr,
                                                          &desc,
                                                          true);
    ASSERT_NE(nullptr, commandQueue);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, returnValue));

    void *deviceAlloc = nullptr;
    auto result = device->getDriverHandle()->allocDeviceMem(device->toHandle(), 0u, 16384u, 4096u, &deviceAlloc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto gpuAlloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(deviceAlloc)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, gpuAlloc);

    createKernel();
    kernel->unifiedMemoryControls.indirectDeviceAllocationsAllowed = true;
    EXPECT_TRUE(kernel->getUnifiedMemoryControls().indirectDeviceAllocationsAllowed);

    ze_group_count_t groupCount{1, 1, 1};
    result = commandList->appendLaunchKernel(kernel->toHandle(),
                                             &groupCount,
                                             nullptr,
                                             0,
                                             nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto itorEvent = std::find(std::begin(commandList->commandContainer.getResidencyContainer()),
                               std::end(commandList->commandContainer.getResidencyContainer()),
                               gpuAlloc);
    EXPECT_EQ(itorEvent, std::end(commandList->commandContainer.getResidencyContainer()));

    auto commandListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    itorEvent = std::find(std::begin(commandList->commandContainer.getResidencyContainer()),
                          std::end(commandList->commandContainer.getResidencyContainer()),
                          gpuAlloc);
    EXPECT_NE(itorEvent, std::end(commandList->commandContainer.getResidencyContainer()));

    device->getDriverHandle()->getSvmAllocsManager()->freeSVMAlloc(deviceAlloc);
    commandQueue->destroy();
}

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
    auto engines = neoDevice->getEngineGroups();
    uint32_t numaAvailableEngineGroups = 0;
    for (uint32_t ordinal = 0; ordinal < static_cast<uint32_t>(NEO::EngineGroupType::MaxEngineGroups); ordinal++) {
        if (engines[ordinal].size()) {
            numaAvailableEngineGroups++;
        }
    }
    for (uint32_t ordinal = 0; ordinal < numaAvailableEngineGroups; ordinal++) {
        uint32_t engineGroupIndex = ordinal;
        device->mapOrdinalForAvailableEngineGroup(&engineGroupIndex);
        for (uint32_t index = 0; index < engines[engineGroupIndex].size(); index++) {
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

HWTEST_F(ContextCreateCommandQueueTest, givenOrdinalBigerThanAvailableEnginesWhenCreatingCommandQueueThenInvalidArgReturned) {
    ze_command_queue_handle_t commandQueue = {};
    auto engines = neoDevice->getEngineGroups();
    uint32_t numaAvailableEngineGroups = 0;
    for (uint32_t ordinal = 0; ordinal < static_cast<uint32_t>(NEO::EngineGroupType::MaxEngineGroups); ordinal++) {
        if (engines[ordinal].size()) {
            numaAvailableEngineGroups++;
        }
    }
    ze_command_queue_desc_t desc = {};
    desc.ordinal = numaAvailableEngineGroups;
    desc.index = 0;
    ze_result_t res = context->createCommandQueue(device, &desc, &commandQueue);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);
    EXPECT_EQ(nullptr, commandQueue);
}

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandQueue : public L0::CommandQueueHw<gfxCoreFamily> {
  public:
    using L0::CommandQueueHw<gfxCoreFamily>::CommandQueueHw;

    MockCommandQueue(L0::Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc) : L0::CommandQueueHw<gfxCoreFamily>(device, csr, desc) {}
    using BaseClass = ::L0::CommandQueueHw<gfxCoreFamily>;

    using BaseClass::heapContainer;
    using BaseClass::residencyContainer;

    NEO::HeapContainer mockHeapContainer;
    void handleScratchSpace(NEO::ResidencyContainer &residency,
                            NEO::HeapContainer &heapContainer,
                            NEO::ScratchSpaceController *scratchController,
                            bool &gsbaState, bool &frontEndState) override {
        this->mockHeapContainer = heapContainer;
    }

    void programFrontEnd(uint64_t scratchAddress, NEO::LinearStream &commandStream) override {
        return;
    }
};

using CommandQueueExecuteTest = Test<DeviceFixture>;
using CommandQueueExecuteTestSupport = IsAtLeastProduct<IGFX_SKYLAKE>;

HWTEST2_F(CommandQueueDestroy, givenCommandQueueAndCommandListWithSshAndScratchWhenExecuteThenSshWasUsed, CommandQueueExecuteTestSupport) {
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    auto commandQueue = new MockCommandQueue<gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false);
    auto commandList = new CommandListCoreFamily<gfxCoreFamily>();
    commandList->initialize(device, NEO::EngineGroupType::Compute);
    commandList->commandListPerThreadScratchSize = 100u;
    auto commandListHandle = commandList->toHandle();

    void *alloc = alignedMalloc(0x100, 0x100);
    NEO::GraphicsAllocation graphicsAllocation1(0, NEO::GraphicsAllocation::AllocationType::BUFFER, alloc, 0u, 0u, 1u, MemoryPool::System4KBPages, 1u);
    NEO::GraphicsAllocation graphicsAllocation2(0, NEO::GraphicsAllocation::AllocationType::BUFFER, alloc, 0u, 0u, 1u, MemoryPool::System4KBPages, 1u);

    commandList->commandContainer.sshAllocations.push_back(&graphicsAllocation1);
    commandList->commandContainer.sshAllocations.push_back(&graphicsAllocation2);

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    EXPECT_EQ(commandQueue->mockHeapContainer.size(), 3u);
    commandQueue->destroy();
    commandList->destroy();
    alignedFree(alloc);
}

using ExecuteCommandListTests = Test<ContextFixture>;
HWTEST2_F(ExecuteCommandListTests, givenExecuteCommandListWhenItReturnsThenContainersAreEmpty, CommandQueueExecuteTestSupport) {
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    auto commandQueue = new MockCommandQueue<gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false);
    auto commandList = new CommandListCoreFamily<gfxCoreFamily>();
    commandList->initialize(device, NEO::EngineGroupType::Compute);
    commandList->commandListPerThreadScratchSize = 100u;
    auto commandListHandle = commandList->toHandle();

    void *alloc = alignedMalloc(0x100, 0x100);
    NEO::GraphicsAllocation graphicsAllocation1(0, NEO::GraphicsAllocation::AllocationType::BUFFER, alloc, 0u, 0u, 1u, MemoryPool::System4KBPages, 1u);
    NEO::GraphicsAllocation graphicsAllocation2(0, NEO::GraphicsAllocation::AllocationType::BUFFER, alloc, 0u, 0u, 1u, MemoryPool::System4KBPages, 1u);

    commandList->commandContainer.sshAllocations.push_back(&graphicsAllocation1);
    commandList->commandContainer.sshAllocations.push_back(&graphicsAllocation2);

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    EXPECT_EQ(0u, commandQueue->residencyContainer.size());
    EXPECT_EQ(0u, commandQueue->heapContainer.size());

    commandQueue->destroy();
    commandList->destroy();
    alignedFree(alloc);
}

using CommandQueueSynchronizeTest = Test<ContextFixture>;

HWTEST_F(CommandQueueSynchronizeTest, givenCallToSynchronizeThenCorrectEnableTimeoutAndTimeoutValuesAreUsed) {
    struct SynchronizeCsr : public NEO::UltCommandStreamReceiver<FamilyType> {
        ~SynchronizeCsr() override {
            delete tagAddress;
        }
        SynchronizeCsr(const NEO::ExecutionEnvironment &executionEnvironment) : NEO::UltCommandStreamReceiver<FamilyType>(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment), 0) {
            tagAddress = new uint32_t;
        }
        bool waitForCompletionWithTimeout(bool enableTimeout, int64_t timeoutMs, uint32_t taskCountToWait) override {
            waitForComplitionCalledTimes++;
            return true;
        }

        volatile uint32_t *getTagAddress() const override {
            return tagAddress;
        }
        uint32_t waitForComplitionCalledTimes = 0;
        uint32_t *tagAddress;
    };

    auto csr = std::unique_ptr<SynchronizeCsr>(new SynchronizeCsr(*device->getNEODevice()->getExecutionEnvironment()));
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

    EXPECT_EQ(csr->waitForComplitionCalledTimes, 1u);
    timeout = std::numeric_limits<uint64_t>::max();
    enableTimeoutExpected = false;
    timeoutMicrosecondsExpected = NEO::TimeoutControls::maxTimeout;

    queue->synchronize(timeout);

    EXPECT_EQ(csr->waitForComplitionCalledTimes, 2u);

    L0::CommandQueue::fromHandle(commandQueue)->destroy();
}

} // namespace ult
} // namespace L0
