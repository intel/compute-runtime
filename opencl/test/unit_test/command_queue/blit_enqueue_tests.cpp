/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/source/helpers/pause_on_gpu_properties.h"
#include "shared/source/helpers/vec.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/unit_test/compiler_interface/linker_mock.h"
#include "shared/test/unit_test/utilities/base_object_utils.h"

#include "opencl/source/event/user_event.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/mocks/mock_timestamp_container.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"
#include "test.h"

namespace NEO {

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];

template <int timestampPacketEnabled>
struct BlitEnqueueTests : public ::testing::Test {
    class BcsMockContext : public MockContext {
      public:
        BcsMockContext(ClDevice *device) : MockContext(device) {
            bcsOsContext.reset(OsContext::create(nullptr, 0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular})));
            bcsCsr.reset(createCommandStream(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield()));
            bcsCsr->setupContext(*bcsOsContext);
            bcsCsr->initializeTagAllocation();

            auto mockBlitMemoryToAllocation = [this](const Device &device, GraphicsAllocation *memory, size_t offset, const void *hostPtr,
                                                     Vec3<size_t> size) -> BlitOperationResult {
                if (!device.getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported) {
                    return BlitOperationResult::Unsupported;
                }

                auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                      *bcsCsr, memory, nullptr,
                                                                                      hostPtr,
                                                                                      memory->getGpuAddress(), 0,
                                                                                      0, 0, size, 0, 0, 0, 0);

                BlitPropertiesContainer container;
                container.push_back(blitProperties);
                bcsCsr->blitBuffer(container, true, false, const_cast<Device &>(device));

