/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/ult_aub_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_scratch_space_controller_xehp_and_later.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/utilities/base_object_utils.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

using namespace NEO;

using PvcCommandStreamReceiverFlushTaskTests = UltCommandStreamReceiverTest;
PVCTEST_F(PvcCommandStreamReceiverFlushTaskTests, givenOverrideThreadArbitrationPolicyDebugVariableSetForPvcWhenFlushingThenRequestRequiredMode) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    DebugManagerStateRestore restore;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    DebugManager.flags.OverrideThreadArbitrationPolicy.set(ThreadArbitrationPolicy::RoundRobin);

    EXPECT_EQ(-1, commandStreamReceiver.streamProperties.stateComputeMode.threadArbitrationPolicy.value);

    flushTask(commandStreamReceiver);
    EXPECT_EQ(ThreadArbitrationPolicy::RoundRobin,
              commandStreamReceiver.streamProperties.stateComputeMode.threadArbitrationPolicy.value);
}

PVCTEST_F(PvcCommandStreamReceiverFlushTaskTests, givenNotExistPolicyWhenFlushingThenDefaultPolicyIsProgrammed) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    int32_t notExistPolicy = -2;
    flushTaskFlags.threadArbitrationPolicy = notExistPolicy;

    flushTask(commandStreamReceiver);

    EXPECT_EQ(notExistPolicy, commandStreamReceiver.streamProperties.stateComputeMode.threadArbitrationPolicy.value);
}

PVCTEST_F(PvcCommandStreamReceiverFlushTaskTests, givenRevisionBAndAboveWhenLastSpecialPipelineSelectModeIsTrueAndFlushTaskIsCalledThenDontReprogramPipelineSelect) {
    auto hwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    flushTaskFlags.pipelineSelectArgs.specialPipelineSelectMode = true;
    flushTaskFlags.pipelineSelectArgs.mediaSamplerRequired = false;

    struct {
        unsigned short revId;
        bool expectedValue;
    } testInputs[] = {
        {0x0, true},
        {0x1, true},
        {0x3, true},
        {0x5, false},
        {0x6, false},
        {0x7, false},
    };
    for (auto &testInput : testInputs) {
        hwInfo->platform.usRevId = testInput.revId;
        commandStreamReceiver.isPreambleSent = true;
        commandStreamReceiver.lastMediaSamplerConfig = false;

        flushTask(commandStreamReceiver);

        EXPECT_EQ(testInput.expectedValue, commandStreamReceiver.lastSpecialPipelineSelectMode);
        commandStreamReceiver.lastSpecialPipelineSelectMode = false;
    }
}

struct PVcBcsTests : public UltCommandStreamReceiverTest {
    void SetUp() override {
        DebugManager.flags.EnableLocalMemory.set(true);
        UltCommandStreamReceiverTest::SetUp();
        context = std::make_unique<MockContext>(pClDevice);
    }

    void TearDown() override {
        context.reset();
        UltCommandStreamReceiverTest::TearDown();
    }

    DebugManagerStateRestore restore;
    std::unique_ptr<MockContext> context;
    cl_int retVal = CL_SUCCESS;
};

