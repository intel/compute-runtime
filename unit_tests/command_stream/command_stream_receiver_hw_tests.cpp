/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/command_stream/linear_stream.h"
#include "core/helpers/aligned_memory.h"
#include "core/helpers/ptr_math.h"
#include "core/os_interface/linux/debug_env_reader.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/built_ins/built_ins.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/command_stream/scratch_space_controller.h"
#include "runtime/command_stream/scratch_space_controller_base.h"
#include "runtime/event/user_event.h"
#include "runtime/helpers/blit_commands_helper.h"
#include "runtime/helpers/cache_policy.h"
#include "runtime/helpers/preamble.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/os_interface/os_context.h"
#include "test.h"
#include "unit_tests/fixtures/built_in_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/ult_command_stream_receiver_fixture.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/helpers/unit_test_helper.h"
#include "unit_tests/libult/create_command_stream.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_event.h"
#include "unit_tests/mocks/mock_internal_allocation_storage.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_submissions_aggregator.h"
#include "unit_tests/mocks/mock_timestamp_container.h"
#include "unit_tests/utilities/base_object_utils.h"

#include "reg_configs_common.h"

#include <memory>

using namespace NEO;

HWCMDTEST_F(IGFX_GEN8_CORE, UltCommandStreamReceiverTest, givenPreambleSentAndThreadArbitrationPolicyNotChangedWhenEstimatingPreambleCmdSizeThenReturnItsValue) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;
    commandStreamReceiver.requiredThreadArbitrationPolicy = commandStreamReceiver.lastSentThreadArbitrationPolicy;
    auto expectedCmdSize = sizeof(typename FamilyType::PIPE_CONTROL) + sizeof(typename FamilyType::MEDIA_VFE_STATE);
    EXPECT_EQ(expectedCmdSize, commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice));
}

HWCMDTEST_F(IGFX_GEN8_CORE, UltCommandStreamReceiverTest, givenNotSentStateSipWhenFirstTaskIsFlushedThenStateSipCmdIsAddedAndIsStateSipSentSetToTrue) {
    using STATE_SIP = typename FamilyType::STATE_SIP;

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    if (mockDevice->getHardwareInfo().capabilityTable.defaultPreemptionMode == PreemptionMode::MidThread) {
        mockDevice->setPreemptionMode(PreemptionMode::MidThread);

        auto &csr = mockDevice->getUltCommandStreamReceiver<FamilyType>();
        csr.isPreambleSent = true;

        CommandQueueHw<FamilyType> commandQueue(nullptr, mockDevice.get(), 0);
        auto &commandStream = commandQueue.getCS(4096u);

        DispatchFlags dispatchFlags;
        dispatchFlags.preemptionMode = PreemptionMode::MidThread;

        MockGraphicsAllocation allocation(nullptr, 0);
        IndirectHeap heap(&allocation);

        csr.flushTask(commandStream,
                      0,
                      heap,
                      heap,
                      heap,
                      0,
                      dispatchFlags,
                      *mockDevice);

        EXPECT_TRUE(csr.isStateSipSent);

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr.getCS(0));

        auto stateSipItor = find<STATE_SIP *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        EXPECT_NE(hwParser.cmdList.end(), stateSipItor);
    }
}

HWTEST_F(UltCommandStreamReceiverTest, givenCsrWhenProgramStateSipIsCalledThenIsStateSipCalledIsSetToTrue) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto requiredSize = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*pDevice);
    StackVec<char, 4096> buffer(requiredSize);
    LinearStream cmdStream(buffer.begin(), buffer.size());

    commandStreamReceiver.programStateSip(cmdStream, *pDevice);
    EXPECT_TRUE(commandStreamReceiver.isStateSipSent);
}

HWTEST_F(UltCommandStreamReceiverTest, givenSentStateSipFlagSetWhenGetRequiredStateSipCmdSizeIsCalledThenStateSipCmdSizeIsNotIncluded) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    DispatchFlags dispatchFlags;

    commandStreamReceiver.isStateSipSent = false;
    auto sizeWithStateSipIsNotSent = commandStreamReceiver.getRequiredCmdStreamSize(dispatchFlags, *pDevice);

    commandStreamReceiver.isStateSipSent = true;
    auto sizeWhenSipIsSent = commandStreamReceiver.getRequiredCmdStreamSize(dispatchFlags, *pDevice);

    auto sizeForStateSip = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*pDevice);
    EXPECT_EQ(sizeForStateSip, sizeWithStateSipIsNotSent - sizeWhenSipIsSent);
}