                return BlitOperationResult::Success;
            };
            blitMemoryToAllocationFuncBackup = mockBlitMemoryToAllocation;
        }

        std::unique_ptr<OsContext> bcsOsContext;
        std::unique_ptr<CommandStreamReceiver> bcsCsr;
        VariableBackup<BlitHelperFunctions::BlitMemoryToAllocationFunc> blitMemoryToAllocationFuncBackup{
            &BlitHelperFunctions::blitMemoryToAllocation};
    };

    template <typename FamilyType>
    void SetUpT() {
        if (is32bit) {
            GTEST_SKIP();
        }
        REQUIRE_AUX_RESOLVES();

        DebugManager.flags.EnableTimestampPacket.set(timestampPacketEnabled);
        DebugManager.flags.EnableBlitterForEnqueueOperations.set(1);
        DebugManager.flags.ForceAuxTranslationMode.set(static_cast<int32_t>(AuxTranslationMode::Blit));
        DebugManager.flags.RenderCompressedBuffersEnabled.set(1);
        DebugManager.flags.ForceGpgpuSubmissionForBcsEnqueue.set(1);
        DebugManager.flags.CsrDispatchMode.set(static_cast<int32_t>(DispatchMode::ImmediateDispatch));
        DebugManager.flags.EnableLocalMemory.set(1);
        device = std::make_unique<MockClDevice>(MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
        auto &capabilityTable = device->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable;
        bool createBcsEngine = !capabilityTable.blitterOperationsSupported;
        capabilityTable.blitterOperationsSupported = true;

        if (createBcsEngine) {
            auto &engine = device->getEngine(getChosenEngineType(device->getHardwareInfo()), EngineUsage::LowPriority);
            bcsOsContext.reset(OsContext::create(nullptr, 1,
                                                 EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular}, device->getDeviceBitfield())));
            engine.osContext = bcsOsContext.get();
            engine.commandStreamReceiver->setupContext(*bcsOsContext);
        }
        bcsMockContext = std::make_unique<BcsMockContext>(device.get());
        auto mockCmdQueue = new MockCommandQueueHw<FamilyType>(bcsMockContext.get(), device.get(), nullptr);
        commandQueue.reset(mockCmdQueue);
        mockKernel = std::make_unique<MockKernelWithInternals>(*device, bcsMockContext.get());
        auto mockProgram = mockKernel->mockProgram;
        mockProgram->setAllowNonUniform(true);

        gpgpuCsr = mockCmdQueue->gpgpuEngine->commandStreamReceiver;
        bcsCsr = mockCmdQueue->bcsEngines[0]->commandStreamReceiver;
    }

    template <typename FamilyType>
    void TearDownT() {}

    template <size_t N>
    void setMockKernelArgs(std::array<Buffer *, N> buffers) {
        for (uint32_t i = 0; i < buffers.size(); i++) {
            mockKernel->kernelInfo.addArgBuffer(i, 0);
        }

        mockKernel->mockKernel->initialize();
        EXPECT_TRUE(mockKernel->mockKernel->auxTranslationRequired);

        for (uint32_t i = 0; i < buffers.size(); i++) {
            cl_mem clMem = buffers[i];
            mockKernel->mockKernel->setArgBuffer(i, sizeof(cl_mem *), &clMem);
        }
    }

    template <size_t N>
    void setMockKernelArgs(std::array<GraphicsAllocation *, N> allocs) {
        for (uint32_t i = 0; i < allocs.size(); i++) {
            mockKernel->kernelInfo.addArgBuffer(i, 0);
        }

        mockKernel->mockKernel->initialize();
        EXPECT_TRUE(mockKernel->mockKernel->auxTranslationRequired);

        for (uint32_t i = 0; i < allocs.size(); i++) {
            auto alloc = allocs[i];
            auto ptr = reinterpret_cast<void *>(alloc->getGpuAddressToPatch());
            mockKernel->mockKernel->setArgSvmAlloc(i, ptr, alloc);
        }
    }

    ReleaseableObjectPtr<Buffer> createBuffer(size_t size, bool compressed) {
        auto buffer = clUniquePtr<Buffer>(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, size, nullptr, retVal));
        if (compressed) {
            buffer->getGraphicsAllocation(device->getRootDeviceIndex())->setAllocationType(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
        } else {
            buffer->getGraphicsAllocation(device->getRootDeviceIndex())->setAllocationType(GraphicsAllocation::AllocationType::BUFFER);
        }
        return buffer;
    }

    std::unique_ptr<GraphicsAllocation> createGfxAllocation(size_t size, bool compressed) {
        auto alloc = std::unique_ptr<GraphicsAllocation>(new MockGraphicsAllocation(nullptr, size));
        if (compressed) {
            alloc->setAllocationType(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
        } else {
            alloc->setAllocationType(GraphicsAllocation::AllocationType::BUFFER);
        }
        return alloc;
    }

    template <typename Family>
    GenCmdList getCmdList(LinearStream &linearStream, size_t offset) {
        HardwareParse hwParser;
        hwParser.parseCommands<Family>(linearStream, offset);

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

using BlitAuxTranslationTests = BlitEnqueueTests<1>;

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
    auto initialBcsTaskCount = mockCmdQ->peekBcsTaskCount(bcsCsr->getOsContext().getEngineType());

    mockCmdQ->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);

    EXPECT_EQ(initialBcsTaskCount + 1, mockCmdQ->peekBcsTaskCount(bcsCsr->getOsContext().getEngineType()));

    // Gpgpu command buffer
    {
        auto cmdListCsr = getCmdList<FamilyType>(gpgpuCsr->getCS(0), 0);
        auto cmdListQueue = getCmdList<FamilyType>(commandQueue->getCS(0), 0);

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
        auto cmdList = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);

        // Barrier
        auto cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(cmdList.begin(), cmdList.end());

        // Aux to NonAux
        cmdFound = expectCommand<XY_COPY_BLT>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<MI_FLUSH_DW>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<XY_COPY_BLT>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<MI_FLUSH_DW>(++cmdFound, cmdList.end());

        // wait for NDR (walker split)
        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdList.end());
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
    auto initialBcsTaskCount = mockCmdQ->peekBcsTaskCount(bcsCsr->getOsContext().getEngineType());

    UserEvent userEvent;
    cl_event waitlist[] = {&userEvent};

    mockCmdQ->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, lws, 1, waitlist, nullptr);
    userEvent.setStatus(CL_COMPLETE);

    EXPECT_EQ(initialBcsTaskCount + 1, mockCmdQ->peekBcsTaskCount(bcsCsr->getOsContext().getEngineType()));

    // Gpgpu command buffer
    {
        auto cmdListCsr = getCmdList<FamilyType>(gpgpuCsr->getCS(0), 0);
        auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr);
        auto cmdListQueue = getCmdList<FamilyType>(*ultCsr->lastFlushedCommandStream, 0);

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
        auto cmdList = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);

        // Barrier
        auto cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(cmdList.begin(), cmdList.end());

        // Aux to NonAux
        cmdFound = expectCommand<XY_COPY_BLT>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<MI_FLUSH_DW>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<XY_COPY_BLT>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<MI_FLUSH_DW>(++cmdFound, cmdList.end());

        // wait for NDR (walker split)
        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdList.end());
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

    auto cmdListCsr = getCmdList<FamilyType>(gpgpuCsr->getCS(0), 0);
    auto pipeControl = expectPipeControl<FamilyType>(cmdListCsr.begin(), cmdListCsr.end());
    auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*pipeControl);

    uint64_t low = pipeControlCmd->getAddress();
    uint64_t high = pipeControlCmd->getAddressHigh();
    uint64_t barrierGpuAddress = (high << 32) | low;

    auto cmdList = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);
    auto semaphore = expectCommand<MI_SEMAPHORE_WAIT>(cmdList.begin(), cmdList.end());
    verifySemaphore<FamilyType>(semaphore, barrierGpuAddress);
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, whenFlushTagUpdateThenMiFlushDwIsFlushed) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    bcsCsr->flushTagUpdate();

    auto cmdListBcs = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);

    auto cmdFound = expectCommand<MI_FLUSH_DW>(cmdListBcs.begin(), cmdListBcs.end());
    EXPECT_NE(cmdFound, cmdListBcs.end());
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
        auto cmdListBcs = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);

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
        auto cmdListQueue = getCmdList<FamilyType>(commandQueue->getCS(0), 0);

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
    auto kernelNodeAddress = TimestampPacketHelper::getContextEndGpuAddress(*kernelNode);

    auto cmdList = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);

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

    auto cmdListBcs = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);
    auto cmdListQueue = getCmdList<FamilyType>(mockCmdQ->getCS(0), 0);

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

    auto eventDependencyAddress = TimestampPacketHelper::getContextEndGpuAddress(*eventDependency);

    auto cmdList = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);

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
    EXPECT_EQ(5u, eventNodes.size());

    auto cmdListQueue = getCmdList<FamilyType>(commandQueue->getCS(0), 0);

    auto cmdFound = expectCommand<WALKER_TYPE>(cmdListQueue.begin(), cmdListQueue.end());

    // NonAux to Aux
    cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());
    auto eventNodeAddress = TimestampPacketHelper::getContextEndGpuAddress(*eventNodes[1]);
    verifySemaphore<FamilyType>(cmdFound, eventNodeAddress);

    cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());
    eventNodeAddress = TimestampPacketHelper::getContextEndGpuAddress(*eventNodes[2]);
    verifySemaphore<FamilyType>(cmdFound, eventNodeAddress);

    EXPECT_NE(0u, event->peekBcsTaskCountFromCommandQueue());

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

    KernelObjsForAuxTranslation kernelObjects;
    kernelObjects.insert({KernelObjForAuxTranslation::Type::MEM_OBJ, buffer0.get()});
    kernelObjects.insert({KernelObjForAuxTranslation::Type::MEM_OBJ, buffer2.get()});

    size_t numBuffersToEstimate = 2;
    size_t dependencySize = numBuffersToEstimate * TimestampPacketHelper::getRequiredCmdStreamSizeForNodeDependencyWithBlitEnqueue<FamilyType>();

    setMockKernelArgs(std::array<Buffer *, 3>{{buffer0.get(), buffer1.get(), buffer2.get()}});

    mockCmdQ->storeMultiDispatchInfo = true;
    mockCmdQ->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);

    MultiDispatchInfo &multiDispatchInfo = mockCmdQ->storedMultiDispatchInfo;
    DispatchInfo *firstDispatchInfo = multiDispatchInfo.begin();
    DispatchInfo *lastDispatchInfo = &(*multiDispatchInfo.rbegin());

    EXPECT_NE(firstDispatchInfo, lastDispatchInfo); // walker split

    EXPECT_EQ(dependencySize, firstDispatchInfo->dispatchInitCommands.estimateCommandsSize(kernelObjects.size(), hwInfo, mockCmdQ->isCacheFlushForBcsRequired()));
    EXPECT_EQ(0u, firstDispatchInfo->dispatchEpilogueCommands.estimateCommandsSize(kernelObjects.size(), hwInfo, mockCmdQ->isCacheFlushForBcsRequired()));

    EXPECT_EQ(0u, lastDispatchInfo->dispatchInitCommands.estimateCommandsSize(kernelObjects.size(), hwInfo, mockCmdQ->isCacheFlushForBcsRequired()));
    EXPECT_EQ(dependencySize, lastDispatchInfo->dispatchEpilogueCommands.estimateCommandsSize(kernelObjects.size(), hwInfo, mockCmdQ->isCacheFlushForBcsRequired()));
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

    KernelObjsForAuxTranslation kernelObjects;
    kernelObjects.insert({KernelObjForAuxTranslation::Type::MEM_OBJ, buffer0.get()});
    kernelObjects.insert({KernelObjForAuxTranslation::Type::MEM_OBJ, buffer2.get()});

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

    EXPECT_EQ(dependencySize, firstDispatchInfo->dispatchInitCommands.estimateCommandsSize(kernelObjects.size(), hwInfo, mockCmdQ->isCacheFlushForBcsRequired()));
    EXPECT_EQ(0u, firstDispatchInfo->dispatchEpilogueCommands.estimateCommandsSize(kernelObjects.size(), hwInfo, mockCmdQ->isCacheFlushForBcsRequired()));

    EXPECT_EQ(0u, lastDispatchInfo->dispatchInitCommands.estimateCommandsSize(kernelObjects.size(), hwInfo, mockCmdQ->isCacheFlushForBcsRequired()));
    EXPECT_EQ(dependencySize + cacheFlushSize, lastDispatchInfo->dispatchEpilogueCommands.estimateCommandsSize(kernelObjects.size(), hwInfo, mockCmdQ->isCacheFlushForBcsRequired()));
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

    auto cmdListCsr = getCmdList<FamilyType>(gpgpuCsr->getCS(0), 0);
    auto pipeControl = expectPipeControl<FamilyType>(cmdListCsr.begin(), cmdListCsr.end());
    auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*pipeControl);

    uint64_t low = pipeControlCmd->getAddress();
    uint64_t high = pipeControlCmd->getAddressHigh();
    uint64_t barrierGpuAddress = (high << 32) | low;

    auto cmdList = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);
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

    auto eventDependencyAddress = TimestampPacketHelper::getContextEndGpuAddress(*eventDependency);

    auto cmdList = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);

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
    auto kernelNodeAddress = TimestampPacketHelper::getContextEndGpuAddress(*kernelNode);

    auto cmdList = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);

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
        auto cmdListBcs = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);

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
        auto cmdListQueue = getCmdList<FamilyType>(*ultCsr->lastFlushedCommandStream, 0);

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

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitTranslationWhenEnqueueIsCalledThenDoImplicitFlushOnGpgpuCsr) {
    auto buffer = createBuffer(1, true);
    setMockKernelArgs(std::array<Buffer *, 1>{{buffer.get()}});

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr);

    EXPECT_EQ(0u, ultCsr->taskCount);

    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(1u, ultCsr->taskCount);
    EXPECT_TRUE(ultCsr->recordedDispatchFlags.implicitFlush);
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitTranslationOnGfxAllocationWhenEnqueueIsCalledThenDoImplicitFlushOnGpgpuCsr) {
    auto gfxAllocation = createGfxAllocation(1, true);
    setMockKernelArgs(std::array<GraphicsAllocation *, 1>{{gfxAllocation.get()}});

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr);

    EXPECT_EQ(0u, ultCsr->taskCount);

    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(1u, ultCsr->taskCount);
    EXPECT_TRUE(ultCsr->recordedDispatchFlags.implicitFlush);
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenCacheFlushRequiredWhenHandlingDependenciesThenPutAllNodesToDeferredList) {
    DebugManager.flags.ForceCacheFlushForBcs.set(1);

    auto gfxAllocation = createGfxAllocation(1, true);
    setMockKernelArgs(std::array<GraphicsAllocation *, 1>{{gfxAllocation.get()}});

    TimestampPacketContainer *deferredTimestampPackets = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get())->deferredTimestampPackets.get();

    EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());
    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(4u, deferredTimestampPackets->peekNodes().size()); // Barrier, CacheFlush, AuxToNonAux, NonAuxToAux
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenCacheFlushRequiredWhenHandlingDependenciesForBlockedEnqueueThenPutAllNodesToDeferredList) {
    DebugManager.flags.ForceCacheFlushForBcs.set(1);

    auto gfxAllocation = createGfxAllocation(1, true);
    setMockKernelArgs(std::array<GraphicsAllocation *, 1>{{gfxAllocation.get()}});

    TimestampPacketContainer *deferredTimestampPackets = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get())->deferredTimestampPackets.get();

    UserEvent userEvent;
    cl_event waitlist[] = {&userEvent};

    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 1, waitlist, nullptr);

    EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());

    userEvent.setStatus(CL_COMPLETE);

    EXPECT_FALSE(commandQueue->isQueueBlocked());

    EXPECT_EQ(4u, deferredTimestampPackets->peekNodes().size()); // Barrier, CacheFlush, AuxToNonAux, NonAuxToAux
}

