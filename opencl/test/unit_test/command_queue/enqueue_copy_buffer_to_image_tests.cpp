/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/test/unit_test/command_queue/enqueue_copy_buffer_to_image_fixture.h"
#include "opencl/test/unit_test/fixtures/one_mip_level_image_fixture.h"
#include "opencl/test/unit_test/gen_common/gen_commands_common_validation.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_builtin_dispatch_info_builder.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

#include "reg_configs_common.h"

#include <algorithm>

using namespace NEO;

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueCopyBufferToImageTest, WhenCopyingBufferToImageThenGpgpuWalkerIsCorrect) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    enqueueCopyBufferToImage<FamilyType>();

    auto *cmd = reinterpret_cast<GPGPU_WALKER *>(cmdWalker);
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

HWTEST_F(EnqueueCopyBufferToImageTest, WhenCopyingBufferToImageThenTaskCountIsAlignedWithCsr) {
    //this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;

    EnqueueCopyBufferToImageHelper<>::enqueueCopyBufferToImage(pCmdQ, srcBuffer, dstImage);
    EXPECT_EQ(csr.peekTaskCount(), pCmdQ->taskCount);
    EXPECT_EQ(csr.peekTaskLevel(), pCmdQ->taskLevel + 1);
}

HWTEST_F(EnqueueCopyBufferToImageTest, WhenCopyingBufferToImageThenTaskLevelIsIncremented) {
    auto taskLevelBefore = pCmdQ->taskLevel;

    EnqueueCopyBufferToImageHelper<>::enqueueCopyBufferToImage(pCmdQ, srcBuffer, dstImage);
    EXPECT_GT(pCmdQ->taskLevel, taskLevelBefore);
}

HWTEST_F(EnqueueCopyBufferToImageTest, WhenCopyingBufferToImageThenCommandsAreAdded) {
    auto usedCmdBufferBefore = pCS->getUsed();

    EnqueueCopyBufferToImageHelper<>::enqueueCopyBufferToImage(pCmdQ, srcBuffer, dstImage);
    EXPECT_NE(usedCmdBufferBefore, pCS->getUsed());
}

HWTEST_F(EnqueueCopyBufferToImageTest, WhenCopyingBufferToImageThenIndirectDataGetsAdded) {
    auto dshBefore = pDSH->getUsed();
    auto iohBefore = pIOH->getUsed();
    auto sshBefore = pSSH->getUsed();

    EnqueueCopyBufferToImageHelper<>::enqueueCopyBufferToImage(pCmdQ, srcBuffer, dstImage);
    EXPECT_TRUE(UnitTestHelper<FamilyType>::evaluateDshUsage(dshBefore, pDSH->getUsed(), nullptr, rootDeviceIndex));
    EXPECT_NE(iohBefore, pIOH->getUsed());
    EXPECT_NE(sshBefore, pSSH->getUsed());
}

