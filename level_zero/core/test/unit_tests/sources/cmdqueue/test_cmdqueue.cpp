/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/scratch_space_controller_base.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "opencl/test/unit_test/libult/ult_command_stream_receiver.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
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
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);
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

    size_t commandStreamSize = MemoryConstants::kiloByte * 128u;
    ASSERT_NE(nullptr, commandQueue->commandStream);
    EXPECT_EQ(commandStreamSize, commandQueue->commandStream->getMaxAvailableSpace());
    EXPECT_EQ(commandQueue->buffers.getCurrentBufferAllocation(), commandQueue->commandStream->getGraphicsAllocation());
    EXPECT_LT(0u, commandQueue->commandStream->getAvailableSpace());

    EXPECT_EQ(csr.get(), commandQueue->getCsr());
    EXPECT_EQ(device, commandQueue->getDevice());
    EXPECT_EQ(0u, commandQueue->getTaskCount());
    EXPECT_NE(nullptr, commandQueue->buffers.getCurrentBufferAllocation());

    size_t expectedCommandBufferAllocationSize = commandStreamSize + MemoryConstants::cacheLineSize + NEO::CSRequirements::csOverfetchSize;
    expectedCommandBufferAllocationSize = alignUp(expectedCommandBufferAllocationSize, MemoryConstants::pageSize64k);

    size_t actualCommandBufferSize = commandQueue->buffers.getCurrentBufferAllocation()->getUnderlyingBufferSize();
    EXPECT_EQ(expectedCommandBufferAllocationSize, actualCommandBufferSize);

    returnValue = commandQueue->destroy();
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
}

TEST_F(CommandQueueCreate, whenSynchronizeByPollingTaskCountThenCallsPrintOutputOnPrintfFunctionsStoredAndClearsFunctionContainer) {
    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                           device,
                                                           neoDevice->getDefaultEngine().commandStreamReceiver,
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));

    Mock<Kernel> kernel1, kernel2;

    commandQueue->printfFunctionContainer.push_back(&kernel1);
    commandQueue->printfFunctionContainer.push_back(&kernel2);

    commandQueue->synchronizeByPollingForTaskCount(0u);

    EXPECT_EQ(0u, commandQueue->printfFunctionContainer.size());
    EXPECT_EQ(1u, kernel1.printPrintfOutputCalledTimes);
    EXPECT_EQ(1u, kernel2.printPrintfOutputCalledTimes);

    commandQueue->destroy();
}

HWTEST_F(CommandQueueCreate, whenReserveLinearStreamThenBufferAllocationSwitched) {
    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;

    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                           device,
                                                           neoDevice->getDefaultEngine().commandStreamReceiver,
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));

    size_t maxSize = commandQueue->commandStream->getMaxAvailableSpace();

    auto firstAllocation = commandQueue->commandStream->getGraphicsAllocation();
    EXPECT_EQ(firstAllocation, commandQueue->buffers.getCurrentBufferAllocation());

    uint32_t currentTaskCount = 33u;
    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.latestWaitForCompletionWithTimeoutTaskCount = currentTaskCount;

    commandQueue->commandStream->getSpace(maxSize - 16u);
    commandQueue->buffers.setCurrentFlushStamp(121u, 121u);
    size_t nextSize = 16u + 16u;
    commandQueue->reserveLinearStreamSize(nextSize);

    auto secondAllocation = commandQueue->commandStream->getGraphicsAllocation();
    EXPECT_EQ(secondAllocation, commandQueue->buffers.getCurrentBufferAllocation());
    EXPECT_NE(firstAllocation, secondAllocation);
    EXPECT_EQ(csr.latestWaitForCompletionWithTimeoutTaskCount, currentTaskCount);

    commandQueue->commandStream->getSpace(maxSize - 16u);
    commandQueue->buffers.setCurrentFlushStamp(244u, 244u);
    commandQueue->reserveLinearStreamSize(nextSize);

    auto thirdAllocation = commandQueue->commandStream->getGraphicsAllocation();
    EXPECT_EQ(thirdAllocation, commandQueue->buffers.getCurrentBufferAllocation());
    EXPECT_EQ(thirdAllocation, firstAllocation);
    EXPECT_NE(thirdAllocation, secondAllocation);
    EXPECT_EQ(csr.latestWaitForCompletionWithTimeoutTaskCount, 121u);

    commandQueue->commandStream->getSpace(maxSize - 16u);
    commandQueue->reserveLinearStreamSize(nextSize);

    auto fourthAllocation = commandQueue->commandStream->getGraphicsAllocation();
    EXPECT_EQ(fourthAllocation, commandQueue->buffers.getCurrentBufferAllocation());
    EXPECT_EQ(fourthAllocation, secondAllocation);
    EXPECT_NE(fourthAllocation, firstAllocation);
    EXPECT_EQ(csr.latestWaitForCompletionWithTimeoutTaskCount, 244u);

    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, whenCreatingCommandQueueWithInvalidProductFamilyThenFailureIsReturned) {
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);
    L0::CommandQueue *commandQueue = CommandQueue::create(PRODUCT_FAMILY::IGFX_MAX_PRODUCT,
                                                          device,
                                                          csr.get(),
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue);

    ASSERT_EQ(nullptr, commandQueue);
}

TEST_F(CommandQueueCreate, whenCmdBuffersAllocationsAreCreatedThenSizeIsNotLessThanQueuesLinearStreamSize) {
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                           device,
                                                           neoDevice->getDefaultEngine().commandStreamReceiver,
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));

    size_t maxSize = commandQueue->commandStream->getMaxAvailableSpace();

    auto sizeFirstBuffer = commandQueue->buffers.getCurrentBufferAllocation()->getUnderlyingBufferSize();
    EXPECT_LE(maxSize, sizeFirstBuffer);

    commandQueue->commandStream->getSpace(maxSize - 16u);
    size_t nextSize = 16u + 16u;
    commandQueue->reserveLinearStreamSize(nextSize);

    auto sizeSecondBuffer = commandQueue->buffers.getCurrentBufferAllocation()->getUnderlyingBufferSize();
    EXPECT_LE(maxSize, sizeSecondBuffer);

    commandQueue->destroy();
}