using BlitEnqueueWithNoTimestampPacketTests = BlitEnqueueTests<0>;

HWTEST_TEMPLATED_F(BlitEnqueueWithNoTimestampPacketTests, givenNoTimestampPacketsWritewhenEnqueueingBlitOperationThenEnginesAreSynchronized) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    const size_t bufferSize = 1u;
    auto buffer = createBuffer(bufferSize, false);
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr);
    ASSERT_EQ(0u, ultCsr->taskCount);

    setMockKernelArgs(std::array<Buffer *, 1>{{buffer.get()}});
    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    char cpuBuffer[bufferSize]{};
    commandQueue->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, bufferSize, cpuBuffer, nullptr, 0, nullptr, nullptr);
    commandQueue->finish();

    auto bcsCommands = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);
    auto ccsCommands = getCmdList<FamilyType>(commandQueue->getCS(0), 0);

    auto cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(bcsCommands.begin(), bcsCommands.end());

    cmdFound = expectMiFlush<MI_FLUSH_DW>(cmdFound++, bcsCommands.end());
    auto miflushDwCmd = genCmdCast<MI_FLUSH_DW *>(*cmdFound);
    const auto bcsSignalAddress = miflushDwCmd->getDestinationAddress();

    cmdFound = expectCommand<WALKER_TYPE>(ccsCommands.begin(), ccsCommands.end());

    cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(cmdFound++, ccsCommands.end());
    verifySemaphore<FamilyType>(cmdFound, bcsSignalAddress);
}

struct BlitEnqueueWithDebugCapabilityTests : public BlitEnqueueTests<0> {
    template <typename MI_SEMAPHORE_WAIT>
    void findSemaphores(GenCmdList &cmdList) {
        auto semaphore = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

        while (semaphore != cmdList.end()) {
            auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphore);
            if (static_cast<uint32_t>(DebugPauseState::hasUserStartConfirmation) == semaphoreCmd->getSemaphoreDataDword() &&
                debugPauseStateAddress == semaphoreCmd->getSemaphoreGraphicsAddress()) {

                EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, semaphoreCmd->getCompareOperation());
                EXPECT_EQ(MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE, semaphoreCmd->getWaitMode());

                semaphoreBeforeCopyFound++;
            }

            if (static_cast<uint32_t>(DebugPauseState::hasUserEndConfirmation) == semaphoreCmd->getSemaphoreDataDword() &&
                debugPauseStateAddress == semaphoreCmd->getSemaphoreGraphicsAddress()) {

                EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, semaphoreCmd->getCompareOperation());
                EXPECT_EQ(MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE, semaphoreCmd->getWaitMode());

                semaphoreAfterCopyFound++;
            }

            semaphore = find<MI_SEMAPHORE_WAIT *>(++semaphore, cmdList.end());
        }
    }

    template <typename MI_FLUSH_DW>
    void findMiFlushes(GenCmdList &cmdList) {
        auto miFlush = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());

        while (miFlush != cmdList.end()) {
            auto miFlushCmd = genCmdCast<MI_FLUSH_DW *>(*miFlush);
            if (static_cast<uint32_t>(DebugPauseState::waitingForUserStartConfirmation) == miFlushCmd->getImmediateData() &&
                debugPauseStateAddress == miFlushCmd->getDestinationAddress()) {

                EXPECT_EQ(MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD, miFlushCmd->getPostSyncOperation());

                miFlushBeforeCopyFound++;
            }

            if (static_cast<uint32_t>(DebugPauseState::waitingForUserEndConfirmation) == miFlushCmd->getImmediateData() &&
                debugPauseStateAddress == miFlushCmd->getDestinationAddress()) {

                EXPECT_EQ(MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD, miFlushCmd->getPostSyncOperation());

                miFlushAfterCopyFound++;
            }

            miFlush = find<MI_FLUSH_DW *>(++miFlush, cmdList.end());
        }
    }

    uint32_t semaphoreBeforeCopyFound = 0;
    uint32_t semaphoreAfterCopyFound = 0;
    uint32_t miFlushBeforeCopyFound = 0;
    uint32_t miFlushAfterCopyFound = 0;

    ReleaseableObjectPtr<Buffer> buffer;
    uint64_t debugPauseStateAddress = 0;
    int hostPtr = 0;
};