HWTEST_F(UltCommandStreamReceiverTest, givenSentStateSipFlagSetAndSourceLevelDebuggerIsActiveWhenGetRequiredStateSipCmdSizeIsCalledThenStateSipCmdSizeIsIncluded) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    DispatchFlags dispatchFlags;

    commandStreamReceiver.isStateSipSent = true;
    auto sizeWithoutSourceKernelDebugging = commandStreamReceiver.getRequiredCmdStreamSize(dispatchFlags, *pDevice);

    pDevice->setSourceLevelDebuggerActive(true);
    commandStreamReceiver.isStateSipSent = true;
    auto sizeWithSourceKernelDebugging = commandStreamReceiver.getRequiredCmdStreamSize(dispatchFlags, *pDevice);

    auto sizeForStateSip = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*pDevice);
    EXPECT_EQ(sizeForStateSip, sizeWithSourceKernelDebugging - sizeWithoutSourceKernelDebugging - PreambleHelper<FamilyType>::getKernelDebuggingCommandsSize(true));
}

HWTEST_F(UltCommandStreamReceiverTest, givenPreambleSentAndThreadArbitrationPolicyChangedWhenEstimatingPreambleCmdSizeThenResultDependsOnPolicyProgrammingCmdSize) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;

    commandStreamReceiver.requiredThreadArbitrationPolicy = commandStreamReceiver.lastSentThreadArbitrationPolicy;
    auto policyNotChanged = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);

    commandStreamReceiver.requiredThreadArbitrationPolicy = commandStreamReceiver.lastSentThreadArbitrationPolicy + 1;
    auto policyChanged = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);

    auto actualDifference = policyChanged - policyNotChanged;
    auto expectedDifference = PreambleHelper<FamilyType>::getThreadArbitrationCommandsSize();
    EXPECT_EQ(expectedDifference, actualDifference);
}

HWTEST_F(UltCommandStreamReceiverTest, givenPreambleSentWhenEstimatingPreambleCmdSizeThenResultDependsOnPolicyProgrammingAndAdditionalCmdsSize) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.requiredThreadArbitrationPolicy = commandStreamReceiver.lastSentThreadArbitrationPolicy;

    commandStreamReceiver.isPreambleSent = false;
    auto preambleNotSent = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);

    commandStreamReceiver.isPreambleSent = true;
    auto preambleSent = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);

    auto actualDifference = preambleNotSent - preambleSent;
    auto expectedDifference = PreambleHelper<FamilyType>::getThreadArbitrationCommandsSize() + PreambleHelper<FamilyType>::getAdditionalCommandsSize(*pDevice);
    EXPECT_EQ(expectedDifference, actualDifference);
}

HWTEST_F(UltCommandStreamReceiverTest, givenPerDssBackBufferProgrammingEnabledWhenEstimatingPreambleCmdSizeThenResultIncludesPerDssBackBufferProgramingCommandsSize) {
    DebugManagerStateRestore restore;
    DebugManager.flags.ForcePerDssBackedBufferProgramming.set(true);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.requiredThreadArbitrationPolicy = commandStreamReceiver.lastSentThreadArbitrationPolicy;

    commandStreamReceiver.isPreambleSent = false;
    auto preambleNotSent = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);

    commandStreamReceiver.isPreambleSent = true;
    auto preambleSent = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);

    auto actualDifference = preambleNotSent - preambleSent;
    auto expectedDifference = PreambleHelper<FamilyType>::getThreadArbitrationCommandsSize() + PreambleHelper<FamilyType>::getAdditionalCommandsSize(*pDevice) + PreambleHelper<FamilyType>::getPerDssBackedBufferCommandsSize(pDevice->getHardwareInfo());
    EXPECT_EQ(expectedDifference, actualDifference);
}

HWCMDTEST_F(IGFX_GEN8_CORE, UltCommandStreamReceiverTest, givenMediaVfeStateDirtyEstimatingPreambleCmdSizeThenResultDependsVfeStateProgrammingCmdSize) {
    typedef typename FamilyType::MEDIA_VFE_STATE MEDIA_VFE_STATE;
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.setMediaVFEStateDirty(false);
    auto notDirty = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);

    commandStreamReceiver.setMediaVFEStateDirty(true);
    auto dirty = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);

    auto actualDifference = dirty - notDirty;
    auto expectedDifference = sizeof(PIPE_CONTROL) + sizeof(MEDIA_VFE_STATE);
    EXPECT_EQ(expectedDifference, actualDifference);
}

HWTEST_F(UltCommandStreamReceiverTest, givenCommandStreamReceiverInInitialStateWhenHeapsAreAskedForDirtyStatusThenTrueIsReturned) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_EQ(0u, commandStreamReceiver.peekTaskCount());
    EXPECT_EQ(0u, commandStreamReceiver.peekTaskLevel());

    EXPECT_TRUE(commandStreamReceiver.dshState.updateAndCheck(&dsh));
    EXPECT_TRUE(commandStreamReceiver.iohState.updateAndCheck(&ioh));
    EXPECT_TRUE(commandStreamReceiver.sshState.updateAndCheck(&ssh));
}

typedef UltCommandStreamReceiverTest CommandStreamReceiverFlushTests;