HWTEST_F(CommandQueueCreate, given100CmdListsWhenExecutingThenCommandStreamIsNotDepleted) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                           device,
                                                           neoDevice->getDefaultEngine().commandStreamReceiver,
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));
    ASSERT_NE(nullptr, commandQueue);

    Mock<Kernel> kernel;
    kernel.immutableData.device = device;

    auto commandList = std::unique_ptr<CommandList>(whitebox_cast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    ASSERT_NE(nullptr, commandList);

    ze_group_count_t dispatchFunctionArguments{1, 1, 1};
    commandList->appendLaunchKernel(kernel.toHandle(), &dispatchFunctionArguments, nullptr, 0, nullptr);

    const size_t numHandles = 100;
    ze_command_list_handle_t cmdListHandles[numHandles];
    for (size_t i = 0; i < numHandles; i++) {
        cmdListHandles[i] = commandList->toHandle();
    }

    auto sizeBefore = commandQueue->commandStream->getUsed();
    commandQueue->executeCommandLists(numHandles, cmdListHandles, nullptr, false);
    auto sizeAfter = commandQueue->commandStream->getUsed();
    EXPECT_LT(sizeBefore, sizeAfter);

    size_t streamSizeMinimum =
        sizeof(MI_BATCH_BUFFER_END) +
        numHandles * sizeof(MI_BATCH_BUFFER_START);

    EXPECT_LE(streamSizeMinimum, sizeAfter - sizeBefore);

    size_t maxSize = 2 * streamSizeMinimum;
    EXPECT_GT(maxSize, sizeAfter - sizeBefore);

    commandQueue->destroy();
}

HWTEST_F(CommandQueueCreate, givenContainerWithAllocationsWhenResidencyContainerIsEmptyThenMakeResidentWasNotCalled) {
    auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                           device,
                                                           csr.get(),
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));
    ResidencyContainer container;
    commandQueue->submitBatchBuffer(0, container, nullptr);
    EXPECT_EQ(csr->makeResidentCalledTimes, 0u);

    EXPECT_EQ(commandQueue->commandStream->getGraphicsAllocation()->getTaskCount(commandQueue->csr->getOsContext().getContextId()), commandQueue->csr->peekTaskCount());
    EXPECT_EQ(commandQueue->commandStream->getGraphicsAllocation()->getTaskCount(commandQueue->csr->getOsContext().getContextId()), commandQueue->csr->peekTaskCount());
    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, whenCommandQueueCreatedThenExpectLinearStreamInitializedWithExpectedSize) {
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;

    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                           device,
                                                           neoDevice->getDefaultEngine().commandStreamReceiver,
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));
    ASSERT_NE(commandQueue, nullptr);

    size_t commandStreamSize = MemoryConstants::kiloByte * 128u;
    EXPECT_EQ(commandStreamSize, commandQueue->commandStream->getMaxAvailableSpace());

    size_t expectedCommandBufferAllocationSize = commandStreamSize + MemoryConstants::cacheLineSize + NEO::CSRequirements::csOverfetchSize;
    expectedCommandBufferAllocationSize = alignUp(expectedCommandBufferAllocationSize, MemoryConstants::pageSize64k);
    size_t actualCommandBufferSize = commandQueue->buffers.getCurrentBufferAllocation()->getUnderlyingBufferSize();
    EXPECT_EQ(expectedCommandBufferAllocationSize, actualCommandBufferSize);

    commandQueue->destroy();
}

HWTEST_F(CommandQueueCreate, givenQueueInAsyncModeAndRugularCmdListWithAppendBarrierThenFlushTaskIsNotUsed) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                           device,
                                                           neoDevice->getDefaultEngine().commandStreamReceiver,
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));
    ASSERT_NE(nullptr, commandQueue);

    auto commandList = std::unique_ptr<CommandList>(whitebox_cast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    ASSERT_NE(nullptr, commandList);

    commandList->appendBarrier(nullptr, 0, nullptr);

    commandQueue->destroy();
}

HWTEST_F(CommandQueueCreate, givenQueueInSyncModeAndRugularCmdListWithAppendBarrierThenFlushTaskIsNotUsed) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                           device,
                                                           neoDevice->getDefaultEngine().commandStreamReceiver,
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));
    ASSERT_NE(nullptr, commandQueue);

    auto commandList = std::unique_ptr<CommandList>(whitebox_cast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    ASSERT_NE(nullptr, commandList);

    commandList->appendBarrier(nullptr, 0, nullptr);

    commandQueue->destroy();
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
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);
    auto commandQueue = new MockCommandQueueHw<gfxCoreFamily>(device, csr.get(), &desc);
    commandQueue->initialize(false, false);

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

    commandQueue->programStateBaseAddress(0u, true, child);

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

    commandQueue->programStateBaseAddress(0u, false, child);

    commandQueue->destroy();
}

HWTEST2_F(CommandQueueProgramSBATest,
          whenProgrammingStateBaseAddressWithcontainsStatelessUncachedResourceThenCorrectMocsAreSet, CommandQueueSBASupport) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);
    auto commandQueue = new MockCommandQueueHw<gfxCoreFamily>(device, csr.get(), &desc);
    commandQueue->initialize(false, false);

    uint32_t alignedSize = 4096u;
    NEO::LinearStream child(commandQueue->commandStream->getSpace(alignedSize), alignedSize);

    commandQueue->programStateBaseAddress(0u, true, child);
    auto pSbaCmd = static_cast<STATE_BASE_ADDRESS *>(commandQueue->commandStream->getSpace(sizeof(STATE_BASE_ADDRESS)));
    uint32_t statelessMocsIndex = pSbaCmd->getStatelessDataPortAccessMemoryObjectControlState();

    auto gmmHelper = device->getNEODevice()->getGmmHelper();
    uint32_t expectedMocs = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED);
    EXPECT_EQ(statelessMocsIndex, expectedMocs);

    commandQueue->destroy();
}

