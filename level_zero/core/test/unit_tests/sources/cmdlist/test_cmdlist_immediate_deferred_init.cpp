/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdlist/cmdlist_host_function_parameters.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"
#include "level_zero/core/test/unit_tests/mocks/mock_graph.h"

namespace L0 {
namespace ult {

struct ImmediateCmdListDeferredInitializationFixture : public DeviceFixture {
    void setUp() {
        NEO::debugManager.flags.DeferCmdQGpgpuInitialization.set(1);
        NEO::debugManager.flags.DeferCmdQBcsInitialization.set(1);
        DeviceFixture::setUp();
    }

    void tearDown() {
        DeviceFixture::tearDown();
    }

    std::unique_ptr<L0::CommandList> createImmediateCmdList(const ze_command_queue_desc_t &desc, bool internalUsage, NEO::EngineGroupType engineGroupType) {
        ze_result_t returnValue = ZE_RESULT_ERROR_UNINITIALIZED;
        std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, internalUsage, engineGroupType, returnValue));
        EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
        return commandList;
    }

    static ze_result_t appendBarrier(L0::CommandList *commandList) {
        CmdListWaitEventParameters waitEventParameters = {};
        CmdListSignalEventParameters signalEventParameters = {};
        return commandList->appendBarrier(nullptr, 0, nullptr, waitEventParameters, signalEventParameters);
    }

    DebugManagerStateRestore restorer;
};

using ImmediateCmdListDeferredInitializationTest = Test<ImmediateCmdListDeferredInitializationFixture>;

constexpr GraphCommandId graphSegmentStart = 0;

TEST_F(ImmediateCmdListDeferredInitializationTest, givenImmediateCmdListWhenCreatedThenQueueCsrAndCommandContainerAreNotCreated) {
    ze_command_queue_desc_t desc = {};
    auto commandList = createImmediateCmdList(desc, false, NEO::EngineGroupType::compute);
    auto whiteBoxCmdList = CommandList::whiteboxCast(commandList.get());

    EXPECT_EQ(nullptr, whiteBoxCmdList->cmdQImmediate);
    EXPECT_EQ(nullptr, whiteBoxCmdList->cmdQImmediateCopyOffload);
    EXPECT_EQ(nullptr, whiteBoxCmdList->commandContainer.getCommandStream());
    EXPECT_FALSE(whiteBoxCmdList->isInOrderExecutionEnabled());
    EXPECT_EQ(device, whiteBoxCmdList->getDevice());
    EXPECT_TRUE(whiteBoxCmdList->isImmediateType());
}

TEST_F(ImmediateCmdListDeferredInitializationTest, givenImmediateCmdListWhenCreatedThenSelectedEngineIsNotInitialized) {
    NEO::CommandStreamReceiver *firstCsr = nullptr;
    NEO::CommandStreamReceiver *expectedCsr = neoDevice->getDefaultEngine().commandStreamReceiver;

    auto &secondaryEngines = neoDevice->secondaryEngines[aub_stream::EngineType::ENGINE_CCS];
    const bool areSecondaryEnginesAvailable = secondaryEngines.engines.size() > 0;

    if (areSecondaryEnginesAvailable) {
        ASSERT_EQ(ZE_RESULT_SUCCESS, device->getCsrForOrdinalAndIndex(&firstCsr, 0, 0, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, std::nullopt));
        ASSERT_LE(2u, secondaryEngines.engines.size());
        expectedCsr = secondaryEngines.engines[1].commandStreamReceiver;
    }
    if (areSecondaryEnginesAvailable) {
        ASSERT_FALSE(expectedCsr->isInitialized());
    }

    ze_command_queue_desc_t desc = {};
    auto commandList = createImmediateCmdList(desc, false, NEO::EngineGroupType::compute);

    if (areSecondaryEnginesAvailable) {
        EXPECT_FALSE(expectedCsr->isInitialized());
        EXPECT_FALSE(expectedCsr->getOsContext().isInitialized());
    }
    auto whiteBoxCmdList = CommandList::whiteboxCast(commandList.get());
    EXPECT_EQ(nullptr, whiteBoxCmdList->cmdQImmediate);

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList.get()));

    EXPECT_EQ(expectedCsr, commandList->getCsr(false));
    EXPECT_TRUE(expectedCsr->isInitialized());
    EXPECT_TRUE(expectedCsr->getOsContext().isInitialized());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);
}

TEST_F(ImmediateCmdListDeferredInitializationTest, givenImmediateCmdListWhenFirstAppendCalledThenQueueAndCommandContainerAreCreated) {
    ze_command_queue_desc_t desc = {};
    auto commandList = createImmediateCmdList(desc, false, NEO::EngineGroupType::compute);
    auto whiteBoxCmdList = CommandList::whiteboxCast(commandList.get());

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList.get()));

    ASSERT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);
    EXPECT_NE(nullptr, whiteBoxCmdList->getCsr(false));
    EXPECT_NE(nullptr, whiteBoxCmdList->getCmdContainer().getCommandStream());
}