HWTEST_F(CommandStreamReceiverFlushTests, addsBatchBufferEnd) {
    auto usedPrevious = commandStream.getUsed();

    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(commandStream, nullptr);

    EXPECT_EQ(commandStream.getUsed(), usedPrevious + sizeof(typename FamilyType::MI_BATCH_BUFFER_END));

    auto batchBufferEnd = genCmdCast<typename FamilyType::MI_BATCH_BUFFER_END *>(
        ptrOffset(commandStream.getCpuBase(), usedPrevious));
    EXPECT_NE(nullptr, batchBufferEnd);
}

HWTEST_F(CommandStreamReceiverFlushTests, shouldAlignToCacheLineSize) {
    commandStream.getSpace(sizeof(uint32_t));
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(commandStream);

    EXPECT_EQ(0u, commandStream.getUsed() % MemoryConstants::cacheLineSize);
}

typedef Test<DeviceFixture> CommandStreamReceiverHwTest;

HWTEST_F(CommandStreamReceiverHwTest, givenCsrHwWhenTypeIsCheckedThenCsrHwIsReturned) {
    auto csr = std::unique_ptr<CommandStreamReceiver>(CommandStreamReceiverHw<FamilyType>::create(*pDevice->executionEnvironment));

    EXPECT_EQ(CommandStreamReceiverType::CSR_HW, csr->getType());
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverHwTest, WhenCommandStreamReceiverHwIsCreatedThenDefaultSshSizeIs64KB) {
    auto &commandStreamReceiver = pDevice->getGpgpuCommandStreamReceiver();
    EXPECT_EQ(64 * KB, commandStreamReceiver.defaultSshSize);
}

HWTEST_F(CommandStreamReceiverHwTest, WhenScratchSpaceIsNotRequiredThenScratchAllocationIsNotCreated) {
    auto commandStreamReceiver = std::make_unique<MockCsrHw<FamilyType>>(*pDevice->executionEnvironment);
    auto scratchController = commandStreamReceiver->getScratchSpaceController();

    bool stateBaseAddressDirty = false;
    bool cfeStateDirty = false;
    scratchController->setRequiredScratchSpace(reinterpret_cast<void *>(0x2000), 0u, 0u, 0u, 0u, stateBaseAddressDirty, cfeStateDirty);
    EXPECT_FALSE(cfeStateDirty);
    EXPECT_FALSE(stateBaseAddressDirty);
    EXPECT_EQ(nullptr, scratchController->getScratchSpaceAllocation());
    EXPECT_EQ(nullptr, scratchController->getPrivateScratchSpaceAllocation());
}

HWTEST_F(CommandStreamReceiverHwTest, WhenScratchSpaceIsRequiredThenCorrectAddressIsReturned) {
    auto commandStreamReceiver = std::make_unique<MockCsrHw<FamilyType>>(*pDevice->executionEnvironment);
    auto scratchController = commandStreamReceiver->getScratchSpaceController();

    bool cfeStateDirty = false;
    bool stateBaseAddressDirty = false;

    std::unique_ptr<void, std::function<decltype(alignedFree)>> surfaceHeap(alignedMalloc(0x1000, 0x1000), alignedFree);
    scratchController->setRequiredScratchSpace(surfaceHeap.get(), 0x1000u, 0u, 0u, 0u, stateBaseAddressDirty, cfeStateDirty);

    uint64_t expectedScratchAddress = 0xAAABBBCCCDDD000ull;
    auto scratchAllocation = scratchController->getScratchSpaceAllocation();
    scratchAllocation->setCpuPtrAndGpuAddress(scratchAllocation->getUnderlyingBuffer(), expectedScratchAddress);
    EXPECT_TRUE(UnitTestHelper<FamilyType>::evaluateGshAddressForScratchSpace((scratchAllocation->getGpuAddress() - MemoryConstants::pageSize), scratchController->calculateNewGSH()));
}

HWTEST_F(CommandStreamReceiverHwTest, WhenScratchSpaceIsNotRequiredThenGshAddressZeroIsReturned) {
    auto commandStreamReceiver = std::make_unique<MockCsrHw<FamilyType>>(*pDevice->executionEnvironment);
    auto scratchController = commandStreamReceiver->getScratchSpaceController();

    EXPECT_EQ(nullptr, scratchController->getScratchSpaceAllocation());
    EXPECT_EQ(0u, scratchController->calculateNewGSH());
}

struct BcsTests : public CommandStreamReceiverHwTest {
    void SetUp() override {
        CommandStreamReceiverHwTest::SetUp();

        auto &csr = pDevice->getGpgpuCommandStreamReceiver();
        auto engine = csr.getMemoryManager()->getRegisteredEngineForCsr(&csr);
        auto contextId = engine->osContext->getContextId();

        delete engine->osContext;
        engine->osContext = OsContext::create(nullptr, contextId, 0, aub_stream::EngineType::ENGINE_BCS, PreemptionMode::Disabled, false);
        engine->osContext->incRefInternal();
        csr.setupContext(*engine->osContext);

        context = std::make_unique<MockContext>(pDevice);
    }