using BindlessCommandQueueSBASupport = IsAtLeastProduct<IGFX_SKYLAKE>;

HWTEST2_F(CommandQueueProgramSBATest,
          givenBindlessModeEnabledWhenProgrammingStateBaseAddressThenBindlessBaseAddressIsPassed, BindlessCommandQueueSBASupport) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(1);
    auto bindlessHeapsHelper = std::make_unique<MockBindlesHeapsHelper>(neoDevice->getMemoryManager(), neoDevice->getNumAvailableDevices() > 1, neoDevice->getRootDeviceIndex());
    MockBindlesHeapsHelper *bindlessHeapsHelperPtr = bindlessHeapsHelper.get();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapsHelper.release());
    NEO::MockGraphicsAllocation baseAllocation;
    bindlessHeapsHelperPtr->surfaceStateHeaps[NEO::BindlessHeapsHelper::GLOBAL_SSH].reset(new IndirectHeap(&baseAllocation, true));
    baseAllocation.setGpuBaseAddress(0x123000);
    ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);
    auto commandQueue = new MockCommandQueueHw<gfxCoreFamily>(device, csr.get(), &desc);
    commandQueue->initialize(false, false);

    uint32_t alignedSize = 4096u;
    NEO::LinearStream child(commandQueue->commandStream->getSpace(alignedSize), alignedSize);

    commandQueue->programStateBaseAddress(0u, true, child);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);
    EXPECT_EQ(cmdSba->getBindlessSurfaceStateBaseAddressModifyEnable(), true);
    EXPECT_EQ(cmdSba->getBindlessSurfaceStateBaseAddress(), neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->getBindlessHeapsHelper()->getGlobalHeapsBase());
    EXPECT_EQ(cmdSba->getBindlessSurfaceStateSize(), MemoryConstants::sizeOf4GBinPageEntities);

    commandQueue->destroy();
}

HWTEST2_F(CommandQueueProgramSBATest,
          givenBindlessModeDisabledWhenProgrammingStateBaseAddressThenBindlessBaseAddressNotPassed, CommandQueueSBASupport) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(0);
    auto bindlessHeapsHelper = std::make_unique<MockBindlesHeapsHelper>(neoDevice->getMemoryManager(), neoDevice->getNumAvailableDevices() > 1, neoDevice->getRootDeviceIndex());
    MockBindlesHeapsHelper *bindlessHeapsHelperPtr = bindlessHeapsHelper.get();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapsHelper.release());
    NEO::MockGraphicsAllocation baseAllocation;
    bindlessHeapsHelperPtr->surfaceStateHeaps[NEO::BindlessHeapsHelper::GLOBAL_SSH].reset(new IndirectHeap(&baseAllocation, true));
    baseAllocation.setGpuBaseAddress(0x123000);
    ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);
    auto commandQueue = new MockCommandQueueHw<gfxCoreFamily>(device, csr.get(), &desc);
    commandQueue->initialize(false, false);

    uint32_t alignedSize = 4096u;
    NEO::LinearStream child(commandQueue->commandStream->getSpace(alignedSize), alignedSize);

    commandQueue->programStateBaseAddress(0u, true, child);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);
    EXPECT_NE(cmdSba->getBindlessSurfaceStateBaseAddress(), neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->getBindlessHeapsHelper()->getGlobalHeapsBase());

    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, givenCmdQueueWithBlitCopyWhenExecutingNonCopyBlitCommandListThenWrongCommandListStatusReturned) {
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          csr.get(),
                                                          &desc,
                                                          true,
                                                          false,
                                                          returnValue);
    ASSERT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    auto commandListHandle = commandList->toHandle();
    auto status = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    EXPECT_EQ(status, ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE);

    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, givenCmdQueueWithBlitCopyWhenExecutingCopyBlitCommandListThenSuccessReturned) {
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          true,
                                                          false,
                                                          returnValue);
    ASSERT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    auto commandListHandle = commandList->toHandle();
    auto status = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    EXPECT_EQ(status, ZE_RESULT_SUCCESS);

    commandQueue->destroy();
}

using CommandQueueDestroySupport = IsAtLeastProduct<IGFX_SKYLAKE>;
using CommandQueueDestroy = Test<DeviceFixture>;

template <bool multiTile>
struct CommandQueueCommands : DeviceFixture, ::testing::Test {
    void SetUp() override {
        DebugManager.flags.ForcePreemptionMode.set(static_cast<int>(NEO::PreemptionMode::Disabled));
        DebugManager.flags.CreateMultipleSubDevices.set(multiTile ? 2 : 1);
        DeviceFixture::SetUp();
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }

    template <typename FamilyType>
    bool isAllocationInResidencyContainer(MockCsrHw2<FamilyType> &csr, NEO::GraphicsAllocation *graphicsAllocation) {
        for (auto alloc : csr.copyOfAllocations) {
            if (alloc == graphicsAllocation) {
                return true;
            }
        }
        return false;
    }

    const ze_command_queue_desc_t desc = {};
    DebugManagerStateRestore restore{};
    VariableBackup<bool> mockDeviceFlagBackup{&NEO::MockDevice::createSingleDevice, false};
};
using CommandQueueCommandsSingleTile = CommandQueueCommands<false>;
using CommandQueueCommandsMultiTile = CommandQueueCommands<true>;

HWTEST_F(CommandQueueCommandsSingleTile, givenCommandQueueWhenExecutingCommandListsThenHardwareContextIsProgrammedAndGlobalAllocationResident) {
    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t returnValue;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          &csr,
                                                          &desc,
                                                          true,
                                                          false,
                                                          returnValue);
    ASSERT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, 0u, returnValue));
    auto commandListHandle = commandList->toHandle();
    auto status = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    auto globalFence = csr.getGlobalFenceAllocation();
    if (globalFence) {
        EXPECT_TRUE(isAllocationInResidencyContainer(csr, globalFence));
    }
    EXPECT_EQ(status, ZE_RESULT_SUCCESS);
    EXPECT_TRUE(csr.programHardwareContextCalled);
    commandQueue->destroy();
}