PVCTEST_F(PVcBcsTests, givenCompressibleBuffersWhenStatefulCompressionIsEnabledThenProgramBlitterWithStatefulCompressionSettings) {
    DebugManager.flags.RenderCompressedBuffersEnabled.set(1);

    using MEM_COPY = typename FamilyType::MEM_COPY;
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);
    MockGraphicsAllocation clearColorAlloc;

    auto srcBuffer = clUniquePtr<Buffer>(Buffer::create(context.get(), {}, MemoryConstants::pageSize64k, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto dstBuffer = clUniquePtr<Buffer>(Buffer::create(context.get(), {}, MemoryConstants::pageSize64k, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto srcAllocation = srcBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    EXPECT_TRUE(srcAllocation->getDefaultGmm()->isCompressionEnabled);

    auto dstAllocation = dstBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    EXPECT_TRUE(dstAllocation->getDefaultGmm()->isCompressionEnabled);

    auto blitProperties = BlitProperties::constructPropertiesForCopy(srcAllocation, dstAllocation, 0, 0,
                                                                     {BlitterConstants::maxBlitWidth - 1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);
    auto bltCmd = stream.getSpaceForCmd<MEM_COPY>();
    *bltCmd = FamilyType::cmdInitXyCopyBlt;

    platformsImpl->clear();
    EXPECT_EQ(platform(), nullptr);

    const auto &rootDeviceEnvironment = context->getDevice(0)->getRootDeviceEnvironment();
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForBuffer(blitProperties, *bltCmd, rootDeviceEnvironment);

    EXPECT_EQ(bltCmd->getDestinationCompressionEnable(), MEM_COPY::DESTINATION_COMPRESSION_ENABLE::DESTINATION_COMPRESSION_ENABLE_ENABLE);
    EXPECT_EQ(bltCmd->getDestinationCompressible(), MEM_COPY::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_COMPRESSIBLE);
    EXPECT_EQ(bltCmd->getSourceCompressible(), MEM_COPY::SOURCE_COMPRESSIBLE::SOURCE_COMPRESSIBLE_COMPRESSIBLE);

    auto resourceFormat = srcAllocation->getDefaultGmm()->gmmResourceInfo->getResourceFormat();
    auto compressionFormat = rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat);

    EXPECT_EQ(compressionFormat, bltCmd->getCompressionFormat());
}

PVCTEST_F(PVcBcsTests, givenBufferInDeviceMemoryWhenStatelessCompressionIsEnabledThenBlitterForBufferUsesStatelessCompressedSettings) {
    using MEM_COPY = typename FamilyType::MEM_COPY;
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);
    MockGraphicsAllocation clearColorAlloc;

    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), {}, MemoryConstants::pageSize64k, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto allocation = buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    EXPECT_TRUE(!MemoryPool::isSystemMemoryPool(allocation->getMemoryPool()));

    auto blitProperties = BlitProperties::constructPropertiesForCopy(allocation, allocation,
                                                                     0, 0, {BlitterConstants::maxBlitWidth - 1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);
    auto bltCmd = stream.getSpaceForCmd<MEM_COPY>();
    *bltCmd = FamilyType::cmdInitXyCopyBlt;

    DebugManager.flags.EnableStatelessCompressionWithUnifiedMemory.set(true);
    platformsImpl->clear();
    EXPECT_EQ(platform(), nullptr);

    BlitCommandsHelper<FamilyType>::appendBlitCommandsForBuffer(blitProperties, *bltCmd, context->getDevice(0)->getRootDeviceEnvironment());

    EXPECT_EQ(bltCmd->getDestinationCompressionEnable(), MEM_COPY::DESTINATION_COMPRESSION_ENABLE::DESTINATION_COMPRESSION_ENABLE_ENABLE);
    EXPECT_EQ(bltCmd->getDestinationCompressible(), MEM_COPY::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_COMPRESSIBLE);
    EXPECT_EQ(bltCmd->getSourceCompressible(), MEM_COPY::SOURCE_COMPRESSIBLE::SOURCE_COMPRESSIBLE_COMPRESSIBLE);
    EXPECT_EQ(static_cast<uint32_t>(DebugManager.flags.FormatForStatelessCompressionWithUnifiedMemory.get()), bltCmd->getCompressionFormat());
}

PVCTEST_F(PVcBcsTests, givenBufferInSystemMemoryWhenStatelessCompressionIsEnabledThenBlitterForBufferDoesntUseStatelessCompressedSettings) {
    using MEM_COPY = typename FamilyType::MEM_COPY;
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);
    MockGraphicsAllocation clearColorAlloc;

    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_FORCE_HOST_MEMORY_INTEL, MemoryConstants::pageSize64k, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto allocation = buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    EXPECT_TRUE(MemoryPool::isSystemMemoryPool(allocation->getMemoryPool()));

    auto blitProperties = BlitProperties::constructPropertiesForCopy(allocation, allocation,
                                                                     0, 0, {BlitterConstants::maxBlitWidth - 1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);
    auto bltCmd = stream.getSpaceForCmd<MEM_COPY>();
    *bltCmd = FamilyType::cmdInitXyCopyBlt;

    DebugManager.flags.EnableStatelessCompressionWithUnifiedMemory.set(true);
    platformsImpl->clear();
    EXPECT_EQ(platform(), nullptr);

    BlitCommandsHelper<FamilyType>::appendBlitCommandsForBuffer(blitProperties, *bltCmd, context->getDevice(0)->getRootDeviceEnvironment());

    EXPECT_EQ(bltCmd->getDestinationCompressionEnable(), MEM_COPY::DESTINATION_COMPRESSION_ENABLE::DESTINATION_COMPRESSION_ENABLE_DISABLE);
    EXPECT_EQ(bltCmd->getDestinationCompressible(), MEM_COPY::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_NOT_COMPRESSIBLE);
    EXPECT_EQ(bltCmd->getSourceCompressible(), MEM_COPY::SOURCE_COMPRESSIBLE::SOURCE_COMPRESSIBLE_NOT_COMPRESSIBLE);
    EXPECT_EQ(0u, bltCmd->getCompressionFormat());
}

using PvcMultiRootDeviceCommandStreamReceiverBufferTests = MultiRootDeviceFixture;

PVCTEST_F(PvcMultiRootDeviceCommandStreamReceiverBufferTests, givenMultipleEventInMultiRootDeviceEnvironmentOnPvcWhenTheyArePassedToEnqueueWithSubmissionThenCsIsWaitingForEventsFromPreviousDevices) {
    REQUIRE_SVM_OR_SKIP(device1);
    REQUIRE_SVM_OR_SKIP(device2);

    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    cl_int retVal = 0;
    size_t offset = 0;
    size_t size = 1;

    auto pCmdQ1 = context.get()->getSpecialQueue(1u);
    auto pCmdQ2 = context.get()->getSpecialQueue(2u);

    std::unique_ptr<MockProgram> program(Program::createBuiltInFromSource<MockProgram>("FillBufferBytes", context.get(), context.get()->getDevices(), &retVal));
    program->build(program->getDevices(), nullptr, false);
    std::unique_ptr<MockKernel> kernel(Kernel::create<MockKernel>(program.get(), program->getKernelInfoForKernel("FillBufferBytes"), *context.get()->getDevice(0), &retVal));

    size_t svmSize = 4096;
    void *svmPtr = alignedMalloc(svmSize, MemoryConstants::pageSize);
    MockGraphicsAllocation svmAlloc(svmPtr, svmSize);

    Event event1(pCmdQ1, CL_COMMAND_NDRANGE_KERNEL, 5, 15);
    Event event2(nullptr, CL_COMMAND_NDRANGE_KERNEL, 6, 16);
    Event event3(pCmdQ1, CL_COMMAND_NDRANGE_KERNEL, 4, 20);
    Event event4(pCmdQ2, CL_COMMAND_NDRANGE_KERNEL, 3, 4);
    Event event5(pCmdQ2, CL_COMMAND_NDRANGE_KERNEL, 2, 7);
    UserEvent userEvent1(&pCmdQ1->getContext());
    UserEvent userEvent2(&pCmdQ2->getContext());

    userEvent1.setStatus(CL_COMPLETE);
    userEvent2.setStatus(CL_COMPLETE);

    cl_event eventWaitList[] =
        {
            &event1,
            &event2,
            &event3,
            &event4,
            &event5,
            &userEvent1,
            &userEvent2,
        };
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);

    {
        kernel->setSvmKernelExecInfo(&svmAlloc);

        retVal = pCmdQ1->enqueueKernel(
            kernel.get(),
            1,
            &offset,
            &size,
            &size,
            numEventsInWaitList,
            eventWaitList,
            nullptr);

        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(pCmdQ1->getCS(0));
        auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end());

        EXPECT_EQ(3u, semaphores.size());

        auto semaphoreCmd0 = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[0]));
        EXPECT_EQ(4u, semaphoreCmd0->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(pCmdQ2->getGpgpuCommandStreamReceiver().getTagAddress()), semaphoreCmd0->getSemaphoreGraphicsAddress());

        auto semaphoreCmd1 = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[1]));
        EXPECT_EQ(7u, semaphoreCmd1->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(pCmdQ2->getGpgpuCommandStreamReceiver().getTagAddress()), semaphoreCmd1->getSemaphoreGraphicsAddress());
    }

    {
        kernel->setSvmKernelExecInfo(&svmAlloc);

        retVal = pCmdQ2->enqueueKernel(
            kernel.get(),
            1,
            &offset,
            &size,
            &size,
            numEventsInWaitList,
            eventWaitList,
            nullptr);

        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(pCmdQ2->getCS(0));
        auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end());

        EXPECT_EQ(3u, semaphores.size());

        auto semaphoreCmd0 = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[0]));
        EXPECT_EQ(15u, semaphoreCmd0->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(pCmdQ1->getGpgpuCommandStreamReceiver().getTagAddress()), semaphoreCmd0->getSemaphoreGraphicsAddress());

        auto semaphoreCmd1 = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[1]));
        EXPECT_EQ(20u, semaphoreCmd1->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(pCmdQ1->getGpgpuCommandStreamReceiver().getTagAddress()), semaphoreCmd1->getSemaphoreGraphicsAddress());
    }
    alignedFree(svmPtr);
}