    void TearDown() override {
        context.reset();
        CommandStreamReceiverHwTest::TearDown();
    }

    TimestampPacketContainer timestampPacketContainer;
    CsrDependencies csrDependencies;
    std::unique_ptr<MockContext> context;
};

HWTEST_F(BcsTests, givenBltSizeWhenEstimatingCommandSizeThenAddAllRequiredCommands) {
    constexpr auto max2DBlitSize = BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight;
    uint64_t notAlignedBltSize = (3 * max2DBlitSize) + 1;
    uint64_t alignedBltSize = (3 * max2DBlitSize);
    uint32_t alignedNumberOfBlts = 3;
    uint32_t notAlignedNumberOfBlts = 4;

    size_t expectedSize = sizeof(typename FamilyType::MI_FLUSH_DW) + sizeof(typename FamilyType::MI_BATCH_BUFFER_END);

    auto expectedAlignedSize = alignUp(expectedSize + (sizeof(typename FamilyType::XY_COPY_BLT) * alignedNumberOfBlts), MemoryConstants::cacheLineSize);
    auto expectedNotAlignedSize = alignUp(expectedSize + (sizeof(typename FamilyType::XY_COPY_BLT) * notAlignedNumberOfBlts), MemoryConstants::cacheLineSize);

    auto alignedEstimatedSize = BlitCommandsHelper<FamilyType>::estimateBlitCommandsSize(alignedBltSize, csrDependencies, false);
    auto notAlignedEstimatedSize = BlitCommandsHelper<FamilyType>::estimateBlitCommandsSize(notAlignedBltSize, csrDependencies, false);

    EXPECT_EQ(expectedAlignedSize, alignedEstimatedSize);
    EXPECT_EQ(expectedNotAlignedSize, notAlignedEstimatedSize);
}

HWTEST_F(BcsTests, givenTimestampPacketWriteRequestWhenEstimatingSizeForCommandsThenAddMiFlushDw) {
    size_t expectedBaseSize = sizeof(typename FamilyType::XY_COPY_BLT) + sizeof(typename FamilyType::MI_FLUSH_DW) +
                              sizeof(typename FamilyType::MI_BATCH_BUFFER_END);

    auto expectedSizeWithTimestampPacketWrite = alignUp(expectedBaseSize + sizeof(typename FamilyType::MI_FLUSH_DW), MemoryConstants::cacheLineSize);
    auto expectedSizeWithoutTimestampPacketWrite = alignUp(expectedBaseSize, MemoryConstants::cacheLineSize);

    auto estimatedSizeWithTimestampPacketWrite = BlitCommandsHelper<FamilyType>::estimateBlitCommandsSize(1, csrDependencies, true);
    auto estimatedSizeWithoutTimestampPacketWrite = BlitCommandsHelper<FamilyType>::estimateBlitCommandsSize(1, csrDependencies, false);

    EXPECT_EQ(expectedSizeWithTimestampPacketWrite, estimatedSizeWithTimestampPacketWrite);
    EXPECT_EQ(expectedSizeWithoutTimestampPacketWrite, estimatedSizeWithoutTimestampPacketWrite);
}

HWTEST_F(BcsTests, givenBltSizeAndCsrDependenciesWhenEstimatingCommandSizeThenAddAllRequiredCommands) {
    uint32_t numberOfBlts = 1;
    size_t numberNodesPerContainer = 5;
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    MockTimestampPacketContainer timestamp0(*csr.getTimestampPacketAllocator(), numberNodesPerContainer);
    MockTimestampPacketContainer timestamp1(*csr.getTimestampPacketAllocator(), numberNodesPerContainer);
    csrDependencies.push_back(&timestamp0);
    csrDependencies.push_back(&timestamp1);

    size_t expectedSize = sizeof(typename FamilyType::MI_FLUSH_DW) + sizeof(typename FamilyType::MI_BATCH_BUFFER_END) +
                          (sizeof(typename FamilyType::XY_COPY_BLT) * numberOfBlts) +
                          TimestampPacketHelper::getRequiredCmdStreamSize<FamilyType>(csrDependencies);

    auto expectedAlignedSize = alignUp(expectedSize, MemoryConstants::cacheLineSize);

    auto estimatedSize = BlitCommandsHelper<FamilyType>::estimateBlitCommandsSize(1, csrDependencies, false);

    EXPECT_EQ(expectedAlignedSize, estimatedSize);
}