HWTEST_F(EnqueueCopyBufferToImageTest, WhenCopyingBufferToImageThenL3ProgrammingIsCorrect) {
    enqueueCopyBufferToImage<FamilyType>();
    validateL3Programming<FamilyType>(cmdList, itorWalker);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueCopyBufferToImageTest, WhenEnqueueIsDoneThenStateBaseAddressIsProperlyProgrammed) {
    enqueueCopyBufferToImage<FamilyType>();
    auto &ultCsr = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto &hwHelper = HwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);

    validateStateBaseAddress<FamilyType>(ultCsr.getMemoryManager()->getInternalHeapBaseAddress(ultCsr.rootDeviceIndex, pIOH->getGraphicsAllocation()->isAllocatedInLocalMemoryPool()),
                                         ultCsr.getMemoryManager()->getInternalHeapBaseAddress(ultCsr.rootDeviceIndex, !hwHelper.useSystemMemoryPlacementForISA(pDevice->getHardwareInfo())),
                                         pDSH, pIOH, pSSH, itorPipelineSelect, itorWalker, cmdList, 0llu);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueCopyBufferToImageTest, WhenCopyingBufferToImageThenMediaInterfaceDescriptorLoadIsCorrect) {
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueCopyBufferToImage<FamilyType>();

    // All state should be programmed before walker
    auto cmd = reinterpret_cast<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(cmdMediaInterfaceDescriptorLoad);
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
    FamilyType::PARSE::template validateCommand<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(cmdList.begin(), itorMediaInterfaceDescriptorLoad);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueCopyBufferToImageTest, WhenCopyingBufferToImageThenInterfaceDescriptorDataIsCorrect) {
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueCopyBufferToImage<FamilyType>();

    // Extract the interfaceDescriptorData
    auto cmdSBA = (STATE_BASE_ADDRESS *)cmdStateBaseAddress;
    auto &interfaceDescriptorData = *(INTERFACE_DESCRIPTOR_DATA *)cmdInterfaceDescriptorData;

    // Validate the kernel start pointer.  Technically, a kernel can start at address 0 but let's force a value.
    auto kernelStartPointer = ((uint64_t)interfaceDescriptorData.getKernelStartPointerHigh() << 32) + interfaceDescriptorData.getKernelStartPointer();
    EXPECT_LE(kernelStartPointer, cmdSBA->getInstructionBufferSize() * MemoryConstants::pageSize);

    size_t maxLocalSize = 256u;
    auto localWorkSize = std::min(
        maxLocalSize, Image2dDefaults::imageDesc.image_width * Image2dDefaults::imageDesc.image_height);
    auto simd = 32u;
    auto numThreadsPerThreadGroup = Math::divideAndRoundUp(localWorkSize, simd);
    EXPECT_EQ(numThreadsPerThreadGroup, interfaceDescriptorData.getNumberOfThreadsInGpgpuThreadGroup());
    EXPECT_NE(0u, interfaceDescriptorData.getCrossThreadConstantDataReadLength());
    EXPECT_NE(0u, interfaceDescriptorData.getConstantIndirectUrbEntryReadLength());

    // We shouldn't have these pointers the same.
    EXPECT_NE(kernelStartPointer, interfaceDescriptorData.getBindingTablePointer());
}

HWTEST_F(EnqueueCopyBufferToImageTest, WhenCopyingBufferToImageThenSurfaceStateIsCorrect) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);
    VariableBackup<CommandQueue *> cmdQBackup(&pCmdQ, mockCmdQ.get());
    mockCmdQ->storeMultiDispatchInfo = true;
    enqueueCopyBufferToImage<FamilyType>();

    const auto &kernelInfo = mockCmdQ->storedMultiDispatchInfo.begin()->getKernel()->getKernelInfo();
    uint32_t index = static_cast<uint32_t>(kernelInfo.getArgDescriptorAt(1).template as<ArgDescImage>().bindful) / sizeof(RENDER_SURFACE_STATE);

    const auto surfaceState = getSurfaceState<FamilyType>(&pCmdQ->getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 0), index);
    const auto &imageDesc = dstImage->getImageDesc();
    // EnqueueReadImage uses multi-byte copies depending on per-pixel-size-in-bytes
    EXPECT_EQ(imageDesc.image_width, surfaceState->getWidth());
    EXPECT_EQ(imageDesc.image_height, surfaceState->getHeight());
    EXPECT_NE(0u, surfaceState->getSurfacePitch());
    EXPECT_NE(0u, surfaceState->getSurfaceType());
    auto surfaceFormat = surfaceState->getSurfaceFormat();
    bool isRedescribedFormat =
        surfaceFormat == RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32A32_UINT ||
        surfaceFormat == RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32_UINT ||
        surfaceFormat == RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_UINT ||
        surfaceFormat == RENDER_SURFACE_STATE::SURFACE_FORMAT_R16_UINT ||
        surfaceFormat == RENDER_SURFACE_STATE::SURFACE_FORMAT_R8_UINT;
    EXPECT_TRUE(isRedescribedFormat);
    EXPECT_EQ(MockGmmResourceInfo::getHAlignSurfaceStateResult, surfaceState->getSurfaceHorizontalAlignment());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT_VALIGN_4, surfaceState->getSurfaceVerticalAlignment());
    EXPECT_EQ(dstImage->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddress(), surfaceState->getSurfaceBaseAddress());
}