using PvcCommandStreamReceiverTests = ::testing::Test;
PVCTEST_F(PvcCommandStreamReceiverTests, givenScratchSpaceRequiredWhenScratchSurfaceStateIsProgrammedThenSizeDoesNotExceedMaxThreadCount) {
    using CFE_STATE = typename FamilyType::CFE_STATE;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    HardwareInfo hwInfo = *defaultHwInfo;

    int revisions[] = {0, 3};
    int expectComputeUnitsEqualMaxThreadCount[] = {true, false};

    for (auto i = 0; i < 2; i++) {
        hwInfo.platform.usRevId = revisions[i];
        auto pDevice = MockClDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0);
        ASSERT_NE(nullptr, pDevice);
        auto pClDevice = std::make_unique<MockClDevice>(pDevice);

        size_t GWS = 1;
        MockContext ctx(pClDevice.get());
        MockKernelWithInternals kernel(*pClDevice);
        CommandQueueHw<FamilyType> commandQueue(&ctx, pClDevice.get(), 0, false);
        auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
        auto scratchController = static_cast<MockScratchSpaceControllerXeHPAndLater *>(commandStreamReceiver->getScratchSpaceController());
        scratchController->slotId = 2u;
        pDevice->resetCommandStreamReceiver(commandStreamReceiver);
        auto &commandStreamCSR = commandStreamReceiver->getCS();

        kernel.kernelInfo.kernelDescriptor.kernelAttributes.perThreadScratchSize[0] = 0x1000;
        auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
        uint32_t computeUnits = hwHelper.getComputeUnitsUsedForScratch(&hwInfo);

        auto maxSubSlice = HwInfoConfig::get(hwInfo.platform.eProductFamily)->computeMaxNeededSubSliceSpace(hwInfo);
        auto maxHwThreadCount = maxSubSlice * hwInfo.gtSystemInfo.MaxEuPerSubSlice * (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount);

        if (expectComputeUnitsEqualMaxThreadCount[i]) {
            EXPECT_EQ(maxHwThreadCount, computeUnits);
        } else {
            EXPECT_NE(maxHwThreadCount, computeUnits);
        }

        size_t scratchSpaceSize = kernel.kernelInfo.kernelDescriptor.kernelAttributes.perThreadScratchSize[0] * computeUnits;

        commandQueue.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, nullptr);
        commandQueue.flush();

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            cmdList,
            ptrOffset(commandStreamCSR.getCpuBase(), 0),
            commandStreamCSR.getUsed()));

        auto itor = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itor);

        CFE_STATE *cfeState = genCmdCast<CFE_STATE *>(*itor);
        ASSERT_NE(nullptr, cfeState);

        EXPECT_EQ(scratchSpaceSize, scratchController->getScratchSpaceAllocation()->getUnderlyingBufferSize());

        uint32_t bufferOffset = static_cast<uint32_t>(scratchController->slotId * scratchController->singleSurfaceStateSize * 2);
        EXPECT_EQ(bufferOffset, cfeState->getScratchSpaceBuffer());
        RENDER_SURFACE_STATE *scratchState = reinterpret_cast<RENDER_SURFACE_STATE *>(scratchController->surfaceStateHeap + bufferOffset);
        EXPECT_EQ(scratchController->scratchAllocation->getGpuAddress(), scratchState->getSurfaceBaseAddress());
        EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_SCRATCH, scratchState->getSurfaceType());

        SURFACE_STATE_BUFFER_LENGTH length = {0};
        length.Length = static_cast<uint32_t>(maxHwThreadCount - 1);
        EXPECT_EQ(length.SurfaceState.Depth + 1u, scratchState->getDepth());
        EXPECT_EQ(length.SurfaceState.Width + 1u, scratchState->getWidth());
        EXPECT_EQ(length.SurfaceState.Height + 1u, scratchState->getHeight());
        EXPECT_EQ(kernel.kernelInfo.kernelDescriptor.kernelAttributes.perThreadScratchSize[0], scratchState->getSurfacePitch());
    }
}