HWTEST_F(CommandQueueCommandsMultiTile, givenCommandQueueOnMultiTileWhenExecutingCommandListsThenWorkPartitionAllocationIsMadeResident) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableWalkerPartition.set(1);

    class MyCsrMock : public MockCsrHw2<FamilyType> {
        using MockCsrHw2<FamilyType>::MockCsrHw2;

      public:
        void makeResident(GraphicsAllocation &graphicsAllocation) override {
            if (expectedGa == &graphicsAllocation) {
                expectedGAWasMadeResident = true;
            }
        }
        GraphicsAllocation *expectedGa = nullptr;
        bool expectedGAWasMadeResident = false;
    };
    MyCsrMock csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.createWorkPartitionAllocation(*neoDevice);
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t returnValue;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          &csr,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue);
    ASSERT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Compute, 0u, returnValue));
    auto commandListHandle = commandList->toHandle();
    auto workPartitionAllocation = csr.getWorkPartitionAllocation();
    csr.expectedGa = workPartitionAllocation;
    auto status = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
    EXPECT_EQ(status, ZE_RESULT_SUCCESS);

    ASSERT_NE(nullptr, workPartitionAllocation);
    EXPECT_TRUE(csr.expectedGAWasMadeResident);

    commandQueue->destroy();
}

HWTEST_F(CommandQueueCommandsMultiTile, givenCommandQueueOnMultiTileWhenWalkerPartitionIsDisabledThenWorkPartitionAllocationIsNotCreated) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableWalkerPartition.set(0);

    class MyCsrMock : public MockCsrHw2<FamilyType> {
        using MockCsrHw2<FamilyType>::MockCsrHw2;

      public:
        void makeResident(GraphicsAllocation &graphicsAllocation) override {
            if (expectedGa == &graphicsAllocation) {
                expectedGAWasMadeResident = true;
            }
        }
        GraphicsAllocation *expectedGa = nullptr;
        bool expectedGAWasMadeResident = false;
    };
    MyCsrMock csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.createWorkPartitionAllocation(*neoDevice);
    auto workPartitionAllocation = csr.getWorkPartitionAllocation();
    EXPECT_EQ(nullptr, workPartitionAllocation);
}

using CommandQueueIndirectAllocations = Test<ModuleFixture>;
HWTEST_F(CommandQueueIndirectAllocations, givenCommandQueueWhenExecutingCommandListsThenExpectedIndirectAllocationsAddedToResidencyContainer) {
    const ze_command_queue_desc_t desc = {};

    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t returnValue;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          &csr,
                                                          &desc,
                                                          true,
                                                          false,
                                                          returnValue);
    ASSERT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, 0u, returnValue));

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &deviceAlloc);
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

using DeviceCreateCommandQueueTest = Test<DeviceFixture>;
TEST_F(DeviceCreateCommandQueueTest, givenLowPriorityDescWhenCreateCommandQueueIsCalledThenLowPriorityCsrIsAssigned) {
    ze_command_queue_desc_t desc{};
    desc.ordinal = 0u;
    desc.index = 0u;
    desc.priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW;

    ze_command_queue_handle_t commandQueueHandle = {};

    ze_result_t res = device->createCommandQueue(&desc, &commandQueueHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto commandQueue = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle));
    EXPECT_NE(commandQueue, nullptr);
    EXPECT_TRUE(commandQueue->getCsr()->getOsContext().isLowPriority());
    NEO::CommandStreamReceiver *csr = nullptr;
    device->getCsrForLowPriority(&csr);
    EXPECT_EQ(commandQueue->getCsr(), csr);
    commandQueue->destroy();
}

struct DeferredContextCreationDeviceCreateCommandQueueTest : DeviceCreateCommandQueueTest {
    void SetUp() override {
        DebugManager.flags.DeferOsContextInitialization.set(1);
        DeviceCreateCommandQueueTest::SetUp();
    }

    DebugManagerStateRestore restore;
};

TEST_F(DeferredContextCreationDeviceCreateCommandQueueTest, givenLowPriorityEngineNotInitializedWhenCreateLowPriorityCommandQueueIsCalledThenEngineIsInitialized) {
    NEO::CommandStreamReceiver *lowPriorityCsr = nullptr;
    device->getCsrForLowPriority(&lowPriorityCsr);
    ASSERT_FALSE(lowPriorityCsr->getOsContext().isInitialized());

    ze_command_queue_desc_t desc{};
    desc.ordinal = 0u;
    desc.index = 0u;
    desc.priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW;
    ze_command_queue_handle_t commandQueueHandle = {};
    ze_result_t res = device->createCommandQueue(&desc, &commandQueueHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_TRUE(lowPriorityCsr->getOsContext().isInitialized());

    auto commandQueue = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle));
    commandQueue->destroy();
}

TEST_F(DeviceCreateCommandQueueTest, givenNormalPriorityDescWhenCreateCommandQueueIsCalledWithValidArgumentThenCsrIsAssignedWithOrdinalAndIndex) {
    ze_command_queue_desc_t desc{};
    desc.ordinal = 0u;
    desc.index = 0u;
    desc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    ze_command_queue_handle_t commandQueueHandle = {};

    ze_result_t res = device->createCommandQueue(&desc, &commandQueueHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto commandQueue = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle));
    EXPECT_NE(commandQueue, nullptr);
    EXPECT_FALSE(commandQueue->getCsr()->getOsContext().isLowPriority());
    NEO::CommandStreamReceiver *csr = nullptr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    EXPECT_EQ(commandQueue->getCsr(), csr);
    commandQueue->destroy();
}