HWTEST2_F(EnqueueCopyBufferToImageTest, WhenCopyingBufferToImageThenNumberOfPipelineSelectsIsOne, IsAtMostXeHpcCore) {
    enqueueCopyBufferToImage<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueCopyBufferToImageTest, WhenCopyingBufferToImageThenMediaVfeStateIsSetCorrectly) {
    enqueueCopyBufferToImage<FamilyType>();
    validateMediaVFEState<FamilyType>(&pDevice->getHardwareInfo(), cmdMediaVfeState, cmdList, itorMediaVfeState);
}

typedef EnqueueCopyBufferToImageMipMapTest MipMapCopyBufferToImageTest;

HWTEST_P(MipMapCopyBufferToImageTest, GivenImageWithMipLevelNonZeroWhenCopyBufferToImageIsCalledThenProperMipLevelIsSet) {
    auto imageType = (cl_mem_object_type)GetParam();
    auto builtIns = new MockBuiltins();
    pCmdQ->getDevice().getExecutionEnvironment()->rootDeviceEnvironments[pCmdQ->getDevice().getRootDeviceIndex()]->builtins.reset(builtIns);
    auto &origBuilder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(
        EBuiltInOps::CopyBufferToImage3d,
        pCmdQ->getClDevice());

    // substitute original builder with mock builder
    auto oldBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        EBuiltInOps::CopyBufferToImage3d,
        std::unique_ptr<NEO::BuiltinDispatchInfoBuilder>(new MockBuiltinDispatchInfoBuilder(*builtIns, pCmdQ->getClDevice(), &origBuilder)));

    cl_int retVal = CL_SUCCESS;
    cl_image_desc imageDesc = {};
    uint32_t expectedMipLevel = 3;
    imageDesc.image_type = imageType;
    imageDesc.num_mip_levels = 10;
    imageDesc.image_width = 4;
    imageDesc.image_height = 1;
    imageDesc.image_depth = 1;
    size_t origin[] = {0, 0, 0, 0};
    size_t region[] = {imageDesc.image_width, 1, 1};
    std::unique_ptr<Image> image;
    switch (imageType) {
    case CL_MEM_OBJECT_IMAGE1D:
        origin[1] = expectedMipLevel;
        image = std::unique_ptr<Image>(ImageHelper<Image1dDefaults>::create(context, &imageDesc));
        break;
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
        imageDesc.image_array_size = 2;
        origin[2] = expectedMipLevel;
        image = std::unique_ptr<Image>(ImageHelper<Image1dArrayDefaults>::create(context, &imageDesc));
        break;
    case CL_MEM_OBJECT_IMAGE2D:
        origin[2] = expectedMipLevel;
        image = std::unique_ptr<Image>(ImageHelper<Image2dDefaults>::create(context, &imageDesc));
        break;
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        imageDesc.image_array_size = 2;
        origin[3] = expectedMipLevel;
        image = std::unique_ptr<Image>(ImageHelper<Image2dArrayDefaults>::create(context, &imageDesc));
        break;
    case CL_MEM_OBJECT_IMAGE3D:
        origin[3] = expectedMipLevel;
        image = std::unique_ptr<Image>(ImageHelper<Image3dDefaults>::create(context, &imageDesc));
        break;
    }
    EXPECT_NE(nullptr, image.get());

    std::unique_ptr<uint32_t[]> ptr = std::unique_ptr<uint32_t[]>(new uint32_t[3]);
    retVal = pCmdQ->enqueueCopyBufferToImage(srcBuffer,
                                             image.get(),
                                             0,
                                             origin,
                                             region,
                                             0,
                                             nullptr,
                                             nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto &mockBuilder = static_cast<MockBuiltinDispatchInfoBuilder &>(BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToImage3d,
                                                                                                                              pCmdQ->getClDevice()));
    auto params = mockBuilder.getBuiltinOpParams();

    EXPECT_EQ(expectedMipLevel, params->dstMipLevel);

    // restore original builder and retrieve mock builder
    auto newBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        EBuiltInOps::CopyBufferToImage3d,
        std::move(oldBuilder));
    EXPECT_NE(nullptr, newBuilder);
}

