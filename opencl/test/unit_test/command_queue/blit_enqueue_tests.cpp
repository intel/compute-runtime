/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/vec.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/utilities/base_object_utils.h"

#include "opencl/source/event/user_event.h"
#include "opencl/test/unit_test/helpers/hw_parse.h"
#include "opencl/test/unit_test/helpers/unit_test_helper.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_timestamp_container.h"
#include "test.h"

using namespace NEO;

struct BlitAuxTranslationTests : public ::testing::Test {
    class BcsMockContext : public MockContext {
      public:
        BcsMockContext(ClDevice *device) : MockContext(device) {
            bcsOsContext.reset(OsContext::create(nullptr, 0, 0, aub_stream::ENGINE_BCS, PreemptionMode::Disabled,
                                                 false, false, false));
            bcsCsr.reset(createCommandStream(*device->getExecutionEnvironment(), device->getRootDeviceIndex()));
            bcsCsr->setupContext(*bcsOsContext);
            bcsCsr->initializeTagAllocation();
        }

        BlitOperationResult blitMemoryToAllocation(MemObj &memObj, GraphicsAllocation *memory, void *hostPtr, Vec3<size_t> size) const override {
            auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                        *bcsCsr, memory, nullptr,
                                                                                        hostPtr,
                                                                                        memory->getGpuAddress(), 0,
                                                                                        0, 0, size, 0, 0, 0, 0);

            BlitPropertiesContainer container;
            container.push_back(blitProperties);
            bcsCsr->blitBuffer(container, true);