TEST_F(DeviceCreateCommandQueueTest,
       whenCallingGetCsrForOrdinalAndIndexWithInvalidOrdinalThenInvalidArgumentIsReturned) {
    ze_command_queue_desc_t desc{};
    desc.ordinal = 0u;
    desc.index = 0u;
    desc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    ze_command_queue_handle_t commandQueueHandle = {};

    ze_result_t res = device->createCommandQueue(&desc, &commandQueueHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto commandQueue = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle));
    EXPECT_NE(commandQueue, nullptr);
    EXPECT_FALSE(commandQueue->getCsr()->getOsContext().isLowPriority());
    NEO::CommandStreamReceiver *csr = nullptr;
    res = device->getCsrForOrdinalAndIndex(&csr, std::numeric_limits<uint32_t>::max(), 0u);
    EXPECT_EQ(res, ZE_RESULT_ERROR_INVALID_ARGUMENT);
    commandQueue->destroy();
}

TEST_F(DeviceCreateCommandQueueTest,
       whenCallingGetCsrForOrdinalAndIndexWithInvalidIndexThenInvalidArgumentIsReturned) {
    ze_command_queue_desc_t desc{};
    desc.ordinal = 0u;
    desc.index = 0u;
    desc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    ze_command_queue_handle_t commandQueueHandle = {};

    ze_result_t res = device->createCommandQueue(&desc, &commandQueueHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto commandQueue = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle));
    EXPECT_NE(commandQueue, nullptr);
    EXPECT_FALSE(commandQueue->getCsr()->getOsContext().isLowPriority());
    NEO::CommandStreamReceiver *csr = nullptr;
    res = device->getCsrForOrdinalAndIndex(&csr, 0u, std::numeric_limits<uint32_t>::max());
    EXPECT_EQ(res, ZE_RESULT_ERROR_INVALID_ARGUMENT);
    commandQueue->destroy();
}

TEST_F(DeviceCreateCommandQueueTest, givenLowPriorityDescAndWithoutLowPriorityCsrWhenCreateCommandQueueIsCalledThenAbortIsThrown) {
    // remove low priority EngineControl objects for negative testing
    neoDevice->engines.erase(std::remove_if(
        neoDevice->engines.begin(),
        neoDevice->engines.end(),
        [](EngineControl &p) { return p.osContext->isLowPriority(); }));

    ze_command_queue_desc_t desc{};
    desc.ordinal = 0u;
    desc.index = 0u;
    desc.priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW;

    ze_command_queue_handle_t commandQueueHandle = {};

    ze_result_t res{};
    EXPECT_THROW(res = device->createCommandQueue(&desc, &commandQueueHandle), std::exception);
}

using MultiDeviceCreateCommandQueueTest = Test<MultiDeviceFixture>;

TEST_F(MultiDeviceCreateCommandQueueTest, givenLowPriorityDescWhenCreateCommandQueueIsCalledThenLowPriorityCsrIsAssigned) {
    auto device = driverHandle->devices[0];

    ze_command_queue_desc_t desc{};
    desc.ordinal = 0u;
    desc.index = 0u;
    desc.priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW;

    ze_command_queue_handle_t commandQueueHandle = {};

    ze_result_t res = device->createCommandQueue(&desc, &commandQueueHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto commandQueue = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle));
    EXPECT_NE(commandQueue, nullptr);
    EXPECT_TRUE(commandQueue->getCsr()->getOsContext().isLowPriority());
    NEO::CommandStreamReceiver *csr = nullptr;
    device->getCsrForLowPriority(&csr);
    EXPECT_EQ(commandQueue->getCsr(), csr);
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
    auto &engines = neoDevice->getEngineGroups();
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
    auto &engines = neoDevice->getEngineGroups();
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

    using BaseClass::csr;
    using BaseClass::heapContainer;

    NEO::HeapContainer mockHeapContainer;
    void handleScratchSpace(NEO::HeapContainer &heapContainer,
                            NEO::ScratchSpaceController *scratchController,
                            bool &gsbaState, bool &frontEndState,
                            uint32_t perThreadScratchSpaceSize) override {
        this->mockHeapContainer = heapContainer;
    }

    void programFrontEnd(uint64_t scratchAddress, uint32_t perThreadScratchSpaceSize, NEO::LinearStream &commandStream) override {
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
    commandQueue->initialize(false, false);
    auto commandList = new CommandListCoreFamily<gfxCoreFamily>();
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    commandList->setCommandListPerThreadScratchSize(100u);
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

HWTEST2_F(CommandQueueDestroy, givenCommandQueueAndCommandListWithWhenBindlessEnabledThenHeapContainerIsEmpty, CommandQueueExecuteTestSupport) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(1);
    auto bindlessHeapsHelper = std::make_unique<MockBindlesHeapsHelper>(neoDevice->getMemoryManager(), neoDevice->getNumAvailableDevices() > 1, neoDevice->getRootDeviceIndex());
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapsHelper.release());
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    auto commandQueue = new MockCommandQueue<gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false);
    auto commandList = new CommandListCoreFamily<gfxCoreFamily>();
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    commandList->setCommandListPerThreadScratchSize(100u);
    auto commandListHandle = commandList->toHandle();

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    EXPECT_EQ(commandQueue->mockHeapContainer.size(), 0u);
    commandQueue->destroy();
    commandList->destroy();
}