HWTEST_TEMPLATED_F(BlitEnqueueWithDebugCapabilityTests, givenDebugFlagSetWhenDispatchingBlitEnqueueThenAddPausingCommands) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    auto ultBcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr);

    debugPauseStateAddress = ultBcsCsr->getDebugPauseStateGPUAddress();

    buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;

    DebugManager.flags.PauseOnBlitCopy.set(1);

    commandQueue->enqueueWriteBuffer(buffer.get(), true, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    commandQueue->enqueueWriteBuffer(buffer.get(), true, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(ultBcsCsr->commandStream);

    findSemaphores<MI_SEMAPHORE_WAIT>(hwParser.cmdList);

    EXPECT_EQ(1u, semaphoreBeforeCopyFound);
    EXPECT_EQ(1u, semaphoreAfterCopyFound);

    findMiFlushes<MI_FLUSH_DW>(hwParser.cmdList);

    EXPECT_EQ(1u, miFlushBeforeCopyFound);
    EXPECT_EQ(1u, miFlushAfterCopyFound);
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDebugCapabilityTests, givenDebugFlagSetToMinusTwoWhenDispatchingBlitEnqueueThenAddPausingCommandsForEachEnqueue) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    auto ultBcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr);

    debugPauseStateAddress = ultBcsCsr->getDebugPauseStateGPUAddress();

    buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;

    DebugManager.flags.PauseOnBlitCopy.set(-2);

    commandQueue->enqueueWriteBuffer(buffer.get(), true, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    commandQueue->enqueueWriteBuffer(buffer.get(), true, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(ultBcsCsr->commandStream);

    findSemaphores<MI_SEMAPHORE_WAIT>(hwParser.cmdList);

    EXPECT_EQ(2u, semaphoreBeforeCopyFound);
    EXPECT_EQ(2u, semaphoreAfterCopyFound);

    findMiFlushes<MI_FLUSH_DW>(hwParser.cmdList);

    EXPECT_EQ(2u, miFlushBeforeCopyFound);
    EXPECT_EQ(2u, miFlushAfterCopyFound);
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDebugCapabilityTests, givenPauseModeSetToBeforeOnlyWhenDispatchingBlitEnqueueThenAddPauseCommandsOnlyBeforeEnqueue) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    auto ultBcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr);

    debugPauseStateAddress = ultBcsCsr->getDebugPauseStateGPUAddress();

    buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;

    DebugManager.flags.PauseOnBlitCopy.set(0);
    DebugManager.flags.PauseOnGpuMode.set(PauseOnGpuProperties::PauseMode::BeforeWorkload);

    commandQueue->enqueueWriteBuffer(buffer.get(), true, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(ultBcsCsr->commandStream);

    findSemaphores<MI_SEMAPHORE_WAIT>(hwParser.cmdList);

    EXPECT_EQ(1u, semaphoreBeforeCopyFound);
    EXPECT_EQ(0u, semaphoreAfterCopyFound);

    findMiFlushes<MI_FLUSH_DW>(hwParser.cmdList);

    EXPECT_EQ(1u, miFlushBeforeCopyFound);
    EXPECT_EQ(0u, miFlushAfterCopyFound);
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDebugCapabilityTests, givenPauseModeSetToAfterOnlyWhenDispatchingBlitEnqueueThenAddPauseCommandsOnlyAfterEnqueue) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    auto ultBcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr);

    debugPauseStateAddress = ultBcsCsr->getDebugPauseStateGPUAddress();

    buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;

    DebugManager.flags.PauseOnBlitCopy.set(0);
    DebugManager.flags.PauseOnGpuMode.set(PauseOnGpuProperties::PauseMode::AfterWorkload);

    commandQueue->enqueueWriteBuffer(buffer.get(), true, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(ultBcsCsr->commandStream);

    findSemaphores<MI_SEMAPHORE_WAIT>(hwParser.cmdList);

    EXPECT_EQ(0u, semaphoreBeforeCopyFound);
    EXPECT_EQ(1u, semaphoreAfterCopyFound);

    findMiFlushes<MI_FLUSH_DW>(hwParser.cmdList);

    EXPECT_EQ(0u, miFlushBeforeCopyFound);
    EXPECT_EQ(1u, miFlushAfterCopyFound);
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDebugCapabilityTests, givenPauseModeSetToBeforeAndAfterWorkloadWhenDispatchingBlitEnqueueThenAddPauseCommandsAroundEnqueue) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    auto ultBcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr);

    debugPauseStateAddress = ultBcsCsr->getDebugPauseStateGPUAddress();

    buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;

    DebugManager.flags.PauseOnBlitCopy.set(0);
    DebugManager.flags.PauseOnGpuMode.set(PauseOnGpuProperties::PauseMode::BeforeAndAfterWorkload);

    commandQueue->enqueueWriteBuffer(buffer.get(), true, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(ultBcsCsr->commandStream);

    findSemaphores<MI_SEMAPHORE_WAIT>(hwParser.cmdList);

    EXPECT_EQ(1u, semaphoreBeforeCopyFound);
    EXPECT_EQ(1u, semaphoreAfterCopyFound);

    findMiFlushes<MI_FLUSH_DW>(hwParser.cmdList);

    EXPECT_EQ(1u, miFlushBeforeCopyFound);
    EXPECT_EQ(1u, miFlushAfterCopyFound);
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDebugCapabilityTests, givenDebugFlagSetWhenCreatingCsrThenCreateDebugThread) {
    DebugManager.flags.PauseOnBlitCopy.set(1);

    auto localDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(localDevice->getDefaultEngine().commandStreamReceiver);

    EXPECT_NE(nullptr, ultCsr->userPauseConfirmation.get());
}

struct BlitEnqueueFlushTests : public BlitEnqueueTests<1> {
    template <typename FamilyType>
    class MyUltCsr : public UltCommandStreamReceiver<FamilyType> {
      public:
        using UltCommandStreamReceiver<FamilyType>::UltCommandStreamReceiver;

        bool flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
            latestFlushedCounter = ++(*flushCounter);
            return UltCommandStreamReceiver<FamilyType>::flush(batchBuffer, allocationsForResidency);
        }

        static CommandStreamReceiver *create(bool withAubDump, ExecutionEnvironment &executionEnvironment,
                                             uint32_t rootDeviceIndex,
                                             const DeviceBitfield deviceBitfield) {
            return new MyUltCsr<FamilyType>(executionEnvironment, rootDeviceIndex, deviceBitfield);
        }

