/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/source/helpers/pause_on_gpu_properties.h"
#include "shared/source/helpers/vec.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/compiler_interface/linker_mock.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/event/user_event.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

namespace NEO {

template <int timestampPacketEnabled>
struct BlitEnqueueTests : public ::testing::Test {
    class BcsMockContext : public MockContext {
      public:
        BcsMockContext(ClDevice *device) : MockContext(device) {
            bcsOsContext.reset(OsContext::create(nullptr, device->getRootDeviceIndex(), 0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular})));
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
                bcsCsr->flushBcsTask(container, true, false, const_cast<Device &>(device));

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
    void setUpT() {
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

        if (!HwInfoConfig::get(defaultHwInfo->platform.eProductFamily)->isBlitterFullySupported(device->getHardwareInfo())) {
            GTEST_SKIP();
        }
        if (createBcsEngine) {
            auto &engine = device->getEngine(getChosenEngineType(device->getHardwareInfo()), EngineUsage::LowPriority);
            bcsOsContext.reset(OsContext::create(nullptr, device->getRootDeviceIndex(), 1,
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

        gpgpuCsr = &mockCmdQueue->getGpgpuCommandStreamReceiver();
        bcsCsr = mockCmdQueue->bcsEngines[0]->commandStreamReceiver;
    }

    template <typename FamilyType>
    void tearDownT() {}

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
            mockKernel->mockKernel->setArgSvmAlloc(i, ptr, alloc, 0u);
        }
    }

    ReleaseableObjectPtr<Buffer> createBuffer(size_t size, bool compressed) {
        auto buffer = clUniquePtr<Buffer>(Buffer::create(bcsMockContext.get(), CL_MEM_READ_WRITE, size, nullptr, retVal));
        auto graphicsAllocation = buffer->getGraphicsAllocation(device->getRootDeviceIndex());

        setAllocationType(graphicsAllocation, compressed);

        return buffer;
    }

    MockGraphicsAllocation *createGfxAllocation(size_t size, bool compressed) {
        auto alloc = new MockGraphicsAllocation(nullptr, size);
        setAllocationType(alloc, compressed);
        return alloc;
    }

    void setAllocationType(GraphicsAllocation *graphicsAllocation, bool compressed) {
        graphicsAllocation->setAllocationType(AllocationType::BUFFER);

        if (compressed && !graphicsAllocation->getDefaultGmm()) {
            auto gmmHelper = device->getRootDeviceEnvironment().getGmmHelper();

            graphicsAllocation->setDefaultGmm(new Gmm(gmmHelper, nullptr, 0, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
        }

        if (graphicsAllocation->getDefaultGmm()) {
            graphicsAllocation->getDefaultGmm()->isCompressionEnabled = compressed;
        }
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

    template <typename Command>
    void expectNoCommand(GenCmdList::iterator itorStart, GenCmdList::iterator itorEnd) {
        auto commandItor = find<Command *>(itorStart, itorEnd);
        EXPECT_TRUE(commandItor == itorEnd);
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

} // namespace NEO