using ExecuteCommandListTests = Test<ContextFixture>;
HWTEST2_F(ExecuteCommandListTests, givenExecuteCommandListWhenItReturnsThenContainersAreEmpty, CommandQueueExecuteTestSupport) {
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    auto commandQueue = new MockCommandQueue<gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false);
    auto commandList = new CommandListCoreFamily<gfxCoreFamily>();
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    commandList->setCommandListPerThreadScratchSize(100u);
    auto commandListHandle = commandList->toHandle();

    void *alloc = alignedMalloc(0x100, 0x100);
    NEO::GraphicsAllocation graphicsAllocation1(0, NEO::GraphicsAllocation::AllocationType::BUFFER, alloc, 0u, 0u, 1u, MemoryPool::System4KBPages, 1u);
    NEO::GraphicsAllocation graphicsAllocation2(0, NEO::GraphicsAllocation::AllocationType::BUFFER, alloc, 0u, 0u, 1u, MemoryPool::System4KBPages, 1u);

    commandList->commandContainer.sshAllocations.push_back(&graphicsAllocation1);
    commandList->commandContainer.sshAllocations.push_back(&graphicsAllocation2);

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    EXPECT_EQ(0u, commandQueue->csr->getResidencyAllocations().size());
    EXPECT_EQ(0u, commandQueue->heapContainer.size());

    commandQueue->destroy();
    commandList->destroy();
    alignedFree(alloc);
}

HWTEST2_F(ExecuteCommandListTests, givenCommandQueueHavingTwoB2BCommandListsThenMVSDirtyFlagAndGSBADirtyFlagAreSetOnlyOnce, CommandQueueExecuteTestSupport) {
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    ze_result_t returnValue;
    auto commandQueue = CommandQueue::create(productFamily,
                                             device,
                                             csr,
                                             &desc,
                                             false,
                                             false,
                                             returnValue);
    auto commandList0 = new CommandListCoreFamily<gfxCoreFamily>();
    commandList0->initialize(device, NEO::EngineGroupType::Compute, 0u);
    commandList0->setCommandListPerThreadScratchSize(0u);
    auto commandList1 = new CommandListCoreFamily<gfxCoreFamily>();
    commandList1->initialize(device, NEO::EngineGroupType::Compute, 0u);
    commandList1->setCommandListPerThreadScratchSize(0u);
    auto commandListHandle0 = commandList0->toHandle();
    auto commandListHandle1 = commandList1->toHandle();

    EXPECT_EQ(true, csr->getMediaVFEStateDirty());
    EXPECT_EQ(true, csr->getGSBAStateDirty());
    commandQueue->executeCommandLists(1, &commandListHandle0, nullptr, false);
    EXPECT_EQ(false, csr->getMediaVFEStateDirty());
    EXPECT_EQ(false, csr->getGSBAStateDirty());
    commandQueue->executeCommandLists(1, &commandListHandle1, nullptr, false);
    EXPECT_EQ(false, csr->getMediaVFEStateDirty());
    EXPECT_EQ(false, csr->getGSBAStateDirty());

    commandQueue->destroy();
    commandList0->destroy();
    commandList1->destroy();
}

using CommandQueueExecuteSupport = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;
HWTEST2_F(ExecuteCommandListTests, givenCommandQueueHavingTwoB2BCommandListsThenMVSIsProgrammedOnlyOnce, CommandQueueExecuteSupport) {
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    ze_result_t returnValue;
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                           device,
                                                           csr,
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));
    auto commandList0 = std::unique_ptr<CommandList>(whitebox_cast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    commandList0->setCommandListPerThreadScratchSize(0u);
    auto commandList1 = std::unique_ptr<CommandList>(whitebox_cast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    commandList1->setCommandListPerThreadScratchSize(0u);
    auto commandListHandle0 = commandList0->toHandle();
    auto commandListHandle1 = commandList1->toHandle();

    ASSERT_NE(nullptr, commandQueue->commandStream);

    commandQueue->executeCommandLists(1, &commandListHandle0, nullptr, false);
    commandQueue->executeCommandLists(1, &commandListHandle1, nullptr, false);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();

    GenCmdList cmdList1;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList1, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto mediaVfeStates = findAll<MEDIA_VFE_STATE *>(cmdList1.begin(), cmdList1.end());
    auto GSBAStates = findAll<STATE_BASE_ADDRESS *>(cmdList1.begin(), cmdList1.end());
    // We should have only 1 state added
    ASSERT_EQ(1u, mediaVfeStates.size());
    ASSERT_EQ(1u, GSBAStates.size());

    commandQueue->destroy();
}

HWTEST2_F(ExecuteCommandListTests, givenTwoCommandQueuesHavingTwoB2BCommandListsWithPTSSsetForFirstCmdListThenMVSAndGSBAAreProgrammedOnlyOnce, CommandQueueExecuteSupport) {
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    ze_result_t returnValue;
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                           device,
                                                           csr,
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));
    auto commandList0 = std::unique_ptr<CommandList>(whitebox_cast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    auto commandList1 = std::unique_ptr<CommandList>(whitebox_cast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    commandList0->setCommandListPerThreadScratchSize(512u);
    commandList1->setCommandListPerThreadScratchSize(0u);
    auto commandListHandle0 = commandList0->toHandle();
    auto commandListHandle1 = commandList1->toHandle();

    commandQueue->executeCommandLists(1, &commandListHandle0, nullptr, false);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());
    commandQueue->executeCommandLists(1, &commandListHandle1, nullptr, false);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto mediaVfeStates = findAll<MEDIA_VFE_STATE *>(cmdList.begin(), cmdList.end());
    auto GSBAStates = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    // We should have only 1 state added
    ASSERT_EQ(1u, mediaVfeStates.size());
    ASSERT_EQ(1u, GSBAStates.size());

    commandList0->reset();
    commandList0->setCommandListPerThreadScratchSize(0u);
    commandList1->reset();
    commandList1->setCommandListPerThreadScratchSize(0u);

    auto commandQueue1 = whitebox_cast(CommandQueue::create(productFamily,
                                                            device,
                                                            csr,
                                                            &desc,
                                                            false,
                                                            false,
                                                            returnValue));

    commandQueue1->executeCommandLists(1, &commandListHandle0, nullptr, false);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());
    commandQueue1->executeCommandLists(1, &commandListHandle1, nullptr, false);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());

    usedSpaceAfter = commandQueue1->commandStream->getUsed();

    GenCmdList cmdList1;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList1, ptrOffset(commandQueue1->commandStream->getCpuBase(), 0), usedSpaceAfter));

    mediaVfeStates = findAll<MEDIA_VFE_STATE *>(cmdList1.begin(), cmdList1.end());
    GSBAStates = findAll<STATE_BASE_ADDRESS *>(cmdList1.begin(), cmdList1.end());
    // We should have no state added
    ASSERT_EQ(0u, mediaVfeStates.size());
    ASSERT_EQ(0u, GSBAStates.size());

    commandQueue->destroy();
    commandQueue1->destroy();
}