        uint32_t *flushCounter = nullptr;
        uint32_t latestFlushedCounter = 0;
    };

    template <typename T>
    void SetUpT() {
        auto csrCreateFcn = &commandStreamReceiverFactory[IGFX_MAX_CORE + defaultHwInfo->platform.eRenderCoreFamily];
        variableBackup = std::make_unique<VariableBackup<CommandStreamReceiverCreateFunc>>(csrCreateFcn);
        *csrCreateFcn = MyUltCsr<T>::create;

        BlitEnqueueTests<1>::SetUpT<T>();
    }

    std::unique_ptr<VariableBackup<CommandStreamReceiverCreateFunc>> variableBackup;
};

HWTEST_TEMPLATED_F(BlitEnqueueFlushTests, givenNonBlockedQueueWhenBlitEnqueuedThenFlushGpgpuCsrFirst) {
    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    uint32_t flushCounter = 0;

    auto myUltGpgpuCsr = static_cast<MyUltCsr<FamilyType> *>(gpgpuCsr);
    myUltGpgpuCsr->flushCounter = &flushCounter;
    auto myUltBcsCsr = static_cast<MyUltCsr<FamilyType> *>(bcsCsr);
    myUltBcsCsr->flushCounter = &flushCounter;

    commandQueue->enqueueWriteBuffer(buffer.get(), true, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(1u, myUltGpgpuCsr->latestFlushedCounter);
    EXPECT_EQ(2u, myUltBcsCsr->latestFlushedCounter);
}

HWTEST_TEMPLATED_F(BlitEnqueueFlushTests, givenBlockedQueueWhenBlitEnqueuedThenFlushGpgpuCsrFirst) {
    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    uint32_t flushCounter = 0;

    auto myUltGpgpuCsr = static_cast<MyUltCsr<FamilyType> *>(gpgpuCsr);
    myUltGpgpuCsr->flushCounter = &flushCounter;
    auto myUltBcsCsr = static_cast<MyUltCsr<FamilyType> *>(bcsCsr);
    myUltBcsCsr->flushCounter = &flushCounter;

    UserEvent userEvent;
    cl_event waitlist[] = {&userEvent};

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 1, waitlist, nullptr);
    userEvent.setStatus(CL_COMPLETE);

    EXPECT_EQ(1u, myUltGpgpuCsr->latestFlushedCounter);
    EXPECT_EQ(2u, myUltBcsCsr->latestFlushedCounter);

    EXPECT_FALSE(commandQueue->isQueueBlocked());
}

HWTEST_TEMPLATED_F(BlitEnqueueFlushTests, givenDebugFlagSetWhenCheckingBcsCacheFlushRequirementThenReturnCorrectValue) {
    auto mockCommandQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());

    DebugManager.flags.ForceCacheFlushForBcs.set(0);
    EXPECT_FALSE(mockCommandQueue->isCacheFlushForBcsRequired());

    DebugManager.flags.ForceCacheFlushForBcs.set(1);
    EXPECT_TRUE(mockCommandQueue->isCacheFlushForBcsRequired());
}

using BlitEnqueueTaskCountTests = BlitEnqueueTests<1>;

HWTEST_TEMPLATED_F(BlitEnqueueTaskCountTests, whenWaitUntilCompletionCalledThenWaitForSpecificBcsTaskCount) {
    uint32_t gpgpuTaskCount = 123;
    uint32_t bcsTaskCount = 123;

    CopyEngineState bcsState{bcsCsr->getOsContext().getEngineType(), bcsTaskCount};
    commandQueue->waitUntilComplete(gpgpuTaskCount, Range{&bcsState}, 0, false);

    EXPECT_EQ(gpgpuTaskCount, static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr)->latestWaitForCompletionWithTimeoutTaskCount.load());
    EXPECT_EQ(bcsTaskCount, static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr)->latestWaitForCompletionWithTimeoutTaskCount.load());
}

HWTEST_TEMPLATED_F(BlitEnqueueTaskCountTests, givenEventWithNotreadyBcsTaskCountThenDontReportCompletion) {
    const uint32_t gpgpuTaskCount = 123;
    const uint32_t bcsTaskCount = 123;

    *gpgpuCsr->getTagAddress() = gpgpuTaskCount;
    *bcsCsr->getTagAddress() = bcsTaskCount - 1;
    commandQueue->updateBcsTaskCount(bcsCsr->getOsContext().getEngineType(), bcsTaskCount);

    Event event(commandQueue.get(), CL_COMMAND_WRITE_BUFFER, 1, gpgpuTaskCount);
    event.setupBcs(bcsCsr->getOsContext().getEngineType());
    event.updateCompletionStamp(gpgpuTaskCount, bcsTaskCount, 1, 0);

    event.updateExecutionStatus();
    EXPECT_EQ(static_cast<cl_int>(CL_SUBMITTED), event.peekExecutionStatus());

    *bcsCsr->getTagAddress() = bcsTaskCount;
    event.updateExecutionStatus();
    EXPECT_EQ(static_cast<cl_int>(CL_COMPLETE), event.peekExecutionStatus());
}

HWTEST_TEMPLATED_F(BlitEnqueueTaskCountTests, givenEventWhenWaitingForCompletionThenWaitForCurrentBcsTaskCount) {
    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    auto ultGpgpuCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr);
    auto ultBcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr);

    cl_event outEvent1, outEvent2;
    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, &outEvent1);
    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, &outEvent2);

    clWaitForEvents(1, &outEvent2);
    EXPECT_EQ(2u, ultGpgpuCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
    EXPECT_EQ(2u, ultBcsCsr->latestWaitForCompletionWithTimeoutTaskCount.load());

    clWaitForEvents(1, &outEvent1);
    EXPECT_EQ(1u, ultGpgpuCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
    EXPECT_EQ(1u, ultBcsCsr->latestWaitForCompletionWithTimeoutTaskCount.load());

    clReleaseEvent(outEvent1);
    clReleaseEvent(outEvent2);
}

HWTEST_TEMPLATED_F(BlitEnqueueTaskCountTests, givenBufferDumpingEnabledWhenEnqueueingThenSetCorrectDumpOption) {
    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    DebugManager.flags.AUBDumpAllocsOnEnqueueReadOnly.set(true);
    DebugManager.flags.AUBDumpBufferFormat.set("BIN");

    auto mockCommandQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());

    {
        // BCS enqueue
        commandQueue->enqueueReadBuffer(buffer.get(), true, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);

        EXPECT_TRUE(mockCommandQueue->notifyEnqueueReadBufferCalled);
        EXPECT_TRUE(mockCommandQueue->useBcsCsrOnNotifyEnabled);

        mockCommandQueue->notifyEnqueueReadBufferCalled = false;
    }

    {
        // Non-BCS enqueue
        DebugManager.flags.EnableBlitterForEnqueueOperations.set(0);

        commandQueue->enqueueReadBuffer(buffer.get(), true, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);

        EXPECT_TRUE(mockCommandQueue->notifyEnqueueReadBufferCalled);
        EXPECT_FALSE(mockCommandQueue->useBcsCsrOnNotifyEnabled);
    }
}

