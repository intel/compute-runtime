/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_in_ops_base.h"
#include "shared/source/gen_common/reg_configs_common.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"
#include "opencl/test/unit_test/command_queue/enqueue_copy_buffer_fixture.h"
#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/gen_common/gen_commands_common_validation.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

#include <memory>

using namespace NEO;

using clEnqueueCopyBufferTests = ApiTests;

HWTEST_F(clEnqueueCopyBufferTests, GivenNullSrcMemObjWhenCopyingBufferThenClInvalidMemObjectErrorIsReturned) {
    MockBuffer dstBuffer{};

    auto retVal = clEnqueueCopyBuffer(
        pCommandQueue,
        nullptr,
        &dstBuffer,
        0,
        0,
        sizeof(float),
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

HWTEST_F(clEnqueueCopyBufferTests, GivenNullDstMemObjWhenCopyingBufferThenClInvalidMemObjectErrorIsReturned) {
    MockBuffer srcBuffer{};

    auto retVal = clEnqueueCopyBuffer(
        pCommandQueue,
        &srcBuffer,
        nullptr,
        0,
        0,
        sizeof(float),
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

HWTEST_F(clEnqueueCopyBufferTests, GivenCorrectArgumentsWhenCopyingBufferThenSuccessIsReturned) {
    MockBuffer srcBuffer{};
    MockBuffer dstBuffer{};

    retVal = clEnqueueCopyBuffer(
        pCommandQueue,
        &srcBuffer,
        &dstBuffer,
        0,
        0,
        128,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEnqueueCopyBufferTests, GivenQueueIncapableWhenCopyingBufferThenInvalidOperationIsReturned) {
    MockBuffer srcBuffer{};
    MockBuffer dstBuffer{};

    this->disableQueueCapabilities(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL);
    retVal = clEnqueueCopyBuffer(
        pCommandQueue,
        &srcBuffer,
        &dstBuffer,
        0,
        0,
        128,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

HWTEST_F(EnqueueCopyBufferTest, GivenInvalidMemoryLocationWhenCopyingBufferThenClInvalidValueErrorIsReturned) {
    auto retVal = clEnqueueCopyBuffer(
        pCmdQ,
        srcBuffer,
        dstBuffer,
        0,
        8,
        128,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = clEnqueueCopyBuffer(
        pCmdQ,
        srcBuffer,
        dstBuffer,
        8,
        0,
        128,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

HWTEST_F(EnqueueCopyBufferTest, WhenCopyingBufferThenTaskCountIsAlignedWithCsr) {
    // this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;

    enqueueCopyBuffer();

    auto cmdQTaskLevel = pCmdQ->taskLevel;
    if (!csr.isUpdateTagFromWaitEnabled()) {
        cmdQTaskLevel++;
    }

    EXPECT_EQ(csr.peekTaskCount(), pCmdQ->taskCount);
    EXPECT_EQ(csr.peekTaskLevel(), cmdQTaskLevel);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueCopyBufferTest, WhenCopyingBufferThenGpgpuWalkerIsCorrect) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    enqueueCopyBufferAndParse<FamilyType>();

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

HWTEST_F(EnqueueCopyBufferTest, WhenCopyingBufferThenTaskLevelIsIncremented) {
    auto taskLevelBefore = pCmdQ->taskLevel;

    enqueueCopyBuffer();
    EXPECT_GT(pCmdQ->taskLevel, taskLevelBefore);
}

HWTEST_F(EnqueueCopyBufferTest, WhenCopyingBufferThenCommandsAreAdded) {
    auto usedCmdBufferBefore = pCS->getUsed();

    enqueueCopyBuffer();
    EXPECT_NE(usedCmdBufferBefore, pCS->getUsed());
}

HWTEST_F(EnqueueCopyBufferTest, GivenGpuHangAndBlockingCallWhenCopyingBufferThenOutOfResourcesIsReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.MakeEachEnqueueBlocking.set(true);

    std::unique_ptr<ClDevice> device(new MockClDevice{MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)});
    cl_queue_properties props = {};

    MockCommandQueueHw<FamilyType> mockCommandQueueHw(&context, device.get(), &props);
    mockCommandQueueHw.waitForAllEnginesReturnValue = WaitStatus::gpuHang;

    const auto enqueueResult = EnqueueCopyBufferHelper::enqueueCopyBuffer(
        &mockCommandQueueHw,
        srcBuffer,
        dstBuffer,
        0,
        0,
        sizeof(float),
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, enqueueResult);
    EXPECT_EQ(1, mockCommandQueueHw.waitForAllEnginesCalledCount);
}

HWTEST_F(EnqueueCopyBufferTest, WhenCopyingBufferThenIndirectDataGetsAdded) {
    auto dshBefore = pDSH->getUsed();
    auto iohBefore = pIOH->getUsed();
    auto sshBefore = pSSH->getUsed();

    enqueueCopyBuffer();

    auto builtInType = EBuiltInOps::adjustBuiltinType<EBuiltInOps::copyBufferToBuffer>(pDevice->getCompilerProductHelper().isForceToStatelessRequired(), pCmdQ->getHeaplessModeEnabled());
    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInType,
                                                                            pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcMemObj = srcBuffer;
    dc.dstMemObj = dstBuffer;
    dc.srcOffset = {EnqueueCopyBufferTraits::srcOffset, 0, 0};
    dc.dstOffset = {EnqueueCopyBufferTraits::dstOffset, 0, 0};
    dc.size = {EnqueueCopyBufferTraits::size, 0, 0};

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    auto kernelDescriptor = &kernel->getKernelInfo().kernelDescriptor;

    EXPECT_TRUE(UnitTestHelper<FamilyType>::evaluateDshUsage(dshBefore, pDSH->getUsed(), kernelDescriptor, rootDeviceIndex));

    auto crossThreadDatSize = kernel->getCrossThreadDataSize();
    auto heaplessEnabled = pDevice->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo);
    auto inlineDataSize = UnitTestHelper<FamilyType>::getInlineDataSize(heaplessEnabled);
    bool crossThreadDataFitsInInlineData = (crossThreadDatSize <= inlineDataSize);

    if (crossThreadDataFitsInInlineData) {
        EXPECT_EQ(iohBefore, pIOH->getUsed());
    } else {
        EXPECT_NE(iohBefore, pIOH->getUsed());
    }

    if (kernel->getKernelInfo().kernelDescriptor.kernelAttributes.bufferAddressingMode == KernelDescriptor::BindfulAndStateless) {
        EXPECT_NE(sshBefore, pSSH->getUsed());
    }
}

HWTEST_F(EnqueueCopyBufferTest, WhenCopyingBufferStatelessThenStatelessKernelIsUsed) {

    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
    auto dstBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferToBufferStateless,
                                                                            pCmdQ->getClDevice());

    ASSERT_NE(nullptr, &builder);
    BuiltinOpParams dc;
    dc.srcMemObj = srcBuffer.get();
    dc.dstMemObj = dstBuffer.get();
    dc.srcOffset = {EnqueueCopyBufferTraits::srcOffset, 0, 0};
    dc.dstOffset = {EnqueueCopyBufferTraits::dstOffset, 0, 0};
    dc.size = {EnqueueCopyBufferTraits::size, 0, 0};

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelAttributes.supportsBuffersBiggerThan4Gb());
    EXPECT_FALSE(kernel->getKernelInfo().getArgDescriptorAt(0).as<ArgDescPointer>().isPureStateful());
}

HWTEST2_F(EnqueueCopyBufferTest, WhenCopyingBufferStatelessHeaplessThenCorrectKernelIsUsed, HeaplessSupportedMatcher) {

    if (is32bit) {
        GTEST_SKIP();
    }

    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
    auto dstBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferToBufferStatelessHeapless,
                                                                            pCmdQ->getClDevice());

    ASSERT_NE(nullptr, &builder);
    BuiltinOpParams dc;
    dc.srcMemObj = srcBuffer.get();
    dc.dstMemObj = dstBuffer.get();
    dc.srcOffset = {EnqueueCopyBufferTraits::srcOffset, 0, 0};
    dc.dstOffset = {EnqueueCopyBufferTraits::dstOffset, 0, 0};
    dc.size = {EnqueueCopyBufferTraits::size, 0, 0};

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
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

HWTEST_F(EnqueueCopyBufferTest, WhenCopyingBufferThenL3ProgrammingIsCorrect) {
    enqueueCopyBufferAndParse<FamilyType>();
    validateL3Programming<FamilyType>(cmdList, itorWalker);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueCopyBufferTest, WhenEnqueueIsDoneThenStateBaseAddressIsProperlyProgrammed) {
    enqueueCopyBufferAndParse<FamilyType>();
    auto &ultCsr = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();

    validateStateBaseAddress<FamilyType>(ultCsr.getMemoryManager()->getInternalHeapBaseAddress(ultCsr.rootDeviceIndex, pIOH->getGraphicsAllocation()->isAllocatedInLocalMemoryPool()),
                                         ultCsr.getMemoryManager()->getInternalHeapBaseAddress(ultCsr.rootDeviceIndex, !gfxCoreHelper.useSystemMemoryPlacementForISA(pDevice->getHardwareInfo())),
                                         pDSH, pIOH, pSSH, itorPipelineSelect, itorWalker, cmdList, 0llu);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueCopyBufferTest, WhenCopyingBufferThenMediaInterfaceDescriptorLoadIsCorrect) {
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueCopyBufferAndParse<FamilyType>();

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

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueCopyBufferTest, WhenCopyingBufferThenInterfaceDescriptorDataIsCorrect) {
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueCopyBufferAndParse<FamilyType>();

    auto cmdIDD = (INTERFACE_DESCRIPTOR_DATA *)cmdInterfaceDescriptorData;
    auto cmdSBA = (STATE_BASE_ADDRESS *)cmdStateBaseAddress;

    // Validate the kernel start pointer.  Technically, a kernel can start at address 0 but let's force a value.
    auto kernelStartPointer = ((uint64_t)cmdIDD->getKernelStartPointerHigh() << 32) + cmdIDD->getKernelStartPointer();
    EXPECT_LE(kernelStartPointer, cmdSBA->getInstructionBufferSize() * MemoryConstants::pageSize);

    EXPECT_NE(0u, cmdIDD->getNumberOfThreadsInGpgpuThreadGroup());
    EXPECT_NE(0u, cmdIDD->getCrossThreadConstantDataReadLength());
    EXPECT_NE(0u, cmdIDD->getConstantIndirectUrbEntryReadLength());
}

HWTEST2_F(EnqueueCopyBufferTest, WhenCopyingBufferThenNumberOfPipelineSelectsIsOne, IsAtMostXeCore) {
    enqueueCopyBufferAndParse<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueCopyBufferTest, WhenCopyingBufferThenMediaVfeStateIsSetCorrectly) {
    enqueueCopyBufferAndParse<FamilyType>();
    validateMediaVFEState<FamilyType>(&pDevice->getHardwareInfo(), cmdMediaVfeState, cmdList, itorMediaVfeState);
}

HWTEST_F(EnqueueCopyBufferTest, WhenCopyingBufferThenArgumentZeroMatchesSourceAddress) {
    enqueueCopyBufferAndParse<FamilyType>();

    // Extract the kernel used

    auto builtInType = EBuiltInOps::adjustBuiltinType<EBuiltInOps::copyBufferToBuffer>(pDevice->getCompilerProductHelper().isForceToStatelessRequired(), pCmdQ->getHeaplessModeEnabled());

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInType,
                                                                            pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcMemObj = srcBuffer;
    dc.dstMemObj = dstBuffer;
    dc.srcOffset = {EnqueueCopyBufferTraits::srcOffset, 0, 0};
    dc.dstOffset = {EnqueueCopyBufferTraits::dstOffset, 0, 0};
    dc.size = {EnqueueCopyBufferTraits::size, 0, 0};

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    // Determine where the argument is
    auto pArgument = (void **)getStatelessArgumentPointer<FamilyType>(kernel->getKernelInfo(), 0u, pCmdQ->getIndirectHeap(IndirectHeap::Type::indirectObject, 0), rootDeviceIndex);
    if (pArgument) {
        EXPECT_TRUE(memoryEqualsPointer(pArgument, static_cast<uintptr_t>(ptrOffset(srcBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddress(), srcBuffer->getOffset()))));
    }
}

HWTEST_F(EnqueueCopyBufferTest, WhenCopyingBufferThenArgumentOneMatchesDestinationAddress) {
    enqueueCopyBufferAndParse<FamilyType>();

    // Extract the kernel used
    auto builtInType = EBuiltInOps::adjustBuiltinType<EBuiltInOps::copyBufferToBuffer>(pDevice->getCompilerProductHelper().isForceToStatelessRequired(), pCmdQ->getHeaplessModeEnabled());
    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInType,
                                                                            pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcMemObj = srcBuffer;
    dc.dstMemObj = dstBuffer;
    dc.srcOffset = {EnqueueCopyBufferTraits::srcOffset, 0, 0};
    dc.dstOffset = {EnqueueCopyBufferTraits::dstOffset, 0, 0};
    dc.size = {EnqueueCopyBufferTraits::size, 0, 0};

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    // Determine where the argument is
    auto pArgument = (void **)getStatelessArgumentPointer<FamilyType>(kernel->getKernelInfo(), 1u, pCmdQ->getIndirectHeap(IndirectHeap::Type::indirectObject, 0), rootDeviceIndex);
    if (pArgument) {
        EXPECT_TRUE(memoryEqualsPointer(pArgument, static_cast<uintptr_t>(ptrOffset(dstBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddress(), dstBuffer->getOffset()))));
    }
}

struct EnqueueCopyBufferHw : public ::testing::Test {

    void SetUp() override {
        if (is32bit) {
            GTEST_SKIP();
        }
        device = std::make_unique<MockClDevice>(MockClDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
        context.reset(new MockContext(device.get()));
        dstBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create(context.get()));
    }

    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
    std::unique_ptr<Buffer> dstBuffer;
    MockBuffer srcBuffer;
    uint64_t bigSize = 4ull * MemoryConstants::gigaByte;
    uint64_t smallSize = 4ull * MemoryConstants::gigaByte - 1;
};

using EnqueueCopyBufferStatelessTest = EnqueueCopyBufferHw;

HWTEST_F(EnqueueCopyBufferStatelessTest, givenBuffersWhenCopyingBufferStatelessThenSuccessIsReturned) {
    auto cmdQ = std::make_unique<CommandQueueStateless<FamilyType>>(context.get(), device.get());
    srcBuffer.size = static_cast<size_t>(bigSize);
    auto retVal = cmdQ->enqueueCopyBuffer(
        &srcBuffer,
        dstBuffer.get(),
        0,
        0,
        sizeof(float),
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

using EnqueueCopyBufferStatefulTest = EnqueueCopyBufferHw;

HWTEST2_F(EnqueueCopyBufferStatefulTest, givenBuffersWhenCopyingBufferStatefulThenSuccessIsReturned, IsStatefulBufferPreferredForProduct) {
    auto cmdQ = std::make_unique<CommandQueueStateful<FamilyType>>(context.get(), device.get());
    if (cmdQ->getHeaplessModeEnabled()) {
        GTEST_SKIP();
    }

    srcBuffer.size = static_cast<size_t>(smallSize);
    auto retVal = cmdQ->enqueueCopyBuffer(
        &srcBuffer,
        dstBuffer.get(),
        0,
        0,
        sizeof(float),
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}