TEST_F(ImmediateCmdListDeferredInitializationTest, givenInitializedImmediateCmdListWhenNextAppendCalledThenResourcesAreNotCreatedAgain) {
    ze_command_queue_desc_t desc = {};
    auto commandList = createImmediateCmdList(desc, false, NEO::EngineGroupType::compute);
    auto whiteBoxCmdList = CommandList::whiteboxCast(commandList.get());

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList.get()));

    auto queueAfterFirstAppend = whiteBoxCmdList->cmdQImmediate;
    auto csrAfterFirstAppend = whiteBoxCmdList->getCsr(false);
    auto commandStreamAfterFirstAppend = whiteBoxCmdList->getCmdContainer().getCommandStream();

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList.get()));

    EXPECT_EQ(queueAfterFirstAppend, whiteBoxCmdList->cmdQImmediate);
    EXPECT_EQ(csrAfterFirstAppend, whiteBoxCmdList->getCsr(false));
    EXPECT_EQ(commandStreamAfterFirstAppend, whiteBoxCmdList->getCmdContainer().getCommandStream());
}

TEST_F(ImmediateCmdListDeferredInitializationTest, givenInOrderImmediateCmdListWhenCreatedThenFlagIsReportedAndCounterIsAllocatedOnFirstAppend) {
    ze_command_queue_desc_t desc = {};
    desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    auto commandList = createImmediateCmdList(desc, false, NEO::EngineGroupType::compute);
    auto whiteBoxCmdList = CommandList::whiteboxCast(commandList.get());

    ze_command_list_flags_t reportedFlags = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->getFlags(&reportedFlags));
    EXPECT_TRUE(NEO::isValueSet(reportedFlags, ZE_COMMAND_LIST_FLAG_IN_ORDER));
    EXPECT_TRUE(whiteBoxCmdList->isInOrderExecutionRequested());
    EXPECT_FALSE(whiteBoxCmdList->isInOrderExecutionEnabled());

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList.get()));

    EXPECT_TRUE(whiteBoxCmdList->isInOrderExecutionEnabled());
}

TEST_F(ImmediateCmdListDeferredInitializationTest, givenInternalImmediateCmdListWhenCreatedThenInternalEngineIsSelectedOnFirstAppend) {
    ze_command_queue_desc_t desc = {};
    auto commandList = createImmediateCmdList(desc, true, NEO::EngineGroupType::renderCompute);
    auto whiteBoxCmdList = CommandList::whiteboxCast(commandList.get());

    EXPECT_EQ(nullptr, whiteBoxCmdList->cmdQImmediate);

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList.get()));

    EXPECT_EQ(neoDevice->getInternalEngine().commandStreamReceiver, whiteBoxCmdList->getCsr(false));
}

TEST_F(ImmediateCmdListDeferredInitializationTest, givenInternalCopyImmediateCmdListWhenCreatedThenInternalCopyEngineIsSelectedOnFirstAppend) {
    ze_command_queue_desc_t desc = {};
    auto commandList = createImmediateCmdList(desc, true, NEO::EngineGroupType::copy);
    auto whiteBoxCmdList = CommandList::whiteboxCast(commandList.get());

    EXPECT_EQ(nullptr, whiteBoxCmdList->cmdQImmediate);

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList.get()));

    auto internalCopyEngine = neoDevice->getInternalCopyEngine();
    auto expectedCsr = internalCopyEngine ? internalCopyEngine->commandStreamReceiver : neoDevice->getInternalEngine().commandStreamReceiver;
    EXPECT_EQ(expectedCsr, whiteBoxCmdList->getCsr(false));
}

TEST_F(ImmediateCmdListDeferredInitializationTest, givenCsrProvidedWhenCreatingImmediateCmdListThenResourcesAreCreatedImmediately) {
    auto providedCsr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    providedCsr->setupContext(*neoDevice->getDefaultEngine().osContext);
    ASSERT_FALSE(providedCsr->isInitialized());

    ze_command_queue_desc_t desc = {};
    ze_result_t returnValue = ZE_RESULT_ERROR_UNINITIALIZED;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, true, NEO::EngineGroupType::compute, providedCsr.get(), returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto whiteBoxCmdList = CommandList::whiteboxCast(commandList.get());

    ASSERT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);
    EXPECT_EQ(providedCsr.get(), whiteBoxCmdList->getCsr(false));
    EXPECT_NE(nullptr, whiteBoxCmdList->getCmdContainer().getCommandStream());
    EXPECT_TRUE(providedCsr->isInitialized());
}

