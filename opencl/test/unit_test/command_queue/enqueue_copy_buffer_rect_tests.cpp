/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/gen_common/reg_configs_common.h"
#include "shared/source/helpers/constants.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/test/unit_test/command_queue/enqueue_copy_buffer_rect_fixture.h"
#include "opencl/test/unit_test/gen_common/gen_commands_common_validation.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

using namespace NEO;

const size_t EnqueueCopyBufferRectTest::BufferRect::sizeInBytes = 100 * 100 * 100 * sizeof(cl_char);

HWTEST_F(EnqueueCopyBufferRectTest, GivenNullSrcMemObjWhenCopyingBufferRectThenClInvalidMemObjectErrorIsReturned) {
    auto retVal = CL_SUCCESS;
    size_t srcOrigin[] = {0, 0, 0};
    size_t dstOrigin[] = {0, 0, 0};
    size_t region[] = {1, 1, 0};

    retVal = clEnqueueCopyBufferRect(
        pCmdQ,
        nullptr,
        dstBuffer,
        srcOrigin,
        dstOrigin,
        region,
        10,
        0,
        10,
        0,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

HWTEST_F(EnqueueCopyBufferRectTest, GivenNullDstMemObjWhenCopyingBufferRectThenClInvalidMemObjectErrorIsReturned) {
    auto retVal = CL_SUCCESS;
    size_t srcOrigin[] = {0, 0, 0};
    size_t dstOrigin[] = {0, 0, 0};
    size_t region[] = {1, 1, 0};

    retVal = clEnqueueCopyBufferRect(
        pCmdQ,
        srcBuffer,
        nullptr,
        srcOrigin,
        dstOrigin,
        region,
        10,
        0,
        10,
        0,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

HWTEST_F(EnqueueCopyBufferRectTest, GivenValidParametersWhenCopyingBufferRectThenSuccessIsReturned) {
    auto retVal = CL_SUCCESS;
    size_t srcOrigin[] = {0, 0, 0};
    size_t dstOrigin[] = {0, 0, 0};
    size_t region[] = {1, 1, 1};

    retVal = clEnqueueCopyBufferRect(
        pCmdQ,
        srcBuffer,
        dstBuffer,
        srcOrigin,
        dstOrigin,
        region,
        10,
        0,
        10,
        0,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(EnqueueCopyBufferRectTest, GivenGpuHangAndBlockingCallAndValidParametersWhenCopyingBufferRectThenOutOfResourcesIsReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.MakeEachEnqueueBlocking.set(true);

    std::unique_ptr<ClDevice> device(new MockClDevice{MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)});
    cl_queue_properties props = {};

    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context, device.get(), &props);
    mockCommandQueueHw.waitForAllEnginesReturnValue = WaitStatus::gpuHang;

    size_t srcOrigin[] = {0, 0, 0};
    size_t dstOrigin[] = {0, 0, 0};
    size_t region[] = {1, 1, 1};

    const auto enqueueResult = clEnqueueCopyBufferRect(
        &mockCommandQueueHw,
        srcBuffer,
        dstBuffer,
        srcOrigin,
        dstOrigin,
        region,
        10,
        0,
        10,
        0,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, enqueueResult);
    EXPECT_EQ(1, mockCommandQueueHw.waitForAllEnginesCalledCount);
}

HWTEST_F(EnqueueCopyBufferRectTest, WhenCopyingBufferRect2DThenTaskCountIsAlignedWithCsr) {
    // this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;

    enqueueCopyBufferRect2D<FamilyType>();
    EXPECT_EQ(csr.peekTaskCount(), pCmdQ->taskCount);

    auto cmdQtaskLevel = pCmdQ->taskLevel;
    if (!csr.isUpdateTagFromWaitEnabled()) {
        cmdQtaskLevel++;
    }
    EXPECT_EQ(csr.peekTaskLevel(), cmdQtaskLevel);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueCopyBufferRectTest, WhenCopyingBufferRect2DThenGpgpuWalkerIsCorrect) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    enqueueCopyBufferRect2D<FamilyType>();

    auto *cmd = (GPGPU_WALKER *)cmdWalker;
    ASSERT_NE(nullptr, cmd);

    // Verify GPGPU_WALKER parameters
    EXPECT_NE(0u, cmd->getThreadGroupIdXDimension());
    EXPECT_NE(0u, cmd->getThreadGroupIdYDimension());
    EXPECT_EQ(1u, cmd->getThreadGroupIdZDimension());
    EXPECT_NE(0u, cmd->getRightExecutionMask());
    EXPECT_NE(0u, cmd->getBottomExecutionMask());
    EXPECT_EQ(GPGPU_WALKER::SIMD_SIZE_SIMD32, cmd->getSimdSize());
    EXPECT_NE(0u, cmd->getIndirectDataLength());
    EXPECT_EQ(0u, cmd->getIndirectDataLength() % GPGPU_WALKER::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);
    EXPECT_FALSE(cmd->getIndirectParameterEnable());

    // Compute the SIMD lane mask
    size_t simd =
        cmd->getSimdSize() == GPGPU_WALKER::SIMD_SIZE_SIMD32 ? 32 : cmd->getSimdSize() == GPGPU_WALKER::SIMD_SIZE_SIMD16 ? 16
                                                                                                                         : 8;
    uint64_t simdMask = maxNBitValue(simd);

    // Mask off lanes based on the execution masks
    auto laneMaskRight = cmd->getRightExecutionMask() & simdMask;
    auto lanesPerThreadX = 0;
    while (laneMaskRight) {
        lanesPerThreadX += laneMaskRight & 1;
        laneMaskRight >>= 1;
    }
}

HWTEST_F(EnqueueCopyBufferRectTest, WhenCopyingBufferRect2DThenTaskLevelIsIncremented) {
    auto taskLevelBefore = pCmdQ->taskLevel;

    enqueueCopyBufferRect2D<FamilyType>();
    EXPECT_GT(pCmdQ->taskLevel, taskLevelBefore);
}

HWTEST_F(EnqueueCopyBufferRectTest, WhenCopyingBufferRect2DThenCommandsAreAdded) {
    auto usedCmdBufferBefore = pCS->getUsed();

    enqueueCopyBufferRect2D<FamilyType>();
    EXPECT_NE(usedCmdBufferBefore, pCS->getUsed());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueCopyBufferRectTest, WhenCopyingBufferRect2DThenIndirectDataGetsAdded) {
    auto dshBefore = pDSH->getUsed();
    auto iohBefore = pIOH->getUsed();
    auto sshBefore = pSSH->getUsed();

    enqueueCopyBufferRect2D<FamilyType>();

    // Extract the kernel used
    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferRect,
                                                                            pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcMemObj = srcBuffer;
    dc.dstMemObj = dstBuffer;
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {50, 50, 1};
    dc.srcRowPitch = rowPitch;
    dc.srcSlicePitch = slicePitch;
    dc.dstRowPitch = rowPitch;
    dc.dstSlicePitch = slicePitch;

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    EXPECT_NE(dshBefore, pDSH->getUsed());
    EXPECT_NE(iohBefore, pIOH->getUsed());
    if (kernel->getKernelInfo().kernelDescriptor.kernelAttributes.bufferAddressingMode == KernelDescriptor::BindfulAndStateless) {
        EXPECT_NE(sshBefore, pSSH->getUsed());
    }
}

HWTEST_F(EnqueueCopyBufferRectTest, WhenCopyingBufferRectStatelessThenStatelessKernelIsUsed) {

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferRectStateless,
                                                                            pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcMemObj = srcBuffer;
    dc.dstMemObj = dstBuffer;
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {50, 50, 1};
    dc.srcRowPitch = rowPitch;
    dc.srcSlicePitch = slicePitch;
    dc.dstRowPitch = rowPitch;
    dc.dstSlicePitch = slicePitch;

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelAttributes.supportsBuffersBiggerThan4Gb());
    EXPECT_FALSE(kernel->getKernelInfo().getArgDescriptorAt(0).as<ArgDescPointer>().isPureStateful());
}

HWTEST2_F(EnqueueCopyBufferRectTest, WhenCopyingBufferRectStatelessHeaplessThenCorrectKernelIsUsed, HeaplessSupportedMatcher) {

    if (is32bit) {
        GTEST_SKIP();
    }

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferRectStatelessHeapless,
                                                                            pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcMemObj = srcBuffer;
    dc.dstMemObj = dstBuffer;
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {50, 50, 1};
    dc.srcRowPitch = rowPitch;
    dc.srcSlicePitch = slicePitch;
    dc.dstRowPitch = rowPitch;
    dc.dstSlicePitch = slicePitch;

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    auto &kernelDescriptor = kernel->getKernelInfo().kernelDescriptor;
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.supportsBuffersBiggerThan4Gb());
    EXPECT_FALSE(kernel->getKernelInfo().getArgDescriptorAt(0).as<ArgDescPointer>().isPureStateful());

    auto indirectDataPointerAddress = kernelDescriptor.payloadMappings.implicitArgs.indirectDataPointerAddress;
    auto scratchPointerAddress = kernelDescriptor.payloadMappings.implicitArgs.scratchPointerAddress;

    EXPECT_EQ(0u, indirectDataPointerAddress.offset);
    EXPECT_EQ(8u, indirectDataPointerAddress.pointerSize);

    EXPECT_EQ(8u, scratchPointerAddress.offset);
    EXPECT_EQ(8u, scratchPointerAddress.pointerSize);
}

HWTEST_F(EnqueueCopyBufferRectTest, WhenCopyingBufferRect2DThenL3ProgrammingIsCorrect) {
    enqueueCopyBufferRect2D<FamilyType>();
    validateL3Programming<FamilyType>(cmdList, itorWalker);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueCopyBufferRectTest, When2DEnqueueIsDoneThenStateBaseAddressIsProperlyProgrammed) {
    enqueueCopyBufferRect2D<FamilyType>();
    auto &ultCsr = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();

    validateStateBaseAddress<FamilyType>(ultCsr.getMemoryManager()->getInternalHeapBaseAddress(ultCsr.rootDeviceIndex, pIOH->getGraphicsAllocation()->isAllocatedInLocalMemoryPool()),
                                         ultCsr.getMemoryManager()->getInternalHeapBaseAddress(ultCsr.rootDeviceIndex, !gfxCoreHelper.useSystemMemoryPlacementForISA(pDevice->getHardwareInfo())),
                                         pDSH, pIOH, pSSH, itorPipelineSelect, itorWalker, cmdList, 0llu);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueCopyBufferRectTest, WhenCopyingBufferRect2DThenMediaInterfaceDescriptorLoadIsCorrect) {
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueCopyBufferRect2D<FamilyType>();

    auto *cmd = (MEDIA_INTERFACE_DESCRIPTOR_LOAD *)cmdMediaInterfaceDescriptorLoad;
    ASSERT_NE(nullptr, cmd);

    // Verify we have a valid length -- multiple of INTERFACE_DESCRIPTOR_DATAs
    EXPECT_EQ(0u, cmd->getInterfaceDescriptorTotalLength() % sizeof(INTERFACE_DESCRIPTOR_DATA));

    // Validate the start address
    size_t alignmentStartAddress = 64 * sizeof(uint8_t);
    EXPECT_EQ(0u, cmd->getInterfaceDescriptorDataStartAddress() % alignmentStartAddress);

    // Validate the length
    EXPECT_NE(0u, cmd->getInterfaceDescriptorTotalLength());
    size_t alignmentTotalLength = 32 * sizeof(uint8_t);
    EXPECT_EQ(0u, cmd->getInterfaceDescriptorTotalLength() % alignmentTotalLength);

    // Generically validate this command
    FamilyType::Parse::template validateCommand<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(cmdList.begin(), itorMediaInterfaceDescriptorLoad);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueCopyBufferRectTest, WhenCopyingBufferRect2DThenInterfaceDescriptorDataIsCorrect) {
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueCopyBufferRect2D<FamilyType>();

    auto *cmdSBA = (STATE_BASE_ADDRESS *)cmdStateBaseAddress;
    auto &idd = *(INTERFACE_DESCRIPTOR_DATA *)cmdInterfaceDescriptorData;

    // Validate the kernel start pointer.  Technically, a kernel can start at address 0 but let's force a value.
    auto kernelStartPointer = ((uint64_t)idd.getKernelStartPointerHigh() << 32) + idd.getKernelStartPointer();
    EXPECT_LE(kernelStartPointer, cmdSBA->getInstructionBufferSize() * MemoryConstants::pageSize);

    EXPECT_NE(0u, idd.getNumberOfThreadsInGpgpuThreadGroup());
    EXPECT_NE(0u, idd.getCrossThreadConstantDataReadLength());
    EXPECT_NE(0u, idd.getConstantIndirectUrbEntryReadLength());
}

HWTEST2_F(EnqueueCopyBufferRectTest, WhenCopyingBufferRect2DThenNumberOfPipelineSelectsIsOne, IsAtMostXeCore) {
    enqueueCopyBufferRect2D<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueCopyBufferRectTest, WhenCopyingBufferRect2DThenMediaVfeStateIsSetCorrectly) {
    enqueueCopyBufferRect2D<FamilyType>();
    validateMediaVFEState<FamilyType>(&pDevice->getHardwareInfo(), cmdMediaVfeState, cmdList, itorMediaVfeState);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueCopyBufferRectTest, WhenCopyingBufferRect3DThenGpgpuWalkerIsCorrect) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    enqueueCopyBufferRect3D<FamilyType>();

    auto *cmd = (GPGPU_WALKER *)cmdWalker;
    ASSERT_NE(nullptr, cmd);

    // Verify GPGPU_WALKER parameters
    EXPECT_NE(0u, cmd->getThreadGroupIdXDimension());
    EXPECT_NE(0u, cmd->getThreadGroupIdYDimension());
    EXPECT_NE(0u, cmd->getThreadGroupIdZDimension());
    EXPECT_NE(0u, cmd->getRightExecutionMask());
    EXPECT_NE(0u, cmd->getBottomExecutionMask());
    EXPECT_EQ(GPGPU_WALKER::SIMD_SIZE_SIMD32, cmd->getSimdSize());
    EXPECT_NE(0u, cmd->getIndirectDataLength());
    EXPECT_EQ(0u, cmd->getIndirectDataLength() % GPGPU_WALKER::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);
    EXPECT_FALSE(cmd->getIndirectParameterEnable());

    // Compute the SIMD lane mask
    size_t simd =
        cmd->getSimdSize() == GPGPU_WALKER::SIMD_SIZE_SIMD32 ? 32 : cmd->getSimdSize() == GPGPU_WALKER::SIMD_SIZE_SIMD16 ? 16
                                                                                                                         : 8;
    uint64_t simdMask = maxNBitValue(simd);

    // Mask off lanes based on the execution masks
    auto laneMaskRight = cmd->getRightExecutionMask() & simdMask;
    auto lanesPerThreadX = 0;
    while (laneMaskRight) {
        lanesPerThreadX += laneMaskRight & 1;
        laneMaskRight >>= 1;
    }
}