HWTEST2_F(ExecuteCommandListTests, givenTwoCommandQueuesHavingTwoB2BCommandListsAndWithPTSSsetForSecondCmdListThenMVSandGSBAAreProgrammedTwice, CommandQueueExecuteSupport) {
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    ze_result_t returnValue;
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                           device,
                                                           csr,
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));
    auto commandList0 = std::unique_ptr<CommandList>(whitebox_cast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    auto commandList1 = std::unique_ptr<CommandList>(whitebox_cast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    commandList0->setCommandListPerThreadScratchSize(0u);
    commandList1->setCommandListPerThreadScratchSize(512u);
    auto commandListHandle0 = commandList0->toHandle();
    auto commandListHandle1 = commandList1->toHandle();

    commandQueue->executeCommandLists(1, &commandListHandle0, nullptr, false);
    EXPECT_EQ(0u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());
    commandQueue->executeCommandLists(1, &commandListHandle1, nullptr, false);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto mediaVfeStates = findAll<MEDIA_VFE_STATE *>(cmdList.begin(), cmdList.end());
    auto GSBAStates = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    // We should have 2 states added
    ASSERT_EQ(2u, mediaVfeStates.size());
    ASSERT_EQ(2u, GSBAStates.size());

    commandList0->reset();
    commandList0->setCommandListPerThreadScratchSize(512u);
    commandList1->reset();
    commandList1->setCommandListPerThreadScratchSize(0u);

    auto commandQueue1 = whitebox_cast(CommandQueue::create(productFamily,
                                                            device,
                                                            csr,
                                                            &desc,
                                                            false,
                                                            false,
                                                            returnValue));

    commandQueue1->executeCommandLists(1, &commandListHandle0, nullptr, false);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());
    commandQueue1->executeCommandLists(1, &commandListHandle1, nullptr, false);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());

    usedSpaceAfter = commandQueue1->commandStream->getUsed();

    GenCmdList cmdList1;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList1, ptrOffset(commandQueue1->commandStream->getCpuBase(), 0), usedSpaceAfter));

    mediaVfeStates = findAll<MEDIA_VFE_STATE *>(cmdList1.begin(), cmdList1.end());
    GSBAStates = findAll<STATE_BASE_ADDRESS *>(cmdList1.begin(), cmdList1.end());
    // We should have no state added
    ASSERT_EQ(0u, mediaVfeStates.size());
    ASSERT_EQ(0u, GSBAStates.size());

    commandQueue->destroy();
    commandQueue1->destroy();
}

HWTEST2_F(ExecuteCommandListTests, givenTwoCommandQueuesHavingTwoB2BCommandListsAndWithPTSSGrowingThenMVSAndGSBAAreProgrammedTwice, CommandQueueExecuteSupport) {
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    ze_result_t returnValue;
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                           device,
                                                           csr,
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));
    auto commandList0 = std::unique_ptr<CommandList>(whitebox_cast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    auto commandList1 = std::unique_ptr<CommandList>(whitebox_cast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    commandList0->setCommandListPerThreadScratchSize(512u);
    commandList1->setCommandListPerThreadScratchSize(512u);
    auto commandListHandle0 = commandList0->toHandle();
    auto commandListHandle1 = commandList1->toHandle();

    commandQueue->executeCommandLists(1, &commandListHandle0, nullptr, false);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());
    commandQueue->executeCommandLists(1, &commandListHandle1, nullptr, false);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto mediaVfeStates = findAll<MEDIA_VFE_STATE *>(cmdList.begin(), cmdList.end());
    auto GSBAStates = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    // We should have only 1 state added
    ASSERT_EQ(1u, mediaVfeStates.size());
    ASSERT_EQ(1u, GSBAStates.size());

    commandList0->reset();
    commandList0->setCommandListPerThreadScratchSize(1024u);
    commandList1->reset();
    commandList1->setCommandListPerThreadScratchSize(1024u);

    auto commandQueue1 = whitebox_cast(CommandQueue::create(productFamily,
                                                            device,
                                                            csr,
                                                            &desc,
                                                            false,
                                                            false,
                                                            returnValue));

    commandQueue1->executeCommandLists(1, &commandListHandle0, nullptr, false);
    EXPECT_EQ(1024u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());
    commandQueue1->executeCommandLists(1, &commandListHandle1, nullptr, false);
    EXPECT_EQ(1024u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());

    usedSpaceAfter = commandQueue1->commandStream->getUsed();

    GenCmdList cmdList1;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList1, ptrOffset(commandQueue1->commandStream->getCpuBase(), 0), usedSpaceAfter));

    mediaVfeStates = findAll<MEDIA_VFE_STATE *>(cmdList1.begin(), cmdList1.end());
    GSBAStates = findAll<STATE_BASE_ADDRESS *>(cmdList1.begin(), cmdList1.end());
    // We should have only 1 state added
    ASSERT_EQ(1u, mediaVfeStates.size());
    ASSERT_EQ(1u, GSBAStates.size());

    commandQueue->destroy();
    commandQueue1->destroy();
}

