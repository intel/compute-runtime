/*
 * Copyright (C) 2021-2025 Intel Corporation
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
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/ult_aub_command_stream_receiver.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/utilities/base_object_utils.h"

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
    DebugManagerStateRestore restore;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    debugManager.flags.OverrideThreadArbitrationPolicy.set(ThreadArbitrationPolicy::RoundRobin);

    EXPECT_EQ(-1, commandStreamReceiver.streamProperties.stateComputeMode.threadArbitrationPolicy.value);

    flushTask(commandStreamReceiver);
    EXPECT_EQ(ThreadArbitrationPolicy::RoundRobin,
              commandStreamReceiver.streamProperties.stateComputeMode.threadArbitrationPolicy.value);
}

PVCTEST_F(PvcCommandStreamReceiverFlushTaskTests, givenNotExistPolicyWhenFlushingThenDefaultPolicyIsProgrammed) {
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    int32_t notExistPolicy = -2;
    flushTaskFlags.threadArbitrationPolicy = notExistPolicy;

    flushTask(commandStreamReceiver);

    EXPECT_EQ(notExistPolicy, commandStreamReceiver.streamProperties.stateComputeMode.threadArbitrationPolicy.value);
}

PVCTEST_F(PvcCommandStreamReceiverFlushTaskTests, givenRevisionBAndAboveWhenLastSystolicPipelineSelectModeIsTrueAndFlushTaskIsCalledThenDontReprogramPipelineSelect) {
    auto hwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    flushTaskFlags.pipelineSelectArgs.systolicPipelineSelectMode = true;
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
    auto &productHelper = pDevice->getProductHelper();
    for (auto &testInput : testInputs) {
        hwInfo->platform.usRevId = testInput.revId;
        productHelper.fillPipelineSelectPropertiesSupportStructure(commandStreamReceiver.pipelineSupportFlags, *hwInfo);
        commandStreamReceiver.isPreambleSent = true;
        commandStreamReceiver.lastMediaSamplerConfig = false;
        commandStreamReceiver.lastSystolicPipelineSelectMode = false;
        commandStreamReceiver.streamProperties.pipelineSelect.systolicMode.value = -1;

        flushTask(commandStreamReceiver);

        if (testInput.expectedValue) {
            EXPECT_TRUE(commandStreamReceiver.lastSystolicPipelineSelectMode);
        } else {
            EXPECT_FALSE(commandStreamReceiver.lastSystolicPipelineSelectMode);
        }
    }
}

struct PVcBcsTests : public UltCommandStreamReceiverTest {
    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(true);
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
    debugManager.flags.RenderCompressedBuffersEnabled.set(1);

    using MEM_COPY = typename FamilyType::MEM_COPY;
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);
    MockGraphicsAllocation clearColorAlloc;

    auto srcBuffer = clUniquePtr<Buffer>(Buffer::create(context.get(), {}, MemoryConstants::pageSize64k, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto dstBuffer = clUniquePtr<Buffer>(Buffer::create(context.get(), {}, MemoryConstants::pageSize64k, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto srcAllocation = srcBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    EXPECT_TRUE(srcAllocation->getDefaultGmm()->isCompressionEnabled());

    auto dstAllocation = dstBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    EXPECT_TRUE(dstAllocation->getDefaultGmm()->isCompressionEnabled());

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
    EXPECT_TRUE(!MemoryPoolHelper::isSystemMemoryPool(allocation->getMemoryPool()));

    auto blitProperties = BlitProperties::constructPropertiesForCopy(allocation, allocation,
                                                                     0, 0, {BlitterConstants::maxBlitWidth - 1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);
    auto bltCmd = stream.getSpaceForCmd<MEM_COPY>();
    *bltCmd = FamilyType::cmdInitXyCopyBlt;

    debugManager.flags.EnableStatelessCompressionWithUnifiedMemory.set(true);
    platformsImpl->clear();
    EXPECT_EQ(platform(), nullptr);

    BlitCommandsHelper<FamilyType>::appendBlitCommandsForBuffer(blitProperties, *bltCmd, context->getDevice(0)->getRootDeviceEnvironment());

    EXPECT_EQ(bltCmd->getDestinationCompressionEnable(), MEM_COPY::DESTINATION_COMPRESSION_ENABLE::DESTINATION_COMPRESSION_ENABLE_ENABLE);
    EXPECT_EQ(bltCmd->getDestinationCompressible(), MEM_COPY::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_COMPRESSIBLE);
    EXPECT_EQ(bltCmd->getSourceCompressible(), MEM_COPY::SOURCE_COMPRESSIBLE::SOURCE_COMPRESSIBLE_COMPRESSIBLE);
    EXPECT_EQ(static_cast<uint32_t>(debugManager.flags.FormatForStatelessCompressionWithUnifiedMemory.get()), bltCmd->getCompressionFormat());
}

PVCTEST_F(PVcBcsTests, givenBufferInSystemMemoryWhenStatelessCompressionIsEnabledThenBlitterForBufferDoesntUseStatelessCompressedSettings) {
    using MEM_COPY = typename FamilyType::MEM_COPY;
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);
    MockGraphicsAllocation clearColorAlloc;

    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_FORCE_HOST_MEMORY_INTEL, MemoryConstants::pageSize64k, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto allocation = buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    EXPECT_TRUE(MemoryPoolHelper::isSystemMemoryPool(allocation->getMemoryPool()));

    auto blitProperties = BlitProperties::constructPropertiesForCopy(allocation, allocation,
                                                                     0, 0, {BlitterConstants::maxBlitWidth - 1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);
    auto bltCmd = stream.getSpaceForCmd<MEM_COPY>();
    *bltCmd = FamilyType::cmdInitXyCopyBlt;

    debugManager.flags.EnableStatelessCompressionWithUnifiedMemory.set(true);
    platformsImpl->clear();
    EXPECT_EQ(platform(), nullptr);

    BlitCommandsHelper<FamilyType>::appendBlitCommandsForBuffer(blitProperties, *bltCmd, context->getDevice(0)->getRootDeviceEnvironment());

    EXPECT_EQ(bltCmd->getDestinationCompressionEnable(), MEM_COPY::DESTINATION_COMPRESSION_ENABLE::DESTINATION_COMPRESSION_ENABLE_DISABLE);
    EXPECT_EQ(bltCmd->getDestinationCompressible(), MEM_COPY::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_NOT_COMPRESSIBLE);
    EXPECT_EQ(bltCmd->getSourceCompressible(), MEM_COPY::SOURCE_COMPRESSIBLE::SOURCE_COMPRESSIBLE_NOT_COMPRESSIBLE);
    EXPECT_EQ(0u, bltCmd->getCompressionFormat());
}

using PvcMultiRootDeviceCommandStreamReceiverBufferTests = MultiRootDeviceFixture;

HWTEST_EXCLUDE_PRODUCT(MultiRootDeviceCommandStreamReceiverBufferTests, givenMultipleEventInMultiRootDeviceEnvironmentWhenTheyArePassedToEnqueueWithSubmissionThenCsIsWaitingForEventsFromPreviousDevices, IGFX_PVC);

PVCTEST_F(PvcMultiRootDeviceCommandStreamReceiverBufferTests, givenMultipleEventInMultiRootDeviceEnvironmentOnPvcWhenTheyArePassedToEnqueueWithSubmissionThenCsIsWaitingForEventsFromPreviousDevices) {
    USE_REAL_FILE_SYSTEM();
    REQUIRE_SVM_OR_SKIP(device1);
    REQUIRE_SVM_OR_SKIP(device2);

    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    cl_int retVal = 0;
    size_t offset = 0;
    size_t size = 1;

    auto pCmdQ1 = context->getSpecialQueue(1u);
    auto pCmdQ2 = context->getSpecialQueue(2u);

    std::unique_ptr<MockProgram> program(Program::createBuiltInFromSource<MockProgram>("FillBufferBytes", context.get(), context->getDevices(), &retVal));
    program->build(program->getDevices(), nullptr);
    std::unique_ptr<MockKernel> kernel(Kernel::create<MockKernel>(program.get(), program->getKernelInfoForKernel("FillBufferBytes"), *context->getDevice(0), retVal));

    size_t svmSize = 4096;
    void *svmPtr = alignedMalloc(svmSize, MemoryConstants::pageSize);
    MockGraphicsAllocation svmAlloc(svmPtr, svmSize);

    Event event1(pCmdQ1, CL_COMMAND_NDRANGE_KERNEL, 5, 15);
    auto node1 = event1.getMultiRootTimestampSyncNode();
    Event event2(nullptr, CL_COMMAND_NDRANGE_KERNEL, 6, 16);
    Event event3(pCmdQ1, CL_COMMAND_NDRANGE_KERNEL, 4, 20);
    auto node3 = event3.getMultiRootTimestampSyncNode();
    Event event4(pCmdQ2, CL_COMMAND_NDRANGE_KERNEL, 3, 4);
    auto node4 = event4.getMultiRootTimestampSyncNode();
    Event event5(pCmdQ2, CL_COMMAND_NDRANGE_KERNEL, 2, 7);
    auto node5 = event5.getMultiRootTimestampSyncNode();
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

        EXPECT_EQ(2u, semaphores.size());

        auto semaphoreCmd0 = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[0]));
        EXPECT_EQ(1u, semaphoreCmd0->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(node4->getContextEndAddress(0u)), semaphoreCmd0->getSemaphoreGraphicsAddress());

        auto semaphoreCmd1 = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[1]));
        EXPECT_EQ(1u, semaphoreCmd1->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(node5->getContextEndAddress(0u)), semaphoreCmd1->getSemaphoreGraphicsAddress());
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

        EXPECT_EQ(2u, semaphores.size());

        auto semaphoreCmd0 = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[0]));
        EXPECT_EQ(1u, semaphoreCmd0->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(node1->getContextEndAddress(0u)), semaphoreCmd0->getSemaphoreGraphicsAddress());

        auto semaphoreCmd1 = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[1]));
        EXPECT_EQ(1u, semaphoreCmd1->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(node3->getContextEndAddress(0u)), semaphoreCmd1->getSemaphoreGraphicsAddress());
    }
    alignedFree(svmPtr);
}