HWTEST_F(EnqueueCopyBufferRectTest, WhenCopyingBufferRect3DThenTaskLevelIsIncremented) {
    auto taskLevelBefore = pCmdQ->taskLevel;

    enqueueCopyBufferRect3D<FamilyType>();
    EXPECT_GT(pCmdQ->taskLevel, taskLevelBefore);
}

HWTEST_F(EnqueueCopyBufferRectTest, WhenCopyingBufferRect3DThenCommandsAreAdded) {
    auto usedCmdBufferBefore = pCS->getUsed();

    enqueueCopyBufferRect3D<FamilyType>();
    EXPECT_NE(usedCmdBufferBefore, pCS->getUsed());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueCopyBufferRectTest, WhenCopyingBufferRect3DThenIndirectDataIsAdded) {
    auto usedIndirectHeapBefore = pDSH->getUsed();

    enqueueCopyBufferRect3D<FamilyType>();
    EXPECT_NE(usedIndirectHeapBefore, pDSH->getUsed());
}

HWTEST_F(EnqueueCopyBufferRectTest, WhenCopyingBufferRect3DThenL3ProgrammingIsCorrect) {
    enqueueCopyBufferRect3D<FamilyType>();
    validateL3Programming<FamilyType>(cmdList, itorWalker);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueCopyBufferRectTest, When3DEnqueueIsDoneThenStateBaseAddressIsProperlyProgrammed) {
    enqueueCopyBufferRect3D<FamilyType>();
    auto &ultCsr = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();

    validateStateBaseAddress<FamilyType>(ultCsr.getMemoryManager()->getInternalHeapBaseAddress(ultCsr.rootDeviceIndex, pIOH->getGraphicsAllocation()->isAllocatedInLocalMemoryPool()),
                                         ultCsr.getMemoryManager()->getInternalHeapBaseAddress(ultCsr.rootDeviceIndex, !gfxCoreHelper.useSystemMemoryPlacementForISA(pDevice->getHardwareInfo())),
                                         pDSH, pIOH, pSSH, itorPipelineSelect, itorWalker, cmdList, 0llu);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueCopyBufferRectTest, WhenCopyingBufferRect3DThenMediaInterfaceDescriptorLoadIsCorrect) {
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueCopyBufferRect3D<FamilyType>();

    auto *cmd = (MEDIA_INTERFACE_DESCRIPTOR_LOAD *)cmdMediaInterfaceDescriptorLoad;
    ASSERT_NE(nullptr, cmd);

    // Verify we have a valid length -- multiple of INTERFACE_DESCRIPTOR_DATAs
    EXPECT_EQ(0u, cmd->getInterfaceDescriptorTotalLength() % sizeof(INTERFACE_DESCRIPTOR_DATA));

    // Validate the start address
    size_t alignmentStartAddress = 64 * sizeof(uint8_t);
    EXPECT_EQ(0u, cmd->getInterfaceDescriptorDataStartAddress() % alignmentStartAddress);

    // Validate the length
    EXPECT_NE(0u, cmd->getInterfaceDescriptorTotalLength());
    size_t alignmentTotalLength = 32 * sizeof(uint8_t);
    EXPECT_EQ(0u, cmd->getInterfaceDescriptorTotalLength() % alignmentTotalLength);

    // Generically validate this command
    FamilyType::Parse::template validateCommand<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(cmdList.begin(), itorMediaInterfaceDescriptorLoad);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueCopyBufferRectTest, WhenCopyingBufferRect3DThenInterfaceDescriptorDataIsCorrect) {
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueCopyBufferRect3D<FamilyType>();

    auto *cmdSBA = (STATE_BASE_ADDRESS *)cmdStateBaseAddress;
    auto &idd = *(INTERFACE_DESCRIPTOR_DATA *)cmdInterfaceDescriptorData;

    // Validate the kernel start pointer.  Technically, a kernel can start at address 0 but let's force a value.
    auto kernelStartPointer = ((uint64_t)idd.getKernelStartPointerHigh() << 32) + idd.getKernelStartPointer();
    EXPECT_LE(kernelStartPointer, cmdSBA->getInstructionBufferSize() * MemoryConstants::pageSize);

    EXPECT_NE(0u, idd.getNumberOfThreadsInGpgpuThreadGroup());
    EXPECT_NE(0u, idd.getCrossThreadConstantDataReadLength());
    EXPECT_NE(0u, idd.getConstantIndirectUrbEntryReadLength());
}