HWTEST_TEMPLATED_F(BlitEnqueueTaskCountTests, givenBlockedEventWhenWaitingForCompletionThenWaitForCurrentBcsTaskCount) {
    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    auto ultGpgpuCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr);
    auto ultBcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr);

    cl_event outEvent1, outEvent2;
    UserEvent userEvent;
    cl_event waitlist1 = &userEvent;
    cl_event *waitlist2 = &outEvent1;

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 1, &waitlist1, &outEvent1);
    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 1, waitlist2, &outEvent2);

    userEvent.setStatus(CL_COMPLETE);

    clWaitForEvents(1, &outEvent2);
    EXPECT_EQ(2u, ultGpgpuCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
    EXPECT_EQ(2u, ultBcsCsr->latestWaitForCompletionWithTimeoutTaskCount.load());

    clWaitForEvents(1, &outEvent1);
    EXPECT_EQ(1u, ultGpgpuCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
    EXPECT_EQ(1u, ultBcsCsr->latestWaitForCompletionWithTimeoutTaskCount.load());

    clReleaseEvent(outEvent1);
    clReleaseEvent(outEvent2);

    EXPECT_FALSE(commandQueue->isQueueBlocked());
}

HWTEST_TEMPLATED_F(BlitEnqueueTaskCountTests, givenBlockedEnqueueWithoutKernelWhenWaitingForCompletionThenWaitForCurrentBcsTaskCount) {
    auto ultGpgpuCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr);
    auto ultBcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr);

    cl_event outEvent1, outEvent2;
    UserEvent userEvent;
    cl_event waitlist1 = &userEvent;
    cl_event *waitlist2 = &outEvent1;

    commandQueue->enqueueMarkerWithWaitList(1, &waitlist1, &outEvent1);
    commandQueue->enqueueMarkerWithWaitList(1, waitlist2, &outEvent2);

    userEvent.setStatus(CL_COMPLETE);

    clWaitForEvents(1, &outEvent2);
    EXPECT_EQ(1u, ultGpgpuCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
    EXPECT_EQ(0u, ultBcsCsr->latestWaitForCompletionWithTimeoutTaskCount.load());

    clWaitForEvents(1, &outEvent1);
    EXPECT_EQ(0u, ultGpgpuCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
    EXPECT_EQ(0u, ultBcsCsr->latestWaitForCompletionWithTimeoutTaskCount.load());

    clReleaseEvent(outEvent1);
    clReleaseEvent(outEvent2);

    EXPECT_FALSE(commandQueue->isQueueBlocked());
}

HWTEST_TEMPLATED_F(BlitEnqueueTaskCountTests, givenEventFromCpuCopyWhenWaitingForCompletionThenWaitForCurrentBcsTaskCount) {
    auto buffer = createBuffer(1, false);
    int hostPtr = 0;

    auto ultGpgpuCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr);
    auto ultBcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr);

    ultGpgpuCsr->taskCount = 1;
    commandQueue->taskCount = 1;

    ultBcsCsr->taskCount = 2;
    commandQueue->updateBcsTaskCount(ultBcsCsr->getOsContext().getEngineType(), 2);

    cl_event outEvent1, outEvent2;
    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, &outEvent1);
    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, &outEvent2);

    clWaitForEvents(1, &outEvent2);
    EXPECT_EQ(3u, static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr)->latestWaitForCompletionWithTimeoutTaskCount.load());
    EXPECT_EQ(4u, static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr)->latestWaitForCompletionWithTimeoutTaskCount.load());

    clWaitForEvents(1, &outEvent1);
    EXPECT_EQ(2u, static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr)->latestWaitForCompletionWithTimeoutTaskCount.load());
    EXPECT_EQ(3u, static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr)->latestWaitForCompletionWithTimeoutTaskCount.load());

    clReleaseEvent(outEvent1);
    clReleaseEvent(outEvent2);
}

using BlitEnqueueWithDisabledGpgpuSubmissionTests = BlitEnqueueTests<1>;