TEST_F(ImmediateCmdListDeferredInitializationTest, givenNotUsedImmediateCmdListWhenHostSynchronizeCalledThenSuccessIsReturnedAndResourcesAreNotCreated) {
    NEO::CommandStreamReceiver *firstCsr = nullptr;
    NEO::CommandStreamReceiver *expectedCsr = neoDevice->getDefaultEngine().commandStreamReceiver;

    auto &secondaryEngines = neoDevice->secondaryEngines[aub_stream::EngineType::ENGINE_CCS];
    const bool areSecondaryEnginesAvailable = secondaryEngines.engines.size() > 0;

    if (areSecondaryEnginesAvailable) {
        ASSERT_EQ(ZE_RESULT_SUCCESS, device->getCsrForOrdinalAndIndex(&firstCsr, 0, 0, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, std::nullopt));
        ASSERT_LE(2u, secondaryEngines.engines.size());
        expectedCsr = secondaryEngines.engines[1].commandStreamReceiver;
    }

    ze_command_queue_desc_t desc = {};
    auto commandList = createImmediateCmdList(desc, false, NEO::EngineGroupType::compute);
    auto whiteBoxCmdList = CommandList::whiteboxCast(commandList.get());

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(0));
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(std::numeric_limits<uint64_t>::max()));

    EXPECT_EQ(nullptr, whiteBoxCmdList->cmdQImmediate);
    EXPECT_EQ(nullptr, whiteBoxCmdList->commandContainer.getCommandStream());
    if (areSecondaryEnginesAvailable) {
        EXPECT_FALSE(expectedCsr->isInitialized());
    }
}

TEST_F(ImmediateCmdListDeferredInitializationTest, givenUsedImmediateCmdListWhenHostSynchronizeCalledThenWaitIsPerformed) {
    ze_command_queue_desc_t desc = {};
    auto commandList = createImmediateCmdList(desc, false, NEO::EngineGroupType::compute);
    auto whiteBoxCmdList = CommandList::whiteboxCast(commandList.get());

    ASSERT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList.get()));
    ASSERT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(std::numeric_limits<uint64_t>::max()));
}

TEST_F(ImmediateCmdListDeferredInitializationTest, givenImmediateCmdListWhenResetCalledThenResourcesAreCreated) {
    ze_command_queue_desc_t desc = {};
    auto commandList = createImmediateCmdList(desc, false, NEO::EngineGroupType::compute);
    auto whiteBoxCmdList = CommandList::whiteboxCast(commandList.get());

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->reset());

    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);
    EXPECT_NE(nullptr, whiteBoxCmdList->getCmdContainer().getCommandStream());
}

TEST_F(ImmediateCmdListDeferredInitializationTest, givenImmediateCmdListWhenQueryingDescriptorPropertiesThenResourcesAreNotCreated) {
    ze_command_queue_desc_t desc = {};
    desc.index = 0;
    desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    desc.priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_HIGH;
    auto commandList = createImmediateCmdList(desc, false, NEO::EngineGroupType::compute);
    auto whiteBoxCmdList = CommandList::whiteboxCast(commandList.get());

    uint32_t index = std::numeric_limits<uint32_t>::max();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->getImmediateIndex(&index));
    EXPECT_EQ(desc.index, index);

    ze_command_queue_flags_t queueFlags = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->getImmediateFlags(&queueFlags));
    EXPECT_EQ(desc.flags, queueFlags);

    ze_command_queue_priority_t priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->getImmediatePriority(&priority));
    EXPECT_EQ(desc.priority, priority);

    EXPECT_EQ(nullptr, whiteBoxCmdList->cmdQImmediate);
}

TEST_F(ImmediateCmdListDeferredInitializationTest, givenImmediateCmdListWhenQueryingModeThenModeIsReturned) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    auto commandList = createImmediateCmdList(desc, false, NEO::EngineGroupType::compute);

    ze_command_queue_mode_t mode = ZE_COMMAND_QUEUE_MODE_DEFAULT;
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->getImmediateMode(&mode));
    EXPECT_EQ(ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS, mode);
}