HWTEST_F(BcsTests, givenBltSizeWithLeftoverWhenDispatchedThenProgramAllRequiredCommands) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    constexpr auto max2DBlitSize = BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight;

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    static_cast<OsAgnosticMemoryManager *>(csr.getMemoryManager())->turnOnFakingBigAllocations();

    uint32_t bltLeftover = 17;
    uint64_t bltSize = (2 * max2DBlitSize) + bltLeftover;
    uint32_t numberOfBlts = 3;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, static_cast<size_t>(bltSize), nullptr, retVal));
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    uint32_t newTaskCount = 19;
    csr.taskCount = newTaskCount - 1;
    EXPECT_EQ(0u, csr.recursiveLockCounter.load());
    auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                csr, buffer->getGraphicsAllocation(), hostPtr, true, 0, bltSize);

    csr.blitBuffer(blitProperties);
    EXPECT_EQ(newTaskCount, csr.taskCount);
    EXPECT_EQ(newTaskCount, csr.latestFlushedTaskCount);
    EXPECT_EQ(newTaskCount, csr.latestSentTaskCount);
    EXPECT_EQ(newTaskCount, csr.latestSentTaskCountValueDuringFlush);
    EXPECT_EQ(1u, csr.recursiveLockCounter.load());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr.commandStream);
    auto &cmdList = hwParser.cmdList;

    auto cmdIterator = cmdList.begin();

    for (uint32_t i = 0; i < numberOfBlts; i++) {
        auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*(cmdIterator++));
        EXPECT_NE(nullptr, bltCmd);

        EXPECT_EQ(0u, bltCmd->getDestinationX1CoordinateLeft());
        EXPECT_EQ(0u, bltCmd->getDestinationY1CoordinateTop());
        EXPECT_EQ(0u, bltCmd->getSourceX1CoordinateLeft());
        EXPECT_EQ(0u, bltCmd->getSourceY1CoordinateTop());
        uint32_t expectedWidth = static_cast<uint32_t>(BlitterConstants::maxBlitWidth);
        uint32_t expectedHeight = static_cast<uint32_t>(BlitterConstants::maxBlitHeight);
        if (i == (numberOfBlts - 1)) {
            expectedWidth = bltLeftover;
            expectedHeight = 1;
        }
        EXPECT_EQ(expectedWidth, bltCmd->getDestinationX2CoordinateRight());
        EXPECT_EQ(expectedHeight, bltCmd->getDestinationY2CoordinateBottom());
        EXPECT_EQ(expectedWidth, bltCmd->getDestinationPitch());
        EXPECT_EQ(expectedWidth, bltCmd->getSourcePitch());
    }

    auto miFlushCmd = genCmdCast<MI_FLUSH_DW *>(*(cmdIterator++));
    EXPECT_NE(nullptr, miFlushCmd);
    EXPECT_EQ(MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD, miFlushCmd->getPostSyncOperation());
    EXPECT_EQ(csr.getTagAllocation()->getGpuAddress(), miFlushCmd->getDestinationAddress());
    EXPECT_EQ(newTaskCount, miFlushCmd->getImmediateData());

    EXPECT_NE(nullptr, genCmdCast<typename FamilyType::MI_BATCH_BUFFER_END *>(*(cmdIterator++)));

    // padding
    while (cmdIterator != cmdList.end()) {
        EXPECT_NE(nullptr, genCmdCast<typename FamilyType::MI_NOOP *>(*(cmdIterator++)));
    }
}

HWTEST_F(BcsTests, givenCsrDependenciesWhenProgrammingCommandStreamThenAddSemaphoreAndAtomic) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    void *hostPtr = reinterpret_cast<void *>(0x12340000);
    uint32_t numberOfDependencyContainers = 2;
    size_t numberNodesPerContainer = 5;

    auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                csr, buffer->getGraphicsAllocation(), hostPtr, true, 0, 1);

    MockTimestampPacketContainer timestamp0(*csr.getTimestampPacketAllocator(), numberNodesPerContainer);
    MockTimestampPacketContainer timestamp1(*csr.getTimestampPacketAllocator(), numberNodesPerContainer);
    blitProperties.csrDependencies.push_back(&timestamp0);
    blitProperties.csrDependencies.push_back(&timestamp1);

    csr.blitBuffer(blitProperties);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr.commandStream);
    auto &cmdList = hwParser.cmdList;
    bool xyCopyBltCmdFound = false;
    bool dependenciesFound = false;

    for (auto cmdIterator = cmdList.begin(); cmdIterator != cmdList.end(); cmdIterator++) {
        if (genCmdCast<typename FamilyType::XY_COPY_BLT *>(*cmdIterator)) {
            xyCopyBltCmdFound = true;
            continue;
        }
        auto miSemaphore = genCmdCast<typename FamilyType::MI_SEMAPHORE_WAIT *>(*cmdIterator);
        if (miSemaphore) {
            dependenciesFound = true;
            EXPECT_FALSE(xyCopyBltCmdFound);
            auto miAtomic = genCmdCast<typename FamilyType::MI_ATOMIC *>(*(++cmdIterator));
            EXPECT_NE(nullptr, miAtomic);

            for (uint32_t i = 1; i < numberOfDependencyContainers * numberNodesPerContainer; i++) {
                EXPECT_NE(nullptr, genCmdCast<typename FamilyType::MI_SEMAPHORE_WAIT *>(*(++cmdIterator)));
                EXPECT_NE(nullptr, genCmdCast<typename FamilyType::MI_ATOMIC *>(*(++cmdIterator)));
            }
        }
    }
    EXPECT_TRUE(xyCopyBltCmdFound);
    EXPECT_TRUE(dependenciesFound);
}