HWTEST_TEMPLATED_F(BlitEnqueueWithDisabledGpgpuSubmissionTests, givenCacheFlushRequiredWhenDoingBcsCopyThenSubmitToGpgpuOnlyIfPreviousEnqueueWasGpgpu) {
    auto mockCommandQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    EXPECT_EQ(EnqueueProperties::Operation::None, mockCommandQueue->latestSentEnqueueType);

    DebugManager.flags.ForceGpgpuSubmissionForBcsEnqueue.set(-1);

    mockCommandQueue->overrideIsCacheFlushForBcsRequired.enabled = true;
    mockCommandQueue->overrideIsCacheFlushForBcsRequired.returnValue = true;

    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(0u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(0u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::GpuKernel, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(1u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(2u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(2u, gpgpuCsr->peekTaskCount());
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDisabledGpgpuSubmissionTests, givenProfilingEnabledWhenSubmittingWithoutFlushToGpgpuThenSetSubmitTime) {
    auto mockCommandQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    EXPECT_EQ(EnqueueProperties::Operation::None, mockCommandQueue->latestSentEnqueueType);

    DebugManager.flags.ForceGpgpuSubmissionForBcsEnqueue.set(-1);

    mockCommandQueue->overrideIsCacheFlushForBcsRequired.enabled = true;
    mockCommandQueue->overrideIsCacheFlushForBcsRequired.returnValue = true;
    mockCommandQueue->setProfilingEnabled();

    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    cl_event clEvent;

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, &clEvent);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(0u, gpgpuCsr->peekTaskCount());

    auto event = castToObject<Event>(clEvent);

    uint64_t submitTime = 0;
    event->getEventProfilingInfo(CL_PROFILING_COMMAND_SUBMIT, sizeof(submitTime), &submitTime, nullptr);

    EXPECT_NE(0u, submitTime);

    clReleaseEvent(clEvent);
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDisabledGpgpuSubmissionTests, givenOutEventWhenEnqueuingBcsSubmissionThenSetupBcsCsrInEvent) {
    auto mockCommandQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    EXPECT_EQ(EnqueueProperties::Operation::None, mockCommandQueue->latestSentEnqueueType);

    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    {
        DebugManager.flags.EnableBlitterForEnqueueOperations.set(0);

        cl_event clEvent;
        commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, &clEvent);
        EXPECT_EQ(EnqueueProperties::Operation::GpuKernel, mockCommandQueue->latestSentEnqueueType);
        EXPECT_EQ(0u, bcsCsr->peekTaskCount());
        EXPECT_EQ(1u, gpgpuCsr->peekTaskCount());

        auto event = castToObject<Event>(clEvent);
        EXPECT_EQ(0u, event->peekBcsTaskCountFromCommandQueue());

        clReleaseEvent(clEvent);
    }
    {
        DebugManager.flags.EnableBlitterForEnqueueOperations.set(1);

        cl_event clEvent;
        commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, &clEvent);
        EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
        EXPECT_EQ(1u, bcsCsr->peekTaskCount());
        EXPECT_EQ(2u, gpgpuCsr->peekTaskCount());

        auto event = castToObject<Event>(clEvent);
        EXPECT_EQ(1u, event->peekBcsTaskCountFromCommandQueue());

        clReleaseEvent(clEvent);
    }
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDisabledGpgpuSubmissionTests, givenCacheFlushNotRequiredWhenDoingBcsCopyThenDontSubmitToGpgpu) {
    auto mockCommandQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    EXPECT_EQ(EnqueueProperties::Operation::None, mockCommandQueue->latestSentEnqueueType);

    DebugManager.flags.ForceGpgpuSubmissionForBcsEnqueue.set(-1);

    mockCommandQueue->overrideIsCacheFlushForBcsRequired.enabled = true;
    mockCommandQueue->overrideIsCacheFlushForBcsRequired.returnValue = false;

    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(0u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(0u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::GpuKernel, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(1u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(2u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(2u, gpgpuCsr->peekTaskCount());
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDisabledGpgpuSubmissionTests, givenCacheFlushNotRequiredWhenDoingBcsCopyAfterBarrierThenSubmitToGpgpu) {
    auto mockCommandQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    EXPECT_EQ(EnqueueProperties::Operation::None, mockCommandQueue->latestSentEnqueueType);

    DebugManager.flags.ForceGpgpuSubmissionForBcsEnqueue.set(-1);

    mockCommandQueue->overrideIsCacheFlushForBcsRequired.enabled = true;
    mockCommandQueue->overrideIsCacheFlushForBcsRequired.returnValue = false;

    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    EXPECT_EQ(0u, gpgpuCsr->peekTaskCount());
    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(1u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueBarrierWithWaitList(0, nullptr, nullptr);
    EXPECT_EQ(1u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(2u, gpgpuCsr->peekTaskCount());
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDisabledGpgpuSubmissionTests, givenCacheFlushNotRequiredWhenDoingBcsCopyOnBlockedQueueThenSubmitToGpgpu) {
    auto mockCommandQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    EXPECT_EQ(EnqueueProperties::Operation::None, mockCommandQueue->latestSentEnqueueType);

    DebugManager.flags.ForceGpgpuSubmissionForBcsEnqueue.set(-1);

    mockCommandQueue->overrideIsCacheFlushForBcsRequired.enabled = true;
    mockCommandQueue->overrideIsCacheFlushForBcsRequired.returnValue = false;

    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    UserEvent userEvent;
    cl_event waitlist = &userEvent;

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 1, &waitlist, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::None, mockCommandQueue->latestSentEnqueueType);

    userEvent.setStatus(CL_COMPLETE);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);

    EXPECT_EQ(1u, gpgpuCsr->peekTaskCount());

    EXPECT_FALSE(commandQueue->isQueueBlocked());
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDisabledGpgpuSubmissionTests, givenCacheFlushRequiredWhenDoingBcsCopyOnBlockedQueueThenSubmitToGpgpu) {
    auto mockCommandQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    EXPECT_EQ(EnqueueProperties::Operation::None, mockCommandQueue->latestSentEnqueueType);

    DebugManager.flags.ForceGpgpuSubmissionForBcsEnqueue.set(-1);

    mockCommandQueue->overrideIsCacheFlushForBcsRequired.enabled = true;
    mockCommandQueue->overrideIsCacheFlushForBcsRequired.returnValue = true;

    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    UserEvent userEvent;
    cl_event waitlist = &userEvent;

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 1, &waitlist, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::None, mockCommandQueue->latestSentEnqueueType);

    userEvent.setStatus(CL_COMPLETE);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);

    EXPECT_EQ(1u, gpgpuCsr->peekTaskCount());

    EXPECT_FALSE(commandQueue->isQueueBlocked());
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDisabledGpgpuSubmissionTests, givenCacheFlushRequiredWhenDoingBcsCopyThatRequiresCacheFlushThenSubmitToGpgpu) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    DebugManager.flags.ForceGpgpuSubmissionForBcsEnqueue.set(-1);

    auto mockCommandQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    mockCommandQueue->overrideIsCacheFlushForBcsRequired.enabled = true;
    mockCommandQueue->overrideIsCacheFlushForBcsRequired.returnValue = true;

    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    // enqueue kernel to force gpgpu submission on write buffer
    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(1u, gpgpuCsr->peekTaskCount());

    auto offset = mockCommandQueue->getCS(0).getUsed();

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(2u, gpgpuCsr->peekTaskCount());

    auto cmdListBcs = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);
    auto cmdListQueue = getCmdList<FamilyType>(mockCommandQueue->getCS(0), offset);

    uint64_t cacheFlushWriteAddress = 0;

    {
        auto cmdFound = expectPipeControl<FamilyType>(cmdListQueue.begin(), cmdListQueue.end());
        auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*cmdFound);

        EXPECT_TRUE(pipeControlCmd->getDcFlushEnable());
        EXPECT_TRUE(pipeControlCmd->getCommandStreamerStallEnable());
        uint64_t low = pipeControlCmd->getAddress();
        uint64_t high = pipeControlCmd->getAddressHigh();
        cacheFlushWriteAddress = (high << 32) | low;
        EXPECT_NE(0u, cacheFlushWriteAddress);
    }

    {
        auto cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(cmdListBcs.begin(), cmdListBcs.end());
        verifySemaphore<FamilyType>(cmdFound, cacheFlushWriteAddress);

        cmdFound = expectCommand<XY_COPY_BLT>(cmdListBcs.begin(), cmdListBcs.end());
        EXPECT_NE(cmdListBcs.end(), cmdFound);
    }
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDisabledGpgpuSubmissionTests, givenSubmissionToDifferentEngineWhenRequestingForNewTimestmapPacketThenDontClearDependencies) {
    auto mockCommandQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    const bool clearDependencies = true;

    {
        TimestampPacketContainer previousNodes;
        mockCommandQueue->obtainNewTimestampPacketNodes(1, previousNodes, clearDependencies, *gpgpuCsr); // init
        EXPECT_EQ(0u, previousNodes.peekNodes().size());
    }

    {
        TimestampPacketContainer previousNodes;
        mockCommandQueue->obtainNewTimestampPacketNodes(1, previousNodes, clearDependencies, *bcsCsr);
        EXPECT_EQ(1u, previousNodes.peekNodes().size());
    }

    {
        TimestampPacketContainer previousNodes;
        mockCommandQueue->obtainNewTimestampPacketNodes(1, previousNodes, clearDependencies, *bcsCsr);
        EXPECT_EQ(0u, previousNodes.peekNodes().size());
    }
}

using BlitCopyTests = BlitEnqueueTests<1>;

HWTEST_TEMPLATED_F(BlitCopyTests, givenKernelAllocationInLocalMemoryWhenCreatingWithoutAllowedCpuAccessThenUseBcsForTransfer) {
    DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessDisallowed));
    DebugManager.flags.ForceNonSystemMemoryPlacement.set(1 << (static_cast<int64_t>(GraphicsAllocation::AllocationType::KERNEL_ISA) - 1));

    uint32_t kernelHeap = 0;
    KernelInfo kernelInfo;
    kernelInfo.heapInfo.KernelHeapSize = 1;
    kernelInfo.heapInfo.pKernelHeap = &kernelHeap;

    auto initialTaskCount = bcsMockContext->bcsCsr->peekTaskCount();

    kernelInfo.createKernelAllocation(device->getDevice(), false);

    if (kernelInfo.kernelAllocation->isAllocatedInLocalMemoryPool()) {
        EXPECT_EQ(initialTaskCount + 1, bcsMockContext->bcsCsr->peekTaskCount());
    } else {
        EXPECT_EQ(initialTaskCount, bcsMockContext->bcsCsr->peekTaskCount());
    }

    device->getMemoryManager()->freeGraphicsMemory(kernelInfo.kernelAllocation);
}