TEST_F(ImmediateCmdListDeferredInitializationTest, givenNotUsedImmediateCmdListWhenDestroyedThenEngineStaysUninitialized) {
    NEO::CommandStreamReceiver *firstCsr = nullptr;
    NEO::CommandStreamReceiver *expectedCsr = neoDevice->getDefaultEngine().commandStreamReceiver;
    auto &secondaryEngines = neoDevice->secondaryEngines[aub_stream::EngineType::ENGINE_CCS];
    const bool areSecondaryEnginesAvailable = secondaryEngines.engines.size() > 0;

    if (areSecondaryEnginesAvailable) {
        ASSERT_EQ(ZE_RESULT_SUCCESS, device->getCsrForOrdinalAndIndex(&firstCsr, 0, 0, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, std::nullopt));
        ASSERT_LE(2u, secondaryEngines.engines.size());
        expectedCsr = secondaryEngines.engines[1].commandStreamReceiver;
    }

    ze_command_queue_desc_t desc = {};
    ze_result_t returnValue = ZE_RESULT_ERROR_UNINITIALIZED;
    auto commandList = CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::compute, returnValue);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto whiteBoxCmdList = CommandList::whiteboxCast(commandList);
    EXPECT_EQ(nullptr, whiteBoxCmdList->cmdQImmediate);
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->destroy());

    if (areSecondaryEnginesAvailable) {
        EXPECT_FALSE(expectedCsr->isInitialized());
    }
}

TEST_F(ImmediateCmdListDeferredInitializationTest, givenInvalidIndexWhenCreatingImmediateCmdListThenInvalidArgumentIsReturnedAtCreation) {
    ze_command_queue_desc_t desc = {};
    desc.ordinal = 0;
    desc.index = std::numeric_limits<uint32_t>::max();

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::compute, returnValue);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, returnValue);
    EXPECT_EQ(nullptr, commandList);
}

TEST_F(ImmediateCmdListDeferredInitializationTest, givenAllocationFailureWhenFirstAppendCalledThenOutOfDeviceMemoryIsReturned) {
    auto memoryManager = static_cast<MockMemoryManager *>(neoDevice->getExecutionEnvironment()->memoryManager.get());
    ze_command_queue_desc_t desc = {};
    auto commandListFirst = createImmediateCmdList(desc, false, NEO::EngineGroupType::compute);
    commandListFirst->ensureImmediateResourcesInitialized();

    auto commandList = createImmediateCmdList(desc, false, NEO::EngineGroupType::compute);

    memoryManager->failInDevicePool = true;
    memoryManager->failInAllocateWithSizeAndAlignment = true;
    memoryManager->failInDevicePoolWithError = true;
    memoryManager->failAllocateSystemMemory = true;
    memoryManager->failInDevicePool = true;
    memoryManager->failInDevicePool = true;

    auto result = appendBarrier(commandList.get());

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, result);
}

TEST_F(ImmediateCmdListDeferredInitializationTest, givenAllocationFailureWhenAppendCalledTwiceThenSameErrorIsReturnedAndInitializationIsNotRepeated) {
    auto memoryManager = static_cast<MockMemoryManager *>(neoDevice->getExecutionEnvironment()->memoryManager.get());
    ze_command_queue_desc_t desc = {};
    auto commandListFirst = createImmediateCmdList(desc, false, NEO::EngineGroupType::compute);
    commandListFirst->ensureImmediateResourcesInitialized();

    auto commandList = createImmediateCmdList(desc, false, NEO::EngineGroupType::compute);
    auto whiteBoxCmdList = CommandList::whiteboxCast(commandList.get());

    memoryManager->failInDevicePool = true;
    memoryManager->failInAllocateWithSizeAndAlignment = true;
    memoryManager->failInDevicePoolWithError = true;
    memoryManager->failAllocateSystemMemory = true;

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, appendBarrier(commandList.get()));
    EXPECT_EQ(nullptr, whiteBoxCmdList->cmdQImmediate);

    auto allocationCountAfterFirstAppend = memoryManager->allocateGraphicsMemoryWithPropertiesCount.load();

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, appendBarrier(commandList.get()));

    EXPECT_EQ(allocationCountAfterFirstAppend, memoryManager->allocateGraphicsMemoryWithPropertiesCount.load());
    EXPECT_EQ(nullptr, whiteBoxCmdList->cmdQImmediate);
}

TEST_F(ImmediateCmdListDeferredInitializationTest, givenFailedInitializationWhenAllocationsSucceedAgainThenCmdListStaysInFailedState) {
    auto memoryManager = static_cast<MockMemoryManager *>(neoDevice->getExecutionEnvironment()->memoryManager.get());

    NEO::CommandStreamReceiver *firstCsr = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, device->getCsrForOrdinalAndIndex(&firstCsr, 0, 0, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, std::nullopt));
    if (firstCsr->getOsContext().getPrimaryContext() == nullptr && firstCsr->getOsContext().isPartOfContextGroup()) {
        auto &secondaryEngines = neoDevice->secondaryEngines[aub_stream::EngineType::ENGINE_CCS];
        ASSERT_LE(2u, secondaryEngines.engines.size());
    }

    ze_command_queue_desc_t desc = {};
    auto commandList = createImmediateCmdList(desc, false, NEO::EngineGroupType::compute);

    memoryManager->failInDevicePool = true;
    memoryManager->failInAllocateWithSizeAndAlignment = true;
    memoryManager->failInDevicePoolWithError = true;
    memoryManager->failAllocateSystemMemory = true;

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, appendBarrier(commandList.get()));

    memoryManager->failInDevicePool = false;
    memoryManager->failInAllocateWithSizeAndAlignment = false;
    memoryManager->failInDevicePoolWithError = false;
    memoryManager->failAllocateSystemMemory = false;

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, commandList->ensureImmediateResourcesInitialized());
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, appendBarrier(commandList.get()));
}