HWTEST2_F(ExecuteCommandListTests, givenTwoCommandQueuesHavingTwoB2BCommandListsAndWithPTSSUniquePerCmdListThenMVSAndGSBAAreProgrammedOncePerSubmission, CommandQueueExecuteSupport) {
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    ze_result_t returnValue;
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                           device,
                                                           csr,
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));
    auto commandList0 = std::unique_ptr<CommandList>(whitebox_cast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    auto commandList1 = std::unique_ptr<CommandList>(whitebox_cast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    commandList0->setCommandListPerThreadScratchSize(0u);
    commandList1->setCommandListPerThreadScratchSize(512u);
    auto commandListHandle0 = commandList0->toHandle();
    auto commandListHandle1 = commandList1->toHandle();

    commandQueue->executeCommandLists(1, &commandListHandle0, nullptr, false);
    EXPECT_EQ(0u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());
    commandQueue->executeCommandLists(1, &commandListHandle1, nullptr, false);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto mediaVfeStates = findAll<MEDIA_VFE_STATE *>(cmdList.begin(), cmdList.end());
    auto GSBAStates = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    // We should have 2 states added
    ASSERT_EQ(2u, mediaVfeStates.size());
    ASSERT_EQ(2u, GSBAStates.size());

    commandList0->reset();
    commandList0->setCommandListPerThreadScratchSize(1024u);
    commandList1->reset();
    commandList1->setCommandListPerThreadScratchSize(2048u);

    auto commandQueue1 = whitebox_cast(CommandQueue::create(productFamily,
                                                            device,
                                                            csr,
                                                            &desc,
                                                            false,
                                                            false,
                                                            returnValue));
    commandQueue1->executeCommandLists(1, &commandListHandle0, nullptr, false);
    EXPECT_EQ(1024u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());
    commandQueue1->executeCommandLists(1, &commandListHandle1, nullptr, false);
    EXPECT_EQ(2048u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());

    usedSpaceAfter = commandQueue1->commandStream->getUsed();

    GenCmdList cmdList1;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList1, ptrOffset(commandQueue1->commandStream->getCpuBase(), 0), usedSpaceAfter));

    mediaVfeStates = findAll<MEDIA_VFE_STATE *>(cmdList1.begin(), cmdList1.end());
    GSBAStates = findAll<STATE_BASE_ADDRESS *>(cmdList1.begin(), cmdList1.end());
    // We should have 2 states added
    ASSERT_EQ(2u, mediaVfeStates.size());
    ASSERT_EQ(2u, GSBAStates.size());

    commandQueue->destroy();
    commandQueue1->destroy();
}

using CommandQueueSynchronizeTest = Test<ContextFixture>;

template <typename GfxFamily>
struct SynchronizeCsr : public NEO::UltCommandStreamReceiver<GfxFamily> {
    ~SynchronizeCsr() override {
        delete tagAddress;
    }

    SynchronizeCsr(const NEO::ExecutionEnvironment &executionEnvironment, const DeviceBitfield deviceBitfield)
        : NEO::UltCommandStreamReceiver<GfxFamily>(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment), 0, deviceBitfield) {
        tagAddress = new uint32_t;
    }

    bool waitForCompletionWithTimeout(bool enableTimeout, int64_t timeoutMs, uint32_t taskCountToWait) override {
        enableTimeoutSet = enableTimeout;
        waitForComplitionCalledTimes++;
        return true;
    }

    void waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool quickKmdSleep, bool forcePowerSavingMode) override {
        waitForTaskCountWithKmdNotifyFallbackCalled++;
        NEO::UltCommandStreamReceiver<GfxFamily>::waitForTaskCountWithKmdNotifyFallback(taskCountToWait, flushStampToWait, quickKmdSleep, forcePowerSavingMode);
    }

    volatile uint32_t *getTagAddress() const override {
        return tagAddress;
    }

    uint32_t *tagAddress;
    uint32_t waitForComplitionCalledTimes = 0;
    uint32_t waitForTaskCountWithKmdNotifyFallbackCalled = 0;
    bool enableTimeoutSet = false;
};

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
        }
    }

    EXPECT_EQ(static_cast<uint32_t>(CommandQueueImp::CommandBufferManager::BUFFER_ALLOCATION::COUNT), cmdBufferAllocationsFound);

    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, givenOverrideCmdQueueSyncModeToDefaultWhenCommandQueueIsCreatedWithSynchronousModeThenDefaultModeIsSelected) {
    DebugManagerStateRestore restore;
    NEO::DebugManager.flags.OverrideCmdQueueSynchronousMode.set(0);

    ze_command_queue_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_ERROR_DEVICE_LOST;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue);
    ASSERT_NE(nullptr, commandQueue);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
    auto cmdQueueSynchronousMode = reinterpret_cast<L0::CommandQueueImp *>(commandQueue)->getSynchronousMode();
    EXPECT_EQ(ZE_COMMAND_QUEUE_MODE_DEFAULT, cmdQueueSynchronousMode);

    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, givenOverrideCmdQueueSyncModeToAsynchronousWhenCommandQueueIsCreatedWithSynchronousModeThenAsynchronousModeIsSelected) {
    DebugManagerStateRestore restore;
    NEO::DebugManager.flags.OverrideCmdQueueSynchronousMode.set(2);

    ze_command_queue_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_ERROR_DEVICE_LOST;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue);
    ASSERT_NE(nullptr, commandQueue);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
    auto cmdQueueSynchronousMode = reinterpret_cast<L0::CommandQueueImp *>(commandQueue)->getSynchronousMode();
    EXPECT_EQ(ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS, cmdQueueSynchronousMode);

    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, givenOverrideCmdQueueSyncModeToSynchronousWhenCommandQueueIsCreatedWithAsynchronousModeThenSynchronousModeIsSelected) {
    DebugManagerStateRestore restore;
    NEO::DebugManager.flags.OverrideCmdQueueSynchronousMode.set(1);

    ze_command_queue_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_ERROR_DEVICE_LOST;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue);
    ASSERT_NE(nullptr, commandQueue);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
    auto cmdQueueSynchronousMode = reinterpret_cast<L0::CommandQueueImp *>(commandQueue)->getSynchronousMode();
    EXPECT_EQ(ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS, cmdQueueSynchronousMode);

    commandQueue->destroy();
}
} // namespace ult
} // namespace L0