HWTEST_TEMPLATED_F(BlitCopyTests, givenKernelAllocationInLocalMemoryWhenCreatingWithAllowedCpuAccessThenDontUseBcsForTransfer) {
    DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessAllowed));
    DebugManager.flags.ForceNonSystemMemoryPlacement.set(1 << (static_cast<int64_t>(GraphicsAllocation::AllocationType::KERNEL_ISA) - 1));

    uint32_t kernelHeap = 0;
    KernelInfo kernelInfo;
    kernelInfo.heapInfo.KernelHeapSize = 1;
    kernelInfo.heapInfo.pKernelHeap = &kernelHeap;

    auto initialTaskCount = bcsMockContext->bcsCsr->peekTaskCount();

    kernelInfo.createKernelAllocation(device->getDevice(), false);

    EXPECT_EQ(initialTaskCount, bcsMockContext->bcsCsr->peekTaskCount());

    device->getMemoryManager()->freeGraphicsMemory(kernelInfo.kernelAllocation);
}

HWTEST_TEMPLATED_F(BlitCopyTests, givenKernelAllocationInLocalMemoryWhenCreatingWithDisallowedCpuAccessAndDisabledBlitterThenFallbackToCpuCopy) {
    DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessDisallowed));
    DebugManager.flags.ForceNonSystemMemoryPlacement.set(1 << (static_cast<int64_t>(GraphicsAllocation::AllocationType::KERNEL_ISA) - 1));

    device->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = false;

    uint32_t kernelHeap = 0;
    KernelInfo kernelInfo;
    kernelInfo.heapInfo.KernelHeapSize = 1;
    kernelInfo.heapInfo.pKernelHeap = &kernelHeap;

    auto initialTaskCount = bcsMockContext->bcsCsr->peekTaskCount();

    kernelInfo.createKernelAllocation(device->getDevice(), false);

    EXPECT_EQ(initialTaskCount, bcsMockContext->bcsCsr->peekTaskCount());

    device->getMemoryManager()->freeGraphicsMemory(kernelInfo.kernelAllocation);
}

HWTEST_TEMPLATED_F(BlitCopyTests, givenLocalMemoryAccessNotAllowedWhenGlobalConstantsAreExportedThenUseBlitter) {
    DebugManager.flags.EnableLocalMemory.set(1);
    DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessDisallowed));

    char constantData[128] = {};
    ProgramInfo programInfo;
    programInfo.globalConstants.initData = constantData;
    programInfo.globalConstants.size = sizeof(constantData);
    auto mockLinkerInput = std::make_unique<WhiteBox<LinkerInput>>();
    mockLinkerInput->traits.exportsGlobalConstants = true;
    programInfo.linkerInput = std::move(mockLinkerInput);

    MockProgram program(bcsMockContext.get(), false, toClDeviceVector(*device));

    EXPECT_EQ(0u, bcsMockContext->bcsCsr->peekTaskCount());

    program.processProgramInfo(programInfo, *device);

    EXPECT_EQ(1u, bcsMockContext->bcsCsr->peekTaskCount());

    auto rootDeviceIndex = device->getRootDeviceIndex();

    ASSERT_NE(nullptr, program.getConstantSurface(rootDeviceIndex));
    auto gpuAddress = reinterpret_cast<const void *>(program.getConstantSurface(rootDeviceIndex)->getGpuAddress());
    EXPECT_NE(nullptr, bcsMockContext->getSVMAllocsManager()->getSVMAlloc(gpuAddress));
}

HWTEST_TEMPLATED_F(BlitCopyTests, givenKernelAllocationInLocalMemoryWithoutCpuAccessAllowedWhenSubstituteKernelHeapIsCalledThenUseBcsForTransfer) {
    DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessDisallowed));
    DebugManager.flags.ForceNonSystemMemoryPlacement.set(1 << (static_cast<int64_t>(GraphicsAllocation::AllocationType::KERNEL_ISA) - 1));

    device->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = true;

    MockKernelWithInternals kernel(*device);
    const size_t initialHeapSize = 0x40;
    kernel.kernelInfo.heapInfo.KernelHeapSize = initialHeapSize;

    kernel.kernelInfo.createKernelAllocation(device->getDevice(), false);
    ASSERT_NE(nullptr, kernel.kernelInfo.kernelAllocation);
    EXPECT_TRUE(kernel.kernelInfo.kernelAllocation->isAllocatedInLocalMemoryPool());

    const size_t newHeapSize = initialHeapSize;
    char newHeap[newHeapSize];

    auto initialTaskCount = bcsMockContext->bcsCsr->peekTaskCount();

    kernel.mockKernel->substituteKernelHeap(newHeap, newHeapSize);

    EXPECT_EQ(initialTaskCount + 1, bcsMockContext->bcsCsr->peekTaskCount());

    device->getMemoryManager()->freeGraphicsMemory(kernel.kernelInfo.kernelAllocation);
}

HWTEST_TEMPLATED_F(BlitCopyTests, givenKernelAllocationInLocalMemoryWithoutCpuAccessAllowedWhenLinkerRequiresPatchingOfInstructionSegmentsThenUseBcsForTransfer) {
    DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessDisallowed));
    DebugManager.flags.ForceNonSystemMemoryPlacement.set(1 << (static_cast<int64_t>(GraphicsAllocation::AllocationType::KERNEL_ISA) - 1));

    device->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = true;

    auto linkerInput = std::make_unique<WhiteBox<LinkerInput>>();
    linkerInput->traits.requiresPatchingOfInstructionSegments = true;

    KernelInfo kernelInfo = {};
    std::vector<char> kernelHeap;
    kernelHeap.resize(32, 7);
    kernelInfo.heapInfo.pKernelHeap = kernelHeap.data();
    kernelInfo.heapInfo.KernelHeapSize = static_cast<uint32_t>(kernelHeap.size());
    kernelInfo.createKernelAllocation(device->getDevice(), false);
    ASSERT_NE(nullptr, kernelInfo.kernelAllocation);
    EXPECT_TRUE(kernelInfo.kernelAllocation->isAllocatedInLocalMemoryPool());

    MockProgram program{nullptr, false, toClDeviceVector(*device)};
    program.getKernelInfoArray(device->getRootDeviceIndex()).push_back(&kernelInfo);
    program.setLinkerInput(device->getRootDeviceIndex(), std::move(linkerInput));

    auto initialTaskCount = bcsMockContext->bcsCsr->peekTaskCount();

    auto ret = program.linkBinary(&device->getDevice(), nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, ret);

    EXPECT_EQ(initialTaskCount + 1, bcsMockContext->bcsCsr->peekTaskCount());

    program.getKernelInfoArray(device->getRootDeviceIndex()).clear();
    device->getMemoryManager()->freeGraphicsMemory(kernelInfo.kernelAllocation);
}

} // namespace NEO