struct FailedImmediateCmdListFixture : public ImmediateCmdListDeferredInitializationFixture {
    void setUp() {
        ImmediateCmdListDeferredInitializationFixture::setUp();

        ze_command_queue_desc_t desc = {};
        auto initializedCmdList = createImmediateCmdList(desc, false, NEO::EngineGroupType::compute);
        EXPECT_EQ(ZE_RESULT_SUCCESS, initializedCmdList->ensureImmediateResourcesInitialized());

        failedCmdList = createImmediateCmdList(desc, false, NEO::EngineGroupType::compute);

        auto memoryManager = static_cast<MockMemoryManager *>(neoDevice->getExecutionEnvironment()->memoryManager.get());
        memoryManager->failInDevicePool = true;
        memoryManager->failInAllocateWithSizeAndAlignment = true;
        memoryManager->failInDevicePoolWithError = true;
        memoryManager->failAllocateSystemMemory = true;

        EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, appendBarrier(failedCmdList.get()));
    }

    void tearDown() {
        failedCmdList.reset();
        ImmediateCmdListDeferredInitializationFixture::tearDown();
    }

    std::unique_ptr<L0::CommandList> failedCmdList;
};

using FailedImmediateCmdListTest = Test<FailedImmediateCmdListFixture>;

TEST_F(FailedImmediateCmdListTest, givenFailedInitializationWhenCallingAppendEntryPointsThenCachedErrorIsReturned) {
    auto cmdList = failedCmdList.get();
    constexpr auto expectedError = ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;

    CmdListWaitEventParameters waitEventParameters = {};
    CmdListMemoryCopyParams memoryCopyParams = {};
    CmdListHostFunctionParameters hostFunctionParameters = {};
    const ze_group_count_t groupCount = {1, 1, 1};
    uint64_t timestampDestination = 0;
    uint64_t memoryDestination = 0;

    EXPECT_EQ(expectedError, cmdList->appendEventReset(nullptr));
    EXPECT_EQ(expectedError, cmdList->appendLaunchKernelIndirect(nullptr, groupCount, nullptr, 0, nullptr, false));
    EXPECT_EQ(expectedError, cmdList->appendMemoryCopyRegion(nullptr, nullptr, 0, 0, nullptr, nullptr, 0, 0, nullptr, 0, nullptr, memoryCopyParams));
    EXPECT_EQ(expectedError, cmdList->appendMemoryFill(nullptr, nullptr, 0, 0, nullptr, 0, nullptr, memoryCopyParams));
    EXPECT_EQ(expectedError, cmdList->appendMemoryRangesBarrier(0, nullptr, nullptr, nullptr, 0, nullptr, waitEventParameters));
    EXPECT_EQ(expectedError, cmdList->appendPageFaultCopy(nullptr, nullptr, 0, false, 0));
    EXPECT_EQ(expectedError, cmdList->appendQueryKernelTimestamps(0, nullptr, nullptr, nullptr, nullptr, 0, nullptr, waitEventParameters));
    EXPECT_EQ(expectedError, cmdList->appendWriteGlobalTimestamp(&timestampDestination, nullptr, 0, nullptr, waitEventParameters));
    EXPECT_EQ(expectedError, cmdList->appendWaitOnMemory(nullptr, &memoryDestination, 0, nullptr, false));
    EXPECT_EQ(expectedError, cmdList->appendWriteToMemory(nullptr, &memoryDestination, 0));
    EXPECT_EQ(expectedError, cmdList->appendHostFunction(nullptr, nullptr, nullptr, nullptr, 0, nullptr, hostFunctionParameters));
    EXPECT_EQ(expectedError, cmdList->appendImageCopyRegion(nullptr, nullptr, nullptr, nullptr, nullptr, 0, nullptr, memoryCopyParams));
    EXPECT_EQ(expectedError, cmdList->appendImageCopyFromMemory(nullptr, nullptr, nullptr, nullptr, 0, nullptr, memoryCopyParams));
    EXPECT_EQ(expectedError, cmdList->appendImageCopyToMemory(nullptr, nullptr, nullptr, nullptr, 0, nullptr, memoryCopyParams));
    EXPECT_EQ(expectedError, cmdList->appendImageCopyFromMemoryExt(nullptr, nullptr, nullptr, 0, 0, nullptr, 0, nullptr, memoryCopyParams));
    EXPECT_EQ(expectedError, cmdList->appendImageCopyToMemoryExt(nullptr, nullptr, nullptr, 0, 0, nullptr, 0, nullptr, memoryCopyParams));
}