INSTANTIATE_TEST_CASE_P(MipMapCopyBufferToImageTest_GivenImageWithMipLevelNonZeroWhenCopyBufferToImageIsCalledThenProperMipLevelIsSet,
                        MipMapCopyBufferToImageTest, ::testing::Values(CL_MEM_OBJECT_IMAGE1D, CL_MEM_OBJECT_IMAGE1D_ARRAY, CL_MEM_OBJECT_IMAGE2D, CL_MEM_OBJECT_IMAGE2D_ARRAY, CL_MEM_OBJECT_IMAGE3D));

struct EnqueueCopyBufferToImageHw : public ::testing::Test {

    void SetUp() override {
        REQUIRE_64BIT_OR_SKIP();
        REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);
        device = std::make_unique<MockClDevice>(MockClDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
        context = std::make_unique<MockContext>(device.get());
        dstImage = std::unique_ptr<Image>(Image2dHelper<>::create(context.get()));
    }

    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
    std::unique_ptr<Image> dstImage;
    MockBuffer srcBuffer;
    uint64_t bigSize = 5ull * MemoryConstants::gigaByte;
    uint64_t smallSize = 4ull * MemoryConstants::gigaByte - 1;
    uint64_t bigOffset = 4ull * MemoryConstants::gigaByte;

    const size_t dstOrigin[3] = {0, 0, 0};
    const size_t region[3] = {4, 1, 1};
};

using EnqueueCopyBufferToImageStatelessTest = EnqueueCopyBufferToImageHw;

HWTEST_F(EnqueueCopyBufferToImageStatelessTest, givenBigBufferWhenCopyingBufferToImageStatelessThenSuccessIsReturned) {
    auto cmdQ = std::make_unique<CommandQueueStateless<FamilyType>>(context.get(), device.get());
    srcBuffer.size = static_cast<size_t>(bigSize);
    auto retVal = cmdQ->enqueueCopyBufferToImage(
        &srcBuffer,
        dstImage.get(),
        static_cast<size_t>(bigOffset),
        dstOrigin,
        region,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(EnqueueCopyBufferToImageStatelessTest, givenGpuHangAndBlockingCallAndBigBufferWhenCopyingBufferToImageStatelessThenOutOfResourcesIsReturned) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.MakeEachEnqueueBlocking.set(true);

    std::unique_ptr<ClDevice> device(new MockClDevice{MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)});
    cl_queue_properties props = {};

    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context.get(), device.get(), &props);
    mockCommandQueueHw.waitForAllEnginesReturnValue = WaitStatus::GpuHang;

    srcBuffer.size = static_cast<size_t>(bigSize);

    const auto enqueueResult = mockCommandQueueHw.enqueueCopyBufferToImage(
        &srcBuffer,
        dstImage.get(),
        static_cast<size_t>(bigOffset),
        dstOrigin,
        region,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, enqueueResult);
    EXPECT_EQ(1, mockCommandQueueHw.waitForAllEnginesCalledCount);
}

using EnqueueCopyBufferToImageStatefulTest = EnqueueCopyBufferToImageHw;

HWTEST_F(EnqueueCopyBufferToImageStatefulTest, givenBigBufferWhenCopyingBufferToImageStatefulThenSuccessIsReturned) {
    auto cmdQ = std::make_unique<CommandQueueStateful<FamilyType>>(context.get(), device.get());
    srcBuffer.size = static_cast<size_t>(smallSize);
    auto retVal = cmdQ->enqueueCopyBufferToImage(
        &srcBuffer,
        dstImage.get(),
        0,
        dstOrigin,
        region,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

using OneMipLevelCopyBufferToImageImageTests = Test<OneMipLevelImageFixture>;

HWTEST_F(OneMipLevelCopyBufferToImageImageTests, GivenNotMippedImageWhenCopyingBufferToImageThenDoNotProgramDestinationMipLevel) {
    auto srcBuffer = std::unique_ptr<Buffer>(createBuffer());
    auto queue = createQueue<FamilyType>();
    auto retVal = queue->enqueueCopyBufferToImage(
        srcBuffer.get(),
        image.get(),
        0u,
        origin,
        region,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(builtinOpsParamsCaptured);
    EXPECT_EQ(0u, usedBuiltinOpsParams.dstMipLevel);
}