HWTEST2_F(EnqueueCopyBufferRectTest, WhenCopyingBufferRect3DThenNumberOfPipelineSelectsIsOne, IsAtMostXeCore) {
    enqueueCopyBufferRect3D<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueCopyBufferRectTest, WhenCopyingBufferRect3DThenMediaVfeStateIsSetCorrectly) {
    enqueueCopyBufferRect3D<FamilyType>();
    validateMediaVFEState<FamilyType>(&pDevice->getHardwareInfo(), cmdMediaVfeState, cmdList, itorMediaVfeState);
}

struct EnqueueCopyBufferRectHw : public ::testing::Test {
    void SetUp() override {
        if (is32bit) {
            GTEST_SKIP();
        }
        device = std::make_unique<MockClDevice>(MockClDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
        context.reset(new MockContext(device.get()));
        dstBuffer = std::unique_ptr<Buffer>(BufferHelper<EnqueueCopyBufferRectTest::BufferRect>::create(context.get()));
    }

    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
    MockBuffer srcBuffer;
    std::unique_ptr<Buffer> dstBuffer;
    const size_t rowPitch = 100;
    const size_t slicePitch = 100 * 100;
    std::array<size_t, 3> srcOrigin = {{0, 0, 0}};
    std::array<size_t, 3> dstOrigin = {{0, 0, 0}};
    std::array<size_t, 3> region = {{50, 50, 1}};
    uint64_t bigSize = 4ull * MemoryConstants::gigaByte;
    uint64_t smallSize = 4ull * MemoryConstants::gigaByte - 1;

  protected:
    template <typename FamilyType>
    cl_int enqueueCopyBufferRectHw(CommandQueueHw<FamilyType> *cmdQ) {

        auto retVal = CL_SUCCESS;

        retVal = clEnqueueCopyBufferRect(
            cmdQ,
            &srcBuffer,
            dstBuffer.get(),
            srcOrigin.data(),
            dstOrigin.data(),
            region.data(),
            rowPitch,
            slicePitch,
            rowPitch,
            slicePitch,
            0,
            nullptr,
            nullptr);

        return retVal;
    }
};

using EnqueueCopyBufferRectStateless = EnqueueCopyBufferRectHw;

HWTEST_F(EnqueueCopyBufferRectStateless, GivenValidParametersWhenCopyingBufferRectStatelessThenSuccessIsReturned) {

    std::unique_ptr<CommandQueueHw<FamilyType>> cmdQ(new CommandQueueStateless<FamilyType>(context.get(), device.get()));
    srcBuffer.size = static_cast<size_t>(bigSize);
    auto retVal = enqueueCopyBufferRectHw(cmdQ.get());
    EXPECT_EQ(CL_SUCCESS, retVal);
}

using EnqueueCopyBufferRectStateful = EnqueueCopyBufferRectHw;

HWTEST2_F(EnqueueCopyBufferRectStateful, GivenValidParametersWhenCopyingBufferRectStatefulThenSuccessIsReturned, IsStatefulBufferPreferredForProduct) {

    std::unique_ptr<CommandQueueHw<FamilyType>> cmdQ(new CommandQueueStateful<FamilyType>(context.get(), device.get()));
    if (cmdQ->getHeaplessModeEnabled()) {
        GTEST_SKIP();
    }

    srcBuffer.size = static_cast<size_t>(smallSize);
    auto retVal = enqueueCopyBufferRectHw(cmdQ.get());
    EXPECT_EQ(CL_SUCCESS, retVal);
}