TEST_F(FailedImmediateCmdListTest, givenFailedInitializationWhenCallingMetricEntryPointsThenCachedErrorIsReturned) {
    auto cmdList = failedCmdList.get();
    constexpr auto expectedError = ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;

    EXPECT_EQ(expectedError, cmdList->appendMetricMemoryBarrier());
    EXPECT_EQ(expectedError, cmdList->appendMetricStreamerMarker(nullptr, 0));
    EXPECT_EQ(expectedError, cmdList->appendMetricQueryBegin(nullptr));
    EXPECT_EQ(expectedError, cmdList->appendMetricQueryEnd(nullptr, nullptr, 0, nullptr));
}

TEST_F(FailedImmediateCmdListTest, givenFailedInitializationOfGraphOwnExecutionTargetWhenExecutingGraphSegmentThenCachedErrorIsReturned) {
    ze_command_queue_desc_t desc = {};
    auto passedExecutionTarget = createImmediateCmdList(desc, false, NEO::EngineGroupType::compute);

    MockExecutableGraph executableGraph;
    executableGraph.executionTarget = failedCmdList.get();

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, executableGraph.executeSegment(passedExecutionTarget.get(), graphSegmentStart, nullptr, 0, nullptr));
    EXPECT_EQ(nullptr, CommandList::whiteboxCast(passedExecutionTarget.get())->cmdQImmediate);
}

TEST_F(FailedImmediateCmdListTest, givenFailedInitializationOfPassedExecutionTargetWhenExecutingGraphSegmentThenCachedErrorIsReturned) {
    MockExecutableGraph executableGraph;

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, executableGraph.executeSegment(failedCmdList.get(), graphSegmentStart, nullptr, 0, nullptr));
}

TEST_F(FailedImmediateCmdListTest, givenFailedInitializationWhenVerifyingMemoryThenFalseIsReturned) {
    uint64_t allocation = 0;
    uint64_t expectedData = 0;
    EXPECT_FALSE(failedCmdList->verifyMemory(&allocation, &expectedData, sizeof(allocation), 0));
}

TEST_F(FailedImmediateCmdListTest, givenFailedInitializationWhenGettingPatchPreambleFullDataThenUnrecoverableIsCalled) {
    uint64_t counterValue = std::numeric_limits<uint64_t>::max();
    uint64_t *hostAddress = nullptr;
    uint64_t hostGpuAddress = std::numeric_limits<uint64_t>::max();
    NEO::GraphicsAllocation *hostNodeAllocation = nullptr;
    uint64_t deviceGpuAddress = std::numeric_limits<uint64_t>::max();
    NEO::GraphicsAllocation *deviceNodeAllocation = nullptr;

    EXPECT_THROW(failedCmdList->getPatchPreambleFullData(counterValue, hostAddress, hostGpuAddress, hostNodeAllocation, deviceGpuAddress, deviceNodeAllocation), std::exception);

    EXPECT_EQ(std::numeric_limits<uint64_t>::max(), counterValue);
    EXPECT_EQ(nullptr, hostAddress);
    EXPECT_EQ(nullptr, hostNodeAllocation);
    EXPECT_EQ(nullptr, deviceNodeAllocation);
}

TEST_F(FailedImmediateCmdListTest, givenFailedInitializationWhenSettingPatchingPreambleThenUnrecoverableIsCalled) {
    auto whiteBoxCmdList = CommandList::whiteboxCast(failedCmdList.get());

    if (!whiteBoxCmdList->cmdQImmediate) {
        EXPECT_THROW(failedCmdList->setPatchingPreamble(true), std::exception);
    } else {
        EXPECT_NO_THROW(failedCmdList->setPatchingPreamble(true));
    }
}

TEST_F(ImmediateCmdListDeferredInitializationTest, givenRegularCmdListWhenSettingPatchingPreambleThenResourcesAreNotInitialized) {
    ze_result_t returnValue = ZE_RESULT_ERROR_UNINITIALIZED;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto whiteBoxCmdList = CommandList::whiteboxCast(commandList.get());

    commandList->setPatchingPreamble(true);

    EXPECT_FALSE(whiteBoxCmdList->isImmediateType());
    EXPECT_EQ(nullptr, whiteBoxCmdList->cmdQImmediate);
}