            return BlitOperationResult::Success;
        }
        std::unique_ptr<OsContext> bcsOsContext;
        std::unique_ptr<CommandStreamReceiver> bcsCsr;
    };

    template <typename FamilyType>
    void SetUpT() {
        auto &hwHelper = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
        if (is32bit || !hwHelper.requiresAuxResolves()) {
            GTEST_SKIP();
        }
        DebugManager.flags.EnableTimestampPacket.set(1);
        DebugManager.flags.EnableBlitterOperationsForReadWriteBuffers.set(1);
        DebugManager.flags.ForceAuxTranslationMode.set(1);
        DebugManager.flags.CsrDispatchMode.set(static_cast<int32_t>(DispatchMode::ImmediateDispatch));
        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
        auto &capabilityTable = device->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable;
        bool createBcsEngine = !capabilityTable.blitterOperationsSupported;
        capabilityTable.blitterOperationsSupported = true;

        if (createBcsEngine) {
            auto &engine = device->getEngine(HwHelperHw<FamilyType>::lowPriorityEngineType, true);
            bcsOsContext.reset(OsContext::create(nullptr, 1, 0, aub_stream::ENGINE_BCS, PreemptionMode::Disabled,
                                                 false, false, false));
            engine.osContext = bcsOsContext.get();
            engine.commandStreamReceiver->setupContext(*bcsOsContext);
        }
        bcsMockContext = std::make_unique<BcsMockContext>(device.get());
        auto mockCmdQueue = new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr);
        commandQueue.reset(mockCmdQueue);
        mockKernel = std::make_unique<MockKernelWithInternals>(*device, bcsMockContext.get());
        mockKernel->mockKernel->auxTranslationRequired = true;
        auto mockProgram = mockKernel->mockProgram;
        mockProgram->setAllowNonUniform(true);

        gpgpuCsr = mockCmdQueue->gpgpuEngine->commandStreamReceiver;
        bcsCsr = mockCmdQueue->bcsEngine->commandStreamReceiver;
    }

    template <typename FamilyType>
    void TearDownT() {}

    template <size_t N>
    void setMockKernelArgs(std::array<Buffer *, N> buffers) {
        if (mockKernel->kernelInfo.kernelArgInfo.size() < buffers.size()) {
            mockKernel->kernelInfo.kernelArgInfo.resize(buffers.size());
        }
        mockKernel->mockKernel->initialize();

        for (uint32_t i = 0; i < buffers.size(); i++) {
            cl_mem clMem = buffers[i];

            mockKernel->kernelInfo.kernelArgInfo.at(i).kernelArgPatchInfoVector.resize(1);
            mockKernel->kernelInfo.kernelArgInfo.at(i).pureStatefulBufferAccess = false;
            mockKernel->mockKernel->setArgBuffer(i, sizeof(cl_mem *), &clMem);
        }
    }

    ReleaseableObjectPtr<Buffer> createBuffer(size_t size, bool compressed) {
        auto buffer = clUniquePtr<Buffer>(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, size, nullptr, retVal));
        if (compressed) {
            buffer->getGraphicsAllocation()->setAllocationType(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
        } else {
            buffer->getGraphicsAllocation()->setAllocationType(GraphicsAllocation::AllocationType::BUFFER);
        }
        return buffer;
    }

    template <typename Family>
    GenCmdList getCmdList(LinearStream &linearStream) {
        HardwareParse hwParser;
        hwParser.parseCommands<Family>(linearStream);

        return hwParser.cmdList;
    }

    template <typename Family>
    GenCmdList::iterator expectPipeControl(GenCmdList::iterator itorStart, GenCmdList::iterator itorEnd) {
        using PIPE_CONTROL = typename Family::PIPE_CONTROL;
        PIPE_CONTROL *pipeControlCmd = nullptr;
        GenCmdList::iterator commandItor = itorStart;
        bool stallingWrite = false;

        do {
            commandItor = find<PIPE_CONTROL *>(commandItor, itorEnd);
            if (itorEnd == commandItor) {
                EXPECT_TRUE(false);
                return itorEnd;
            }
            pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*commandItor);
            stallingWrite = pipeControlCmd->getPostSyncOperation() == PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA &&
                            pipeControlCmd->getCommandStreamerStallEnable();

            ++commandItor;
        } while (!stallingWrite);

        return --commandItor;
    }

    template <typename Family>
    GenCmdList::iterator expectMiFlush(GenCmdList::iterator itorStart, GenCmdList::iterator itorEnd) {
        Family *miFlushCmd = nullptr;
        GenCmdList::iterator commandItor = itorStart;
        bool miFlushWithMemoryWrite = false;

        do {
            commandItor = find<Family *>(commandItor, itorEnd);
            if (itorEnd == commandItor) {
                EXPECT_TRUE(false);
                return itorEnd;
            }
            miFlushCmd = genCmdCast<Family *>(*commandItor);
            miFlushWithMemoryWrite = miFlushCmd->getDestinationAddress() != 0;

            ++commandItor;
        } while (!miFlushWithMemoryWrite);

        return --commandItor;
    }

    template <typename Command>
    GenCmdList::iterator expectCommand(GenCmdList::iterator itorStart, GenCmdList::iterator itorEnd) {
        auto commandItor = find<Command *>(itorStart, itorEnd);
        EXPECT_TRUE(commandItor != itorEnd);

        return commandItor;
    }

    template <typename Family>
    void verifySemaphore(GenCmdList::iterator &semaphoreItor, uint64_t expectedAddress) {
        using MI_SEMAPHORE_WAIT = typename Family::MI_SEMAPHORE_WAIT;

        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
        EXPECT_EQ(expectedAddress, semaphoreCmd->getSemaphoreGraphicsAddress());
    }

    DebugManagerStateRestore restore;

    std::unique_ptr<OsContext> bcsOsContext;
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<BcsMockContext> bcsMockContext;
    std::unique_ptr<CommandQueue> commandQueue;
    std::unique_ptr<MockKernelWithInternals> mockKernel;

    CommandStreamReceiver *bcsCsr = nullptr;
    CommandStreamReceiver *gpgpuCsr = nullptr;

    size_t gws[3] = {63, 0, 0};
    size_t lws[3] = {16, 0, 0};
    uint32_t hostPtr = 0;
    cl_int retVal = CL_SUCCESS;
};

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitAuxTranslationWhenConstructingCommandBufferThenEnsureCorrectOrder) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    auto buffer0 = createBuffer(1, true);
    auto buffer1 = createBuffer(1, false);
    auto buffer2 = createBuffer(1, true);
    setMockKernelArgs(std::array<Buffer *, 3>{{buffer0.get(), buffer1.get(), buffer2.get()}});

    auto mockCmdQ = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    auto initialBcsTaskCount = mockCmdQ->bcsTaskCount;

    mockCmdQ->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);

    EXPECT_EQ(mockCmdQ->bcsTaskCount, initialBcsTaskCount + 1);

    // Gpgpu command buffer
    {
        auto cmdListCsr = getCmdList<FamilyType>(gpgpuCsr->getCS(0));
        auto cmdListQueue = getCmdList<FamilyType>(commandQueue->getCS(0));

        // Barrier
        expectPipeControl<FamilyType>(cmdListCsr.begin(), cmdListCsr.end());

        // Aux to NonAux
        auto cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(cmdListQueue.begin(), cmdListQueue.end());
        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());
        // Walker
        cmdFound = expectCommand<WALKER_TYPE>(++cmdFound, cmdListQueue.end());
        cmdFound = expectCommand<WALKER_TYPE>(++cmdFound, cmdListQueue.end());
        // NonAux to Aux
        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());
        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());

        // task count
        expectPipeControl<FamilyType>(++cmdFound, cmdListQueue.end());
    }

    // BCS command buffer
    {
        auto cmdList = getCmdList<FamilyType>(bcsCsr->getCS(0));

        // Barrier
        auto cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(cmdList.begin(), cmdList.end());

        // Aux to NonAux
        cmdFound = expectCommand<XY_COPY_BLT>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<MI_FLUSH_DW>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<XY_COPY_BLT>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<MI_FLUSH_DW>(++cmdFound, cmdList.end());

        // wait for NDR
        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdList.end());

        // NonAux to Aux
        cmdFound = expectCommand<XY_COPY_BLT>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<MI_FLUSH_DW>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<XY_COPY_BLT>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<MI_FLUSH_DW>(++cmdFound, cmdList.end());

        // taskCount
        expectCommand<MI_FLUSH_DW>(++cmdFound, cmdList.end());
    }
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitAuxTranslationWhenConstructingBlockedCommandBufferThenEnsureCorrectOrder) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    auto buffer0 = createBuffer(1, true);
    auto buffer1 = createBuffer(1, false);
    auto buffer2 = createBuffer(1, true);
    setMockKernelArgs(std::array<Buffer *, 3>{{buffer0.get(), buffer1.get(), buffer2.get()}});

    auto mockCmdQ = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    auto initialBcsTaskCount = mockCmdQ->bcsTaskCount;

    UserEvent userEvent;
    cl_event waitlist[] = {&userEvent};

    mockCmdQ->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, lws, 1, waitlist, nullptr);
    userEvent.setStatus(CL_COMPLETE);

    EXPECT_EQ(mockCmdQ->bcsTaskCount, initialBcsTaskCount + 1);

    // Gpgpu command buffer
    {
        auto cmdListCsr = getCmdList<FamilyType>(gpgpuCsr->getCS(0));
        auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr);
        auto cmdListQueue = getCmdList<FamilyType>(*ultCsr->lastFlushedCommandStream);

        // Barrier
        expectPipeControl<FamilyType>(cmdListCsr.begin(), cmdListCsr.end());

        // Aux to NonAux
        auto cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(cmdListQueue.begin(), cmdListQueue.end());
        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());
        // Walker
        cmdFound = expectCommand<WALKER_TYPE>(++cmdFound, cmdListQueue.end());
        cmdFound = expectCommand<WALKER_TYPE>(++cmdFound, cmdListQueue.end());
        // NonAux to Aux
        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());
        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());

        // task count
        expectPipeControl<FamilyType>(++cmdFound, cmdListQueue.end());
    }

    // BCS command buffer
    {
        auto cmdList = getCmdList<FamilyType>(bcsCsr->getCS(0));

        // Barrier
        auto cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(cmdList.begin(), cmdList.end());

        // Aux to NonAux
        cmdFound = expectCommand<XY_COPY_BLT>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<MI_FLUSH_DW>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<XY_COPY_BLT>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<MI_FLUSH_DW>(++cmdFound, cmdList.end());

        // wait for NDR
        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdList.end());

        // NonAux to Aux
        cmdFound = expectCommand<XY_COPY_BLT>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<MI_FLUSH_DW>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<XY_COPY_BLT>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<MI_FLUSH_DW>(++cmdFound, cmdList.end());

        // taskCount
        expectCommand<MI_FLUSH_DW>(++cmdFound, cmdList.end());
    }
    EXPECT_FALSE(mockCmdQ->isQueueBlocked());
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitTranslationWhenConstructingCommandBufferThenSynchronizeBarrier) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto buffer = createBuffer(1, true);
    setMockKernelArgs(std::array<Buffer *, 1>{{buffer.get()}});

    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    auto cmdListCsr = getCmdList<FamilyType>(gpgpuCsr->getCS(0));
    auto pipeControl = expectPipeControl<FamilyType>(cmdListCsr.begin(), cmdListCsr.end());
    auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*pipeControl);

    uint64_t low = pipeControlCmd->getAddress();
    uint64_t high = pipeControlCmd->getAddressHigh();
    uint64_t barrierGpuAddress = (high << 32) | low;

    auto cmdList = getCmdList<FamilyType>(bcsCsr->getCS(0));
    auto semaphore = expectCommand<MI_SEMAPHORE_WAIT>(cmdList.begin(), cmdList.end());
    verifySemaphore<FamilyType>(semaphore, barrierGpuAddress);
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitTranslationWhenConstructingCommandBufferThenSynchronizeBcsOutput) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    auto buffer0 = createBuffer(1, true);
    auto buffer1 = createBuffer(1, true);
    setMockKernelArgs(std::array<Buffer *, 2>{{buffer0.get(), buffer1.get()}});

    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    uint64_t auxToNonAuxOutputAddress[2] = {};
    uint64_t nonAuxToAuxOutputAddress[2] = {};
    {
        auto cmdListBcs = getCmdList<FamilyType>(bcsCsr->getCS(0));

        auto cmdFound = expectCommand<XY_COPY_BLT>(cmdListBcs.begin(), cmdListBcs.end());

        cmdFound = expectMiFlush<MI_FLUSH_DW>(++cmdFound, cmdListBcs.end());
        auto miflushDwCmd = genCmdCast<MI_FLUSH_DW *>(*cmdFound);
        auxToNonAuxOutputAddress[0] = miflushDwCmd->getDestinationAddress();

        cmdFound = expectMiFlush<MI_FLUSH_DW>(++cmdFound, cmdListBcs.end());
        miflushDwCmd = genCmdCast<MI_FLUSH_DW *>(*cmdFound);
        auxToNonAuxOutputAddress[1] = miflushDwCmd->getDestinationAddress();

        cmdFound = expectCommand<XY_COPY_BLT>(++cmdFound, cmdListBcs.end());

        cmdFound = expectMiFlush<MI_FLUSH_DW>(++cmdFound, cmdListBcs.end());
        miflushDwCmd = genCmdCast<MI_FLUSH_DW *>(*cmdFound);
        nonAuxToAuxOutputAddress[0] = miflushDwCmd->getDestinationAddress();

        cmdFound = expectMiFlush<MI_FLUSH_DW>(++cmdFound, cmdListBcs.end());
        miflushDwCmd = genCmdCast<MI_FLUSH_DW *>(*cmdFound);
        nonAuxToAuxOutputAddress[1] = miflushDwCmd->getDestinationAddress();
    }

    {
        auto cmdListQueue = getCmdList<FamilyType>(commandQueue->getCS(0));

        // Aux to NonAux
        auto cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(cmdListQueue.begin(), cmdListQueue.end());
        verifySemaphore<FamilyType>(cmdFound, auxToNonAuxOutputAddress[0]);

        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());
        verifySemaphore<FamilyType>(cmdFound, auxToNonAuxOutputAddress[1]);

        // Walker
        cmdFound = expectCommand<WALKER_TYPE>(++cmdFound, cmdListQueue.end());

        // NonAux to Aux
        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());
        verifySemaphore<FamilyType>(cmdFound, nonAuxToAuxOutputAddress[0]);

        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());
        verifySemaphore<FamilyType>(cmdFound, nonAuxToAuxOutputAddress[1]);
    }
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitTranslationWhenConstructingCommandBufferThenSynchronizeKernel) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto buffer = createBuffer(1, true);
    setMockKernelArgs(std::array<Buffer *, 1>{{buffer.get()}});

    auto mockCmdQ = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    mockCmdQ->overrideIsCacheFlushForBcsRequired.enabled = true;
    mockCmdQ->overrideIsCacheFlushForBcsRequired.returnValue = false;

    mockCmdQ->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    auto kernelNode = mockCmdQ->timestampPacketContainer->peekNodes()[0];
    auto kernelNodeAddress = kernelNode->getGpuAddress() + offsetof(TimestampPacketStorage, packets[0].contextEnd);

    auto cmdList = getCmdList<FamilyType>(bcsCsr->getCS(0));

    // Aux to nonAux
    auto cmdFound = expectCommand<XY_COPY_BLT>(cmdList.begin(), cmdList.end());

    // semaphore before NonAux to Aux
    auto semaphore = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdList.end());
    verifySemaphore<FamilyType>(semaphore, kernelNodeAddress);
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitTranslationWhenConstructingCommandBufferThenSynchronizeCacheFlush) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    auto buffer = createBuffer(1, true);
    setMockKernelArgs(std::array<Buffer *, 1>{{buffer.get()}});

    auto mockCmdQ = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    mockCmdQ->overrideIsCacheFlushForBcsRequired.enabled = true;
    mockCmdQ->overrideIsCacheFlushForBcsRequired.returnValue = true;
    mockCmdQ->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    auto cmdListBcs = getCmdList<FamilyType>(bcsCsr->getCS(0));
    auto cmdListQueue = getCmdList<FamilyType>(mockCmdQ->getCS(0));

    uint64_t cacheFlushWriteAddress = 0;

    {
        auto cmdFound = expectCommand<WALKER_TYPE>(cmdListQueue.begin(), cmdListQueue.end());
        cmdFound = expectPipeControl<FamilyType>(++cmdFound, cmdListQueue.end());

        auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*cmdFound);
        if (!pipeControlCmd->getDcFlushEnable()) {
            // skip pipe control with TimestampPacket write
            cmdFound = expectPipeControl<FamilyType>(++cmdFound, cmdListQueue.end());
            pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*cmdFound);
        }

        EXPECT_TRUE(pipeControlCmd->getDcFlushEnable());
        EXPECT_TRUE(pipeControlCmd->getCommandStreamerStallEnable());
        uint64_t low = pipeControlCmd->getAddress();
        uint64_t high = pipeControlCmd->getAddressHigh();
        cacheFlushWriteAddress = (high << 32) | low;
        EXPECT_NE(0u, cacheFlushWriteAddress);
    }

    {
        // Aux to nonAux
        auto cmdFound = expectCommand<XY_COPY_BLT>(cmdListBcs.begin(), cmdListBcs.end());

        // semaphore before NonAux to Aux
        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListBcs.end());
        verifySemaphore<FamilyType>(cmdFound, cacheFlushWriteAddress);
    }
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitTranslationWhenConstructingCommandBufferThenSynchronizeEvents) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto buffer = createBuffer(1, true);
    setMockKernelArgs(std::array<Buffer *, 1>{{buffer.get()}});

    auto event = make_releaseable<Event>(commandQueue.get(), CL_COMMAND_READ_BUFFER, 0, 0);
    MockTimestampPacketContainer eventDependencyContainer(*bcsCsr->getTimestampPacketAllocator(), 1);
    auto eventDependency = eventDependencyContainer.getNode(0);
    event->addTimestampPacketNodes(eventDependencyContainer);
    cl_event clEvent[] = {event.get()};

    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 1, clEvent, nullptr);

    auto eventDependencyAddress = eventDependency->getGpuAddress() + offsetof(TimestampPacketStorage, packets[0].contextEnd);

    auto cmdList = getCmdList<FamilyType>(bcsCsr->getCS(0));

    // Barrier
    auto cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(cmdList.begin(), cmdList.end());

    // Event
    auto semaphore = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdList.end());
    verifySemaphore<FamilyType>(semaphore, eventDependencyAddress);

    cmdFound = expectCommand<XY_COPY_BLT>(++semaphore, cmdList.end());
    expectCommand<XY_COPY_BLT>(++cmdFound, cmdList.end());
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenOutEventWhenDispatchingThenAssignNonAuxNodes) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto buffer0 = createBuffer(1, true);
    auto buffer1 = createBuffer(1, false);
    auto buffer2 = createBuffer(1, true);

    setMockKernelArgs(std::array<Buffer *, 3>{{buffer0.get(), buffer1.get(), buffer2.get()}});

    cl_event clEvent;
    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &clEvent);
    auto event = castToObject<Event>(clEvent);
    auto &eventNodes = event->getTimestampPacketNodes()->peekNodes();
    EXPECT_EQ(3u, eventNodes.size());

    auto cmdListQueue = getCmdList<FamilyType>(commandQueue->getCS(0));

    auto cmdFound = expectCommand<WALKER_TYPE>(cmdListQueue.begin(), cmdListQueue.end());

    // NonAux to Aux
    cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());
    auto eventNodeAddress = eventNodes[1]->getGpuAddress() + offsetof(TimestampPacketStorage, packets[0].contextEnd);
    verifySemaphore<FamilyType>(cmdFound, eventNodeAddress);

    cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());
    eventNodeAddress = eventNodes[2]->getGpuAddress() + offsetof(TimestampPacketStorage, packets[0].contextEnd);
    verifySemaphore<FamilyType>(cmdFound, eventNodeAddress);

    clReleaseEvent(clEvent);
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitAuxTranslationWhenDispatchingThenEstimateCmdBufferSize) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto &hwInfo = device->getHardwareInfo();
    auto mockCmdQ = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    mockCmdQ->overrideIsCacheFlushForBcsRequired.enabled = true;
    mockCmdQ->overrideIsCacheFlushForBcsRequired.returnValue = false;

    auto buffer0 = createBuffer(1, true);
    auto buffer1 = createBuffer(1, false);
    auto buffer2 = createBuffer(1, true);

    MemObjsForAuxTranslation memObjects;
    memObjects.insert(buffer0.get());
    memObjects.insert(buffer2.get());

    size_t numBuffersToEstimate = 2;
    size_t dependencySize = numBuffersToEstimate * TimestampPacketHelper::getRequiredCmdStreamSizeForNodeDependencyWithBlitEnqueue<FamilyType>();

    setMockKernelArgs(std::array<Buffer *, 3>{{buffer0.get(), buffer1.get(), buffer2.get()}});

    mockCmdQ->storeMultiDispatchInfo = true;
    mockCmdQ->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);

    MultiDispatchInfo &multiDispatchInfo = mockCmdQ->storedMultiDispatchInfo;
    DispatchInfo *firstDispatchInfo = multiDispatchInfo.begin();
    DispatchInfo *lastDispatchInfo = &(*multiDispatchInfo.rbegin());

    EXPECT_NE(firstDispatchInfo, lastDispatchInfo); // walker split

    EXPECT_EQ(dependencySize, firstDispatchInfo->dispatchInitCommands.estimateCommandsSize(memObjects.size(), hwInfo, mockCmdQ->isCacheFlushForBcsRequired()));
    EXPECT_EQ(0u, firstDispatchInfo->dispatchEpilogueCommands.estimateCommandsSize(memObjects.size(), hwInfo, mockCmdQ->isCacheFlushForBcsRequired()));

    EXPECT_EQ(0u, lastDispatchInfo->dispatchInitCommands.estimateCommandsSize(memObjects.size(), hwInfo, mockCmdQ->isCacheFlushForBcsRequired()));
    EXPECT_EQ(dependencySize, lastDispatchInfo->dispatchEpilogueCommands.estimateCommandsSize(memObjects.size(), hwInfo, mockCmdQ->isCacheFlushForBcsRequired()));
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitAuxTranslationWithRequiredCacheFlushWhenDispatchingThenEstimateCmdBufferSize) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto &hwInfo = device->getHardwareInfo();
    auto mockCmdQ = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    mockCmdQ->overrideIsCacheFlushForBcsRequired.enabled = true;
    mockCmdQ->overrideIsCacheFlushForBcsRequired.returnValue = true;

    auto buffer0 = createBuffer(1, true);
    auto buffer1 = createBuffer(1, false);
    auto buffer2 = createBuffer(1, true);

    MemObjsForAuxTranslation memObjects;
    memObjects.insert(buffer0.get());
    memObjects.insert(buffer2.get());

    size_t numBuffersToEstimate = 2;
    size_t dependencySize = numBuffersToEstimate * TimestampPacketHelper::getRequiredCmdStreamSizeForNodeDependencyWithBlitEnqueue<FamilyType>();

    size_t cacheFlushSize = MemorySynchronizationCommands<FamilyType>::getSizeForPipeControlWithPostSyncOperation(hwInfo);

    setMockKernelArgs(std::array<Buffer *, 3>{{buffer0.get(), buffer1.get(), buffer2.get()}});

    mockCmdQ->storeMultiDispatchInfo = true;
    mockCmdQ->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);

    MultiDispatchInfo &multiDispatchInfo = mockCmdQ->storedMultiDispatchInfo;
    DispatchInfo *firstDispatchInfo = multiDispatchInfo.begin();
    DispatchInfo *lastDispatchInfo = &(*multiDispatchInfo.rbegin());

    EXPECT_NE(firstDispatchInfo, lastDispatchInfo); // walker split

    EXPECT_EQ(dependencySize, firstDispatchInfo->dispatchInitCommands.estimateCommandsSize(memObjects.size(), hwInfo, mockCmdQ->isCacheFlushForBcsRequired()));
    EXPECT_EQ(0u, firstDispatchInfo->dispatchEpilogueCommands.estimateCommandsSize(memObjects.size(), hwInfo, mockCmdQ->isCacheFlushForBcsRequired()));

    EXPECT_EQ(0u, lastDispatchInfo->dispatchInitCommands.estimateCommandsSize(memObjects.size(), hwInfo, mockCmdQ->isCacheFlushForBcsRequired()));
    EXPECT_EQ(dependencySize + cacheFlushSize, lastDispatchInfo->dispatchEpilogueCommands.estimateCommandsSize(memObjects.size(), hwInfo, mockCmdQ->isCacheFlushForBcsRequired()));
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitTranslationWhenConstructingBlockedCommandBufferThenSynchronizeBarrier) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto buffer = createBuffer(1, true);
    setMockKernelArgs(std::array<Buffer *, 1>{{buffer.get()}});

    UserEvent userEvent;
    cl_event waitlist[] = {&userEvent};

    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 1, waitlist, nullptr);
    userEvent.setStatus(CL_COMPLETE);

    auto cmdListCsr = getCmdList<FamilyType>(gpgpuCsr->getCS(0));
    auto pipeControl = expectPipeControl<FamilyType>(cmdListCsr.begin(), cmdListCsr.end());
    auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*pipeControl);

    uint64_t low = pipeControlCmd->getAddress();
    uint64_t high = pipeControlCmd->getAddressHigh();
    uint64_t barrierGpuAddress = (high << 32) | low;

    auto cmdList = getCmdList<FamilyType>(bcsCsr->getCS(0));
    auto semaphore = expectCommand<MI_SEMAPHORE_WAIT>(cmdList.begin(), cmdList.end());
    verifySemaphore<FamilyType>(semaphore, barrierGpuAddress);

    EXPECT_FALSE(commandQueue->isQueueBlocked());
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitTranslationWhenConstructingBlockedCommandBufferThenSynchronizeEvents) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto buffer = createBuffer(1, true);
    setMockKernelArgs(std::array<Buffer *, 1>{{buffer.get()}});

    auto event = make_releaseable<Event>(commandQueue.get(), CL_COMMAND_READ_BUFFER, 0, 0);
    MockTimestampPacketContainer eventDependencyContainer(*bcsCsr->getTimestampPacketAllocator(), 1);
    auto eventDependency = eventDependencyContainer.getNode(0);
    event->addTimestampPacketNodes(eventDependencyContainer);

    UserEvent userEvent;
    cl_event waitlist[] = {&userEvent, event.get()};
    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 2, waitlist, nullptr);
    userEvent.setStatus(CL_COMPLETE);

    auto eventDependencyAddress = eventDependency->getGpuAddress() + offsetof(TimestampPacketStorage, packets[0].contextEnd);

    auto cmdList = getCmdList<FamilyType>(bcsCsr->getCS(0));

    // Barrier
    auto cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(cmdList.begin(), cmdList.end());

    // Event
    auto semaphore = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdList.end());
    verifySemaphore<FamilyType>(semaphore, eventDependencyAddress);

    cmdFound = expectCommand<XY_COPY_BLT>(++semaphore, cmdList.end());
    expectCommand<XY_COPY_BLT>(++cmdFound, cmdList.end());

    EXPECT_FALSE(commandQueue->isQueueBlocked());
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitTranslationWhenConstructingBlockedCommandBufferThenSynchronizeKernel) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto buffer = createBuffer(1, true);
    setMockKernelArgs(std::array<Buffer *, 1>{{buffer.get()}});

    auto mockCmdQ = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    UserEvent userEvent;
    cl_event waitlist[] = {&userEvent};

    mockCmdQ->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 1, waitlist, nullptr);
    userEvent.setStatus(CL_COMPLETE);

    auto kernelNode = mockCmdQ->timestampPacketContainer->peekNodes()[0];
    auto kernelNodeAddress = kernelNode->getGpuAddress() + offsetof(TimestampPacketStorage, packets[0].contextEnd);

    auto cmdList = getCmdList<FamilyType>(bcsCsr->getCS(0));

    // Aux to nonAux
    auto cmdFound = expectCommand<XY_COPY_BLT>(cmdList.begin(), cmdList.end());

    // semaphore before NonAux to Aux
    auto semaphore = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdList.end());
    if (mockCmdQ->isCacheFlushForBcsRequired()) {
        semaphore = expectCommand<MI_SEMAPHORE_WAIT>(++semaphore, cmdList.end());
    }
    verifySemaphore<FamilyType>(semaphore, kernelNodeAddress);

    EXPECT_FALSE(commandQueue->isQueueBlocked());
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitTranslationWhenConstructingBlockedCommandBufferThenSynchronizeBcsOutput) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    auto buffer0 = createBuffer(1, true);
    auto buffer1 = createBuffer(1, true);
    setMockKernelArgs(std::array<Buffer *, 2>{{buffer0.get(), buffer1.get()}});

    UserEvent userEvent;
    cl_event waitlist[] = {&userEvent};
    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 1, waitlist, nullptr);
    userEvent.setStatus(CL_COMPLETE);

    uint64_t auxToNonAuxOutputAddress[2] = {};
    uint64_t nonAuxToAuxOutputAddress[2] = {};
    {
        auto cmdListBcs = getCmdList<FamilyType>(bcsCsr->getCS(0));

        auto cmdFound = expectCommand<XY_COPY_BLT>(cmdListBcs.begin(), cmdListBcs.end());

        cmdFound = expectMiFlush<MI_FLUSH_DW>(++cmdFound, cmdListBcs.end());
        auto miflushDwCmd = genCmdCast<MI_FLUSH_DW *>(*cmdFound);
        auxToNonAuxOutputAddress[0] = miflushDwCmd->getDestinationAddress();

        cmdFound = expectMiFlush<MI_FLUSH_DW>(++cmdFound, cmdListBcs.end());
        miflushDwCmd = genCmdCast<MI_FLUSH_DW *>(*cmdFound);
        auxToNonAuxOutputAddress[1] = miflushDwCmd->getDestinationAddress();

        cmdFound = expectCommand<XY_COPY_BLT>(++cmdFound, cmdListBcs.end());

        cmdFound = expectMiFlush<MI_FLUSH_DW>(++cmdFound, cmdListBcs.end());
        miflushDwCmd = genCmdCast<MI_FLUSH_DW *>(*cmdFound);
        nonAuxToAuxOutputAddress[0] = miflushDwCmd->getDestinationAddress();

        cmdFound = expectMiFlush<MI_FLUSH_DW>(++cmdFound, cmdListBcs.end());
        miflushDwCmd = genCmdCast<MI_FLUSH_DW *>(*cmdFound);
        nonAuxToAuxOutputAddress[1] = miflushDwCmd->getDestinationAddress();
    }

    {
        auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr);
        auto cmdListQueue = getCmdList<FamilyType>(*ultCsr->lastFlushedCommandStream);

        // Aux to NonAux
        auto cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(cmdListQueue.begin(), cmdListQueue.end());
        verifySemaphore<FamilyType>(cmdFound, auxToNonAuxOutputAddress[0]);

        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());
        verifySemaphore<FamilyType>(cmdFound, auxToNonAuxOutputAddress[1]);

        // Walker
        cmdFound = expectCommand<WALKER_TYPE>(++cmdFound, cmdListQueue.end());

        // NonAux to Aux
        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());
        verifySemaphore<FamilyType>(cmdFound, nonAuxToAuxOutputAddress[0]);

        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());
        verifySemaphore<FamilyType>(cmdFound, nonAuxToAuxOutputAddress[1]);
    }

    EXPECT_FALSE(commandQueue->isQueueBlocked());
}