HWTEST_F(BcsTests, givenInputAllocationsWhenBlitDispatchedThenMakeAllAllocationsResident) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.storeMakeResidentAllocations = true;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    EXPECT_EQ(0u, csr.makeSurfacePackNonResidentCalled);

    auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                csr, buffer->getGraphicsAllocation(), hostPtr, true, 0, 1);

    csr.blitBuffer(blitProperties);

    EXPECT_TRUE(csr.isMadeResident(buffer->getGraphicsAllocation()));
    EXPECT_TRUE(csr.isMadeResident(csr.commandStream.getGraphicsAllocation()));
    EXPECT_TRUE(csr.isMadeResident(csr.getTagAllocation()));
    EXPECT_EQ(1u, csr.makeSurfacePackNonResidentCalled);

    EXPECT_EQ(4u, csr.makeResidentAllocations.size());
}

HWTEST_F(BcsTests, givenBufferWhenBlitCalledThenFlushCommandBuffer) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.recordFlusheBatchBuffer = true;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    auto &commandStream = csr.getCS(MemoryConstants::pageSize);
    size_t commandStreamOffset = 4;
    commandStream.getSpace(commandStreamOffset);

    uint32_t newTaskCount = 17;
    csr.taskCount = newTaskCount - 1;

    auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                csr, buffer->getGraphicsAllocation(), hostPtr, true, 0, 1);

    csr.blitBuffer(blitProperties);

    EXPECT_EQ(commandStream.getGraphicsAllocation(), csr.latestFlushedBatchBuffer.commandBufferAllocation);
    EXPECT_EQ(commandStreamOffset, csr.latestFlushedBatchBuffer.startOffset);
    EXPECT_EQ(0u, csr.latestFlushedBatchBuffer.chainedBatchBufferStartOffset);
    EXPECT_EQ(nullptr, csr.latestFlushedBatchBuffer.chainedBatchBuffer);
    EXPECT_FALSE(csr.latestFlushedBatchBuffer.requiresCoherency);
    EXPECT_FALSE(csr.latestFlushedBatchBuffer.low_priority);
    EXPECT_EQ(QueueThrottle::MEDIUM, csr.latestFlushedBatchBuffer.throttle);
    EXPECT_EQ(commandStream.getUsed(), csr.latestFlushedBatchBuffer.usedSize);
    EXPECT_EQ(&commandStream, csr.latestFlushedBatchBuffer.stream);

    EXPECT_EQ(newTaskCount, csr.latestWaitForCompletionWithTimeoutTaskCount.load());
}

HWTEST_F(BcsTests, whenBlitFromHostPtrCalledThenCallWaitWithKmdFallback) {
    class MyMockCsr : public UltCommandStreamReceiver<FamilyType> {
      public:
        using UltCommandStreamReceiver<FamilyType>::UltCommandStreamReceiver;

        void waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait,
                                                   bool useQuickKmdSleep, bool forcePowerSavingMode) override {
            waitForTaskCountWithKmdNotifyFallbackCalled++;
            taskCountToWaitPassed = taskCountToWait;
            flushStampToWaitPassed = flushStampToWait;
            useQuickKmdSleepPassed = useQuickKmdSleep;
            forcePowerSavingModePassed = forcePowerSavingMode;
        }

        uint32_t taskCountToWaitPassed = 0;
        FlushStamp flushStampToWaitPassed = 0;
        bool useQuickKmdSleepPassed = false;
        bool forcePowerSavingModePassed = false;
        uint32_t waitForTaskCountWithKmdNotifyFallbackCalled = 0;
    };

    auto myMockCsr = std::make_unique<::testing::NiceMock<MyMockCsr>>(*pDevice->getExecutionEnvironment());
    auto &bcsOsContext = pDevice->getUltCommandStreamReceiver<FamilyType>().getOsContext();
    myMockCsr->initializeTagAllocation();
    myMockCsr->setupContext(bcsOsContext);

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                *myMockCsr, buffer->getGraphicsAllocation(), hostPtr, false, 0, 1);

    myMockCsr->blitBuffer(blitProperties);

    EXPECT_EQ(0u, myMockCsr->waitForTaskCountWithKmdNotifyFallbackCalled);

    blitProperties.blocking = true;
    myMockCsr->blitBuffer(blitProperties);

    EXPECT_EQ(1u, myMockCsr->waitForTaskCountWithKmdNotifyFallbackCalled);
    EXPECT_EQ(myMockCsr->taskCount, myMockCsr->taskCountToWaitPassed);
    EXPECT_EQ(myMockCsr->flushStamp->peekStamp(), myMockCsr->flushStampToWaitPassed);
    EXPECT_FALSE(myMockCsr->useQuickKmdSleepPassed);
    EXPECT_FALSE(myMockCsr->forcePowerSavingModePassed);
}