TEST_F(ImmediateCmdListDeferredInitializationTest, givenInitializedImmediateCmdListWhenSettingPatchingPreambleThenQueueIsUpdated) {
    ze_command_queue_desc_t desc = {};
    auto commandList = createImmediateCmdList(desc, false, NEO::EngineGroupType::compute);
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->ensureImmediateResourcesInitialized());
    auto whiteBoxCmdList = CommandList::whiteboxCast(commandList.get());

    commandList->setPatchingPreamble(true);

    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);
}

TEST_F(ImmediateCmdListDeferredInitializationTest, givenGraphWithOwnExecutionTargetWhenExecutingSegmentThenOnlyThatTargetIsInitialized) {
    ze_command_queue_desc_t desc = {};
    auto graphExecutionTarget = createImmediateCmdList(desc, false, NEO::EngineGroupType::compute);
    auto passedExecutionTarget = createImmediateCmdList(desc, false, NEO::EngineGroupType::compute);
    auto whiteBoxGraphExecutionTarget = CommandList::whiteboxCast(graphExecutionTarget.get());
    auto whiteBoxPassedExecutionTarget = CommandList::whiteboxCast(passedExecutionTarget.get());
    ASSERT_EQ(nullptr, whiteBoxGraphExecutionTarget->cmdQImmediate);
    ASSERT_EQ(nullptr, whiteBoxPassedExecutionTarget->cmdQImmediate);

    MockExecutableGraph executableGraph;
    executableGraph.executionTarget = graphExecutionTarget.get();

    EXPECT_EQ(ZE_RESULT_SUCCESS, executableGraph.executeSegment(passedExecutionTarget.get(), graphSegmentStart, nullptr, 0, nullptr));

    EXPECT_NE(nullptr, whiteBoxGraphExecutionTarget->cmdQImmediate);
    EXPECT_NE(nullptr, whiteBoxGraphExecutionTarget->getCmdContainer().getCommandStream());
    EXPECT_EQ(nullptr, whiteBoxPassedExecutionTarget->cmdQImmediate);
}

TEST_F(ImmediateCmdListDeferredInitializationTest, givenGraphWithOwnExecutionTargetWhenExecutingSegmentThenSegmentIsSubmittedToInitializedTarget) {
    ze_result_t returnValue = ZE_RESULT_ERROR_UNINITIALIZED;
    std::unique_ptr<L0::CommandList> segmentCmdList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendBarrier(segmentCmdList.get()));
    ASSERT_EQ(ZE_RESULT_SUCCESS, segmentCmdList->close());

    ze_command_queue_desc_t desc = {};
    auto graphExecutionTarget = createImmediateCmdList(desc, false, NEO::EngineGroupType::compute);
    auto passedExecutionTarget = createImmediateCmdList(desc, false, NEO::EngineGroupType::compute);
    auto whiteBoxGraphExecutionTarget = CommandList::whiteboxCast(graphExecutionTarget.get());
    ASSERT_EQ(nullptr, whiteBoxGraphExecutionTarget->cmdQImmediate);

    MockExecutableGraph executableGraph;
    executableGraph.executionTarget = graphExecutionTarget.get();
    executableGraph.myOrderedSegments[graphSegmentStart] = segmentCmdList.get();

    EXPECT_EQ(ZE_RESULT_SUCCESS, executableGraph.executeSegment(passedExecutionTarget.get(), graphSegmentStart, nullptr, 0, nullptr));

    EXPECT_NE(nullptr, whiteBoxGraphExecutionTarget->cmdQImmediate);
    EXPECT_EQ(nullptr, CommandList::whiteboxCast(passedExecutionTarget.get())->cmdQImmediate);
}

TEST_F(ImmediateCmdListDeferredInitializationTest, givenGraphWithoutOwnExecutionTargetWhenExecutingSegmentThenPassedTargetIsInitialized) {
    ze_command_queue_desc_t desc = {};
    auto passedExecutionTarget = createImmediateCmdList(desc, false, NEO::EngineGroupType::compute);
    auto whiteBoxPassedExecutionTarget = CommandList::whiteboxCast(passedExecutionTarget.get());
    ASSERT_EQ(nullptr, whiteBoxPassedExecutionTarget->cmdQImmediate);

    MockExecutableGraph executableGraph;
    ASSERT_EQ(nullptr, executableGraph.executionTarget);

    EXPECT_EQ(ZE_RESULT_SUCCESS, executableGraph.executeSegment(passedExecutionTarget.get(), graphSegmentStart, nullptr, 0, nullptr));

    EXPECT_NE(nullptr, whiteBoxPassedExecutionTarget->cmdQImmediate);
    EXPECT_NE(nullptr, whiteBoxPassedExecutionTarget->getCmdContainer().getCommandStream());
}

HWTEST_F(ImmediateCmdListDeferredInitializationTest, givenInitializedImmediateCmdListWhenCommandBufferAllocationFailsThenCheckAvailableSpaceReturnsOutOfDeviceMemory) {
    NEO::debugManager.flags.SetAmountOfReusableAllocations.set(0);

    ze_command_queue_desc_t desc = {};
    auto commandList = createImmediateCmdList(desc, false, NEO::EngineGroupType::compute);
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->ensureImmediateResourcesInitialized());

    ASSERT_EQ(ZE_RESULT_SUCCESS, appendBarrier(commandList.get()));

    auto immediateCmdList = static_cast<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily> *>(commandList.get());
    ASSERT_EQ(ZE_RESULT_SUCCESS, immediateCmdList->checkAvailableSpace(0, false, 0, false));

    auto memoryManager = static_cast<MockMemoryManager *>(neoDevice->getExecutionEnvironment()->memoryManager.get());
    memoryManager->failInDevicePool = true;
    memoryManager->failInAllocateWithSizeAndAlignment = true;
    memoryManager->failInDevicePoolWithError = true;
    memoryManager->failAllocateSystemMemory = true;

    const size_t requestedSizeExceedingCommandBuffer = MemoryConstants::gigaByte;
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, immediateCmdList->checkAvailableSpace(0, false, requestedSizeExceedingCommandBuffer, false));

    memoryManager->failInDevicePool = false;
    memoryManager->failInAllocateWithSizeAndAlignment = false;
    memoryManager->failInDevicePoolWithError = false;
    memoryManager->failAllocateSystemMemory = false;
}

TEST_F(ImmediateCmdListDeferredInitializationTest, whenQueryingDeferredImmediateCmdListSupportOnBaseDeviceThenItIsEnabled) {
    EXPECT_TRUE(neoDevice->NEO::Device::isDeferredImmediateCmdListEnabled());
}

TEST_F(ImmediateCmdListDeferredInitializationTest, whenQueryingSecondaryEnginesAvailabilityOnBaseDeviceThenItMatchesSecondaryEnginesContainer) {
    const bool expectedAvailability = neoDevice->secondaryEngines.size() > 0;
    EXPECT_EQ(expectedAvailability, neoDevice->NEO::Device::areSecondaryEnginesAvailable());
}

TEST_F(ImmediateCmdListDeferredInitializationTest, givenSecondaryContextsSupportedAndNoSecondaryEnginesWhenObtainingCsrThenSecondaryContextIsNotAssigned) {
    NEO::debugManager.flags.ContextGroupSize.set(8);
    neoDevice->disableSecondaryEngines = true;
    ASSERT_TRUE(device->getGfxCoreHelper().areSecondaryContextsSupported());
    ASSERT_FALSE(neoDevice->areSecondaryEnginesAvailable());

    NEO::CommandStreamReceiver *csr = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getCsrForOrdinalAndIndex(&csr, 0, 0, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, std::nullopt));

    ASSERT_NE(nullptr, csr);
    EXPECT_EQ(neoDevice->getRegularEngineGroups()[0].engines[0].commandStreamReceiver, csr);
}

TEST(ImmediateCmdListDeferredInitialization, givenForcedBcsEngineIndexWithoutLinkedCopyEnginesWhenValidatingOrdinalAndIndexThenValidationFails) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ForceBcsEngineIndex.set(1);

    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo = 1;

    auto neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    auto driverHandle = std::make_unique<Mock<L0::DriverHandle>>();
    driverHandle->initialize(std::move(devices));
    auto device = driverHandle->devices[0];

    auto &engineGroups = device->getActiveDevice()->getRegularEngineGroups();
    uint32_t copyOrdinal = std::numeric_limits<uint32_t>::max();
    uint32_t linkedCopyOrdinal = std::numeric_limits<uint32_t>::max();
    for (uint32_t i = 0; i < engineGroups.size(); i++) {
        if (engineGroups[i].engineGroupType == NEO::EngineGroupType::copy) {
            copyOrdinal = i;
        }
        if (engineGroups[i].engineGroupType == NEO::EngineGroupType::linkedCopy) {
            linkedCopyOrdinal = i;
        }
    }
    if (copyOrdinal == std::numeric_limits<uint32_t>::max() || linkedCopyOrdinal != std::numeric_limits<uint32_t>::max()) {
        GTEST_SKIP();
    }

    EXPECT_FALSE(device->isQueueGroupOrdinalAndIndexValid(copyOrdinal, 0));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = copyOrdinal;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(hwInfo.platform.eProductFamily, device, &desc, false, NEO::EngineGroupType::copy, returnValue);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, returnValue);
    EXPECT_EQ(nullptr, commandList);
}

} // namespace ult
} // namespace L0