HWTEST_F(BcsTests, whenBlitFromHostPtrCalledThenCleanTemporaryAllocations) {
    auto &bcsCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto mockInternalAllocationsStorage = new MockInternalAllocationStorage(bcsCsr);
    bcsCsr.internalAllocationStorage.reset(mockInternalAllocationsStorage);

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    bcsCsr.taskCount = 17;

    EXPECT_EQ(0u, mockInternalAllocationsStorage->cleanAllocationsCalled);

    auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                bcsCsr, buffer->getGraphicsAllocation(), hostPtr, false, 0, 1);

    bcsCsr.blitBuffer(blitProperties);

    EXPECT_EQ(0u, mockInternalAllocationsStorage->cleanAllocationsCalled);

    blitProperties.blocking = true;
    bcsCsr.blitBuffer(blitProperties);

    EXPECT_EQ(1u, mockInternalAllocationsStorage->cleanAllocationsCalled);
    EXPECT_EQ(bcsCsr.taskCount, mockInternalAllocationsStorage->lastCleanAllocationsTaskCount);
    EXPECT_TRUE(TEMPORARY_ALLOCATION == mockInternalAllocationsStorage->lastCleanAllocationUsage);
}

HWTEST_F(BcsTests, givenBufferWhenBlitOperationCalledThenProgramCorrectGpuAddresses) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    cl_int retVal = CL_SUCCESS;
    auto buffer1 = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto buffer2 = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    {
        // from hostPtr
        HardwareParse hwParser;
        auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                    csr, buffer1->getGraphicsAllocation(), hostPtr, true, 0, 1);

        csr.blitBuffer(blitProperties);

        hwParser.parseCommands<FamilyType>(csr.commandStream);

        auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*hwParser.cmdList.begin());
        EXPECT_NE(nullptr, bltCmd);
        if (pDevice->isFullRangeSvm()) {
            EXPECT_EQ(reinterpret_cast<uint64_t>(hostPtr), bltCmd->getSourceBaseAddress());
        }
        EXPECT_EQ(buffer1->getGraphicsAllocation()->getGpuAddress(), bltCmd->getDestinationBaseAddress());
    }
    {
        // to hostPtr
        HardwareParse hwParser;
        auto offset = csr.commandStream.getUsed();
        auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::BufferToHostPtr,
                                                                                    csr, buffer1->getGraphicsAllocation(), hostPtr, true, 0, 1);

        csr.blitBuffer(blitProperties);

        hwParser.parseCommands<FamilyType>(csr.commandStream, offset);

        auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*hwParser.cmdList.begin());
        EXPECT_NE(nullptr, bltCmd);
        if (pDevice->isFullRangeSvm()) {
            EXPECT_EQ(reinterpret_cast<uint64_t>(hostPtr), bltCmd->getDestinationBaseAddress());
        }
        EXPECT_EQ(buffer1->getGraphicsAllocation()->getGpuAddress(), bltCmd->getSourceBaseAddress());
    }
    {
        // Buffer to Buffer
        HardwareParse hwParser;
        auto offset = csr.commandStream.getUsed();
        auto blitProperties = BlitProperties::constructPropertiesForCopyBuffer(buffer1->getGraphicsAllocation(),
                                                                               buffer2->getGraphicsAllocation(),
                                                                               true, 0, 0, 1);

        csr.blitBuffer(blitProperties);

        hwParser.parseCommands<FamilyType>(csr.commandStream, offset);

        auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*hwParser.cmdList.begin());
        EXPECT_NE(nullptr, bltCmd);
        EXPECT_EQ(buffer1->getGraphicsAllocation()->getGpuAddress(), bltCmd->getDestinationBaseAddress());
        EXPECT_EQ(buffer2->getGraphicsAllocation()->getGpuAddress(), bltCmd->getSourceBaseAddress());
    }
}

HWTEST_F(BcsTests, givenBufferWithOffsetWhenBlitOperationCalledThenProgramCorrectGpuAddresses) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    cl_int retVal = CL_SUCCESS;
    auto buffer1 = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto buffer2 = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    void *hostPtr = reinterpret_cast<void *>(0x12340000);

    size_t addressOffsets[] = {0, 1, 1234};

    for (auto buffer1Offset : addressOffsets) {
        {
            // from hostPtr
            HardwareParse hwParser;
            auto offset = csr.commandStream.getUsed();
            auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                        csr, buffer1->getGraphicsAllocation(), hostPtr, true, buffer1Offset, 1);

            csr.blitBuffer(blitProperties);

            hwParser.parseCommands<FamilyType>(csr.commandStream, offset);

            auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*hwParser.cmdList.begin());
            EXPECT_NE(nullptr, bltCmd);
            if (pDevice->isFullRangeSvm()) {
                EXPECT_EQ(reinterpret_cast<uint64_t>(hostPtr), bltCmd->getSourceBaseAddress());
            }
            EXPECT_EQ(ptrOffset(buffer1->getGraphicsAllocation()->getGpuAddress(), buffer1Offset), bltCmd->getDestinationBaseAddress());
        }
        {
            // to hostPtr
            HardwareParse hwParser;
            auto offset = csr.commandStream.getUsed();
            auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::BufferToHostPtr,
                                                                                        csr, buffer1->getGraphicsAllocation(), hostPtr, true, buffer1Offset, 1);

            csr.blitBuffer(blitProperties);

            hwParser.parseCommands<FamilyType>(csr.commandStream, offset);

            auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*hwParser.cmdList.begin());
            EXPECT_NE(nullptr, bltCmd);
            if (pDevice->isFullRangeSvm()) {
                EXPECT_EQ(reinterpret_cast<uint64_t>(hostPtr), bltCmd->getDestinationBaseAddress());
            }
            EXPECT_EQ(ptrOffset(buffer1->getGraphicsAllocation()->getGpuAddress(), buffer1Offset), bltCmd->getSourceBaseAddress());
        }

        for (auto buffer2Offset : addressOffsets) {
            // Buffer to Buffer
            HardwareParse hwParser;
            auto offset = csr.commandStream.getUsed();
            auto blitProperties = BlitProperties::constructPropertiesForCopyBuffer(buffer1->getGraphicsAllocation(),
                                                                                   buffer2->getGraphicsAllocation(),
                                                                                   true, buffer1Offset, buffer2Offset, 1);

            csr.blitBuffer(blitProperties);

            hwParser.parseCommands<FamilyType>(csr.commandStream, offset);

            auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*hwParser.cmdList.begin());
            EXPECT_NE(nullptr, bltCmd);
            EXPECT_EQ(ptrOffset(buffer1->getGraphicsAllocation()->getGpuAddress(), buffer1Offset), bltCmd->getDestinationBaseAddress());
            EXPECT_EQ(ptrOffset(buffer2->getGraphicsAllocation()->getGpuAddress(), buffer2Offset), bltCmd->getSourceBaseAddress());
        }
    }
}

HWTEST_F(BcsTests, givenAuxTranslationRequestWhenBlitCalledThenProgramCommandCorrectly) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 123, nullptr, retVal));
    auto allocationGpuAddress = buffer->getGraphicsAllocation()->getGpuAddress();
    auto allocationSize = buffer->getGraphicsAllocation()->getUnderlyingBufferSize();

    AuxTranslationDirection translationDirection[] = {AuxTranslationDirection::AuxToNonAux, AuxTranslationDirection::NonAuxToAux};

    for (int i = 0; i < 2; i++) {
        auto blitProperties = BlitProperties::constructPropertiesForAuxTranslation(translationDirection[i],
                                                                                   buffer->getGraphicsAllocation());

        auto offset = csr.commandStream.getUsed();
        csr.blitBuffer(blitProperties);

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr.commandStream, offset);
        uint32_t xyCopyBltCmdFound = 0;

        for (auto &cmd : hwParser.cmdList) {
            if (auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(cmd)) {
                xyCopyBltCmdFound++;
                EXPECT_EQ(static_cast<uint32_t>(allocationSize), bltCmd->getDestinationX2CoordinateRight());
                EXPECT_EQ(1u, bltCmd->getDestinationY2CoordinateBottom());

                EXPECT_EQ(allocationGpuAddress, bltCmd->getDestinationBaseAddress());
                EXPECT_EQ(allocationGpuAddress, bltCmd->getSourceBaseAddress());
            }
        }
        EXPECT_EQ(1u, xyCopyBltCmdFound);
    }
}

struct MockScratchSpaceController : ScratchSpaceControllerBase {
    using ScratchSpaceControllerBase::privateScratchAllocation;
    using ScratchSpaceControllerBase::ScratchSpaceControllerBase;
};

using ScratchSpaceControllerTest = Test<DeviceFixture>;

TEST_F(ScratchSpaceControllerTest, whenScratchSpaceControllerIsDestroyedThenItReleasePrivateScratchSpaceAllocation) {
    MockScratchSpaceController scratchSpaceController(*pDevice->getExecutionEnvironment(), *pDevice->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    scratchSpaceController.privateScratchAllocation = pDevice->getExecutionEnvironment()->memoryManager->allocateGraphicsMemoryInPreferredPool(MockAllocationProperties{MemoryConstants::pageSize}, nullptr);
    EXPECT_NE(nullptr, scratchSpaceController.privateScratchAllocation);
    //no memory leak is expected
}
