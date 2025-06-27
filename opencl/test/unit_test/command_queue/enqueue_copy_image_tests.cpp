/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen_common/reg_configs_common.h"
#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/test/unit_test/command_queue/enqueue_copy_image_fixture.h"
#include "opencl/test/unit_test/fixtures/one_mip_level_image_fixture.h"
#include "opencl/test/unit_test/gen_common/gen_commands_common_validation.h"
#include "opencl/test/unit_test/mocks/mock_builtin_dispatch_info_builder.h"
#include "opencl/test/unit_test/mocks/mock_cl_execution_environment.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

using namespace NEO;

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueCopyImageTest, WhenCopyingImageThenGpgpuWalkerIsCorrect) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    enqueueCopyImage<FamilyType>();

    auto *cmd = reinterpret_cast<GPGPU_WALKER *>(cmdWalker);
    ASSERT_NE(nullptr, cmd);

    // Verify GPGPU_WALKER parameters
    EXPECT_NE(0u, cmd->getThreadGroupIdXDimension());
    EXPECT_NE(0u, cmd->getThreadGroupIdYDimension());
    EXPECT_NE(0u, cmd->getThreadGroupIdZDimension());
    EXPECT_NE(0u, cmd->getRightExecutionMask());
    EXPECT_NE(0u, cmd->getBottomExecutionMask());
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

HWTEST_F(EnqueueCopyImageTest, WhenCopyingImageThenTaskCountIsAlignedWithCsr) {
    // this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;

    EnqueueCopyImageHelper<>::enqueueCopyImage(pCmdQ, srcImage, dstImage);
    EXPECT_EQ(csr.peekTaskCount(), pCmdQ->taskCount);

    auto cmdQTaskLevel = pCmdQ->taskLevel;
    if (!csr.isUpdateTagFromWaitEnabled()) {
        cmdQTaskLevel++;
    }

    EXPECT_EQ(csr.peekTaskLevel(), cmdQTaskLevel);
}

HWTEST_F(EnqueueCopyImageTest, GivenGpuHangAndBlockingCallWhenCopyingImageThenOutOfResourcesIsReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.MakeEachEnqueueBlocking.set(true);

    std::unique_ptr<ClDevice> device(new MockClDevice{MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)});
    cl_queue_properties props = {};

    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context, device.get(), &props);
    mockCommandQueueHw.waitForAllEnginesReturnValue = WaitStatus::gpuHang;

    const auto enqueueResult = EnqueueCopyImageHelper<>::enqueueCopyImage(&mockCommandQueueHw, srcImage, dstImage);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, enqueueResult);
    EXPECT_EQ(1, mockCommandQueueHw.waitForAllEnginesCalledCount);
}

HWTEST_F(EnqueueCopyImageTest, WhenCopyingImageThenTaskLevelIsIncremented) {
    auto taskLevelBefore = pCmdQ->taskLevel;

    EnqueueCopyImageHelper<>::enqueueCopyImage(pCmdQ, srcImage, dstImage);
    EXPECT_GT(pCmdQ->taskLevel, taskLevelBefore);
}

HWTEST_F(EnqueueCopyImageTest, WhenCopyingImageThenCommandsAreAdded) {
    auto usedCmdBufferBefore = pCS->getUsed();

    EnqueueCopyImageHelper<>::enqueueCopyImage(pCmdQ, srcImage, dstImage);
    EXPECT_NE(usedCmdBufferBefore, pCS->getUsed());
}

HWTEST_F(EnqueueCopyImageTest, WhenCopyingImageThenIndirectDataGetsAdded) {
    auto dshBefore = pDSH->getUsed();
    auto iohBefore = pIOH->getUsed();
    auto sshBefore = pSSH->getUsed();

    EnqueueCopyImageHelper<>::enqueueCopyImage(pCmdQ, srcImage, dstImage);
    EXPECT_TRUE(UnitTestHelper<FamilyType>::evaluateDshUsage(dshBefore, pDSH->getUsed(), nullptr, rootDeviceIndex));
    EXPECT_NE(iohBefore, pIOH->getUsed());
    EXPECT_NE(sshBefore, pSSH->getUsed());
}

HWTEST_F(EnqueueCopyImageTest, WhenCopyingImageThenL3ProgrammingIsCorrect) {
    enqueueCopyImage<FamilyType>();
    validateL3Programming<FamilyType>(cmdList, itorWalker);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueCopyImageTest, WhenEnqueueIsDoneThenStateBaseAddressIsProperlyProgrammed) {
    enqueueCopyImage<FamilyType>();
    auto &ultCsr = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();

    validateStateBaseAddress<FamilyType>(ultCsr.getMemoryManager()->getInternalHeapBaseAddress(ultCsr.rootDeviceIndex, pIOH->getGraphicsAllocation()->isAllocatedInLocalMemoryPool()),
                                         ultCsr.getMemoryManager()->getInternalHeapBaseAddress(ultCsr.rootDeviceIndex, !gfxCoreHelper.useSystemMemoryPlacementForISA(pDevice->getHardwareInfo())),
                                         pDSH, pIOH, pSSH, itorPipelineSelect, itorWalker, cmdList, 0llu);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueCopyImageTest, WhenCopyingImageThenMediaInterfaceDescriptorLoadIsCorrect) {
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueCopyImage<FamilyType>();

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
    FamilyType::Parse::template validateCommand<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(cmdList.begin(), itorMediaInterfaceDescriptorLoad);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueCopyImageTest, WhenCopyingImageThenInterfaceDescriptorDataIsCorrect) {
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueCopyImage<FamilyType>();

    // Extract the interfaceDescriptorData
    auto cmdSBA = (STATE_BASE_ADDRESS *)cmdStateBaseAddress;
    auto &interfaceDescriptorData = *(INTERFACE_DESCRIPTOR_DATA *)cmdInterfaceDescriptorData;

    // Validate the kernel start pointer.  Technically, a kernel can start at address 0 but let's force a value.
    auto kernelStartPointer = ((uint64_t)interfaceDescriptorData.getKernelStartPointerHigh() << 32) + interfaceDescriptorData.getKernelStartPointer();
    EXPECT_LE(kernelStartPointer, cmdSBA->getInstructionBufferSize() * MemoryConstants::pageSize);

    EXPECT_NE(0u, interfaceDescriptorData.getCrossThreadConstantDataReadLength());
    EXPECT_NE(0u, interfaceDescriptorData.getConstantIndirectUrbEntryReadLength());

    // We shouldn't have these pointers the same.
    EXPECT_NE(kernelStartPointer, interfaceDescriptorData.getBindingTablePointer());
}

HWTEST_F(EnqueueCopyImageTest, WhenCopyingImageThenSurfaceStateIsCorrect) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);
    VariableBackup<CommandQueue *> cmdQBackup(&pCmdQ, mockCmdQ.get());
    mockCmdQ->storeMultiDispatchInfo = true;

    enqueueCopyImage<FamilyType>();

    for (uint32_t i = 0; i < 2; ++i) {
        const auto surfaceState = SurfaceStateAccessor::getSurfaceState<FamilyType>(mockCmdQ, i);

        const auto &imageDesc = dstImage->getImageDesc();
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
    }

    const auto srcSurfaceState = SurfaceStateAccessor::getSurfaceState<FamilyType>(mockCmdQ, 0);
    EXPECT_EQ(srcImage->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddress(), srcSurfaceState->getSurfaceBaseAddress());

    const auto dstSurfaceState = SurfaceStateAccessor::getSurfaceState<FamilyType>(mockCmdQ, 1);
    EXPECT_EQ(dstImage->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddress(), dstSurfaceState->getSurfaceBaseAddress());
}

HWTEST2_F(EnqueueCopyImageTest, WhenCopyingImageThenNumberOfPipelineSelectsIsOne, IsAtMostXeCore) {
    enqueueCopyImage<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueCopyImageTest, WhenCopyingImageThenMediaVfeStateIsSetCorrectly) {
    enqueueCopyImage<FamilyType>();
    validateMediaVFEState<FamilyType>(&pDevice->getHardwareInfo(), cmdMediaVfeState, cmdList, itorMediaVfeState);
}

using MipMapCopyImageTest = EnqueueCopyImageMipMapTest;

HWTEST_P(MipMapCopyImageTest, GivenImagesWithNonZeroMipLevelsWhenCopyImageIsCalledThenProperMipLevelsAreSet) {
    USE_REAL_FILE_SYSTEM();
    bool heaplessAllowed = UnitTestHelper<FamilyType>::isHeaplessAllowed();

    bool useHeapless = false;
    cl_mem_object_type srcImageType, dstImageType;
    std::tie(srcImageType, dstImageType, useHeapless) = GetParam();

    if (useHeapless && !heaplessAllowed) {
        return;
    }

    reinterpret_cast<MockCommandQueueHw<FamilyType> *>(pCmdQ)->heaplessModeEnabled = useHeapless;
    auto builtInType = EBuiltInOps::adjustImageBuiltinType<EBuiltInOps::copyImageToImage3d>(useHeapless);

    auto builtIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(pCmdQ->getDevice().getExecutionEnvironment()->rootDeviceEnvironments[pCmdQ->getDevice().getRootDeviceIndex()].get(), builtIns);
    auto &origBuilder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(
        builtInType,
        pCmdQ->getClDevice());
    // substitute original builder with mock builder
    auto oldBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        builtInType,
        std::unique_ptr<NEO::BuiltinDispatchInfoBuilder>(new MockBuiltinDispatchInfoBuilder(*builtIns, pCmdQ->getClDevice(), &origBuilder)));

    cl_int retVal = CL_SUCCESS;
    cl_image_desc srcImageDesc = {};
    uint32_t expectedSrcMipLevel = 3;
    uint32_t expectedDstMipLevel = 4;
    srcImageDesc.image_type = srcImageType;
    srcImageDesc.num_mip_levels = 10;
    srcImageDesc.image_width = 4;
    srcImageDesc.image_height = 1;
    srcImageDesc.image_depth = 1;

    cl_image_desc dstImageDesc = srcImageDesc;
    dstImageDesc.image_type = dstImageType;

    size_t srcOrigin[] = {0, 0, 0, 0};
    size_t dstOrigin[] = {0, 0, 0, 0};
    size_t region[] = {srcImageDesc.image_width, 1, 1};
    std::unique_ptr<Image> srcImage;
    std::unique_ptr<Image> dstImage;

    switch (srcImageType) {
    case CL_MEM_OBJECT_IMAGE1D:
        srcOrigin[1] = expectedSrcMipLevel;
        srcImage = std::unique_ptr<Image>(ImageHelperUlt<Image1dDefaults>::create(context, &srcImageDesc));
        break;
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
        srcImageDesc.image_array_size = 2;
        srcOrigin[2] = expectedSrcMipLevel;
        srcImage = std::unique_ptr<Image>(ImageHelperUlt<Image1dArrayDefaults>::create(context, &srcImageDesc));
        break;
    case CL_MEM_OBJECT_IMAGE2D:
        srcOrigin[2] = expectedSrcMipLevel;
        srcImage = std::unique_ptr<Image>(ImageHelperUlt<Image2dDefaults>::create(context, &srcImageDesc));
        break;
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        srcImageDesc.image_array_size = 2;
        srcOrigin[3] = expectedSrcMipLevel;
        srcImage = std::unique_ptr<Image>(ImageHelperUlt<Image2dArrayDefaults>::create(context, &srcImageDesc));
        break;
    case CL_MEM_OBJECT_IMAGE3D:
        srcOrigin[3] = expectedSrcMipLevel;
        srcImage = std::unique_ptr<Image>(ImageHelperUlt<Image3dDefaults>::create(context, &srcImageDesc));
        break;
    }

    EXPECT_NE(nullptr, srcImage.get());

    switch (dstImageType) {
    case CL_MEM_OBJECT_IMAGE1D:
        dstOrigin[1] = expectedDstMipLevel;
        dstImage = std::unique_ptr<Image>(ImageHelperUlt<Image1dDefaults>::create(context, &dstImageDesc));
        break;
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
        dstImageDesc.image_array_size = 2;
        dstOrigin[2] = expectedDstMipLevel;
        dstImage = std::unique_ptr<Image>(ImageHelperUlt<Image1dArrayDefaults>::create(context, &dstImageDesc));
        break;
    case CL_MEM_OBJECT_IMAGE2D:
        dstOrigin[2] = expectedDstMipLevel;
        dstImage = std::unique_ptr<Image>(ImageHelperUlt<Image2dDefaults>::create(context, &dstImageDesc));
        break;
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        dstOrigin[3] = expectedDstMipLevel;
        dstImage = std::unique_ptr<Image>(ImageHelperUlt<Image2dArrayDefaults>::create(context, &dstImageDesc));
        break;
    case CL_MEM_OBJECT_IMAGE3D:
        dstOrigin[3] = expectedDstMipLevel;
        dstImage = std::unique_ptr<Image>(ImageHelperUlt<Image3dDefaults>::create(context, &dstImageDesc));
        break;
    }

    EXPECT_NE(nullptr, dstImage.get());

    retVal = pCmdQ->enqueueCopyImage(srcImage.get(),
                                     dstImage.get(),
                                     srcOrigin,
                                     dstOrigin,
                                     region,
                                     0,
                                     nullptr,
                                     nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto &mockBuilder = static_cast<MockBuiltinDispatchInfoBuilder &>(BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInType,
                                                                                                                              pCmdQ->getClDevice()));
    auto params = mockBuilder.getBuiltinOpParams();

    EXPECT_EQ(expectedSrcMipLevel, params->srcMipLevel);
    EXPECT_EQ(expectedDstMipLevel, params->dstMipLevel);

    // restore original builder and retrieve mock builder
    auto newBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        builtInType,
        std::move(oldBuilder));
    EXPECT_NE(nullptr, newBuilder);
}

uint32_t types[] = {CL_MEM_OBJECT_IMAGE1D, CL_MEM_OBJECT_IMAGE1D_ARRAY, CL_MEM_OBJECT_IMAGE2D, CL_MEM_OBJECT_IMAGE2D_ARRAY, CL_MEM_OBJECT_IMAGE3D};

INSTANTIATE_TEST_SUITE_P(MipMapCopyImageTest_GivenImagesWithNonZeroMipLevelsWhenCopyImageIsCalledThenProperMipLevelsAreSet,
                         MipMapCopyImageTest,
                         ::testing::Combine(
                             ::testing::ValuesIn(types),
                             ::testing::ValuesIn(types),
                             ::testing::Values(false, true)));

using OneMipLevelCopyImageImageTests = Test<OneMipLevelImageFixture>;

HWTEST_F(OneMipLevelCopyImageImageTests, GivenNotMippedImageWhenCopyingImageThenDoNotProgramSourceAndDestinationMipLevels) {
    auto dstImage = std::unique_ptr<Image>(createImage());
    auto queue = createQueue<FamilyType>();
    auto retVal = queue->enqueueCopyImage(
        image.get(),
        dstImage.get(),
        origin,
        origin,
        region,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(builtinOpsParamsCaptured);
    EXPECT_EQ(0u, usedBuiltinOpsParams.srcMipLevel);
    EXPECT_EQ(0u, usedBuiltinOpsParams.dstMipLevel);
}

HWTEST_F(EnqueueCopyImageTest, WhenCopyImage1dBufferToImage1dBufferThenCorrectBuitInIsSelected) {
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);
    VariableBackup<CommandQueue *> cmdQBackup(&pCmdQ, mockCmdQ.get());
    std::unique_ptr<Image> srcImage1dBuffer;
    srcImage1dBuffer.reset(Image1dBufferHelperUlt<>::create(context));
    VariableBackup<Image *> srcImageBackup(&srcImage, srcImage1dBuffer.get());
    std::unique_ptr<Image> dstImage1dBuffer;
    dstImage1dBuffer.reset(Image1dBufferHelperUlt<>::create(context));
    VariableBackup<Image *> dstImageBackup(&dstImage, dstImage1dBuffer.get());
    mockCmdQ->storeMultiDispatchInfo = true;
    EnqueueCopyImageHelper<>::enqueueCopyImage(pCmdQ, srcImage, dstImage);
    const auto &kernelInfo = mockCmdQ->storedMultiDispatchInfo.begin()->getKernel()->getKernelInfo();
    EXPECT_TRUE(kernelInfo.kernelDescriptor.kernelMetadata.kernelName == "CopyImage1dBufferToImage1dBuffer");
}

HWTEST_F(EnqueueCopyImageTest, WhenCopyImage1dBufferToImageThenCorrectBuitInIsSelected) {
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);
    VariableBackup<CommandQueue *> cmdQBackup(&pCmdQ, mockCmdQ.get());
    std::unique_ptr<Image> srcImage1dBuffer;
    srcImage1dBuffer.reset(Image1dBufferHelperUlt<>::create(context));
    VariableBackup<Image *> srcImageBackup(&srcImage, srcImage1dBuffer.get());
    mockCmdQ->storeMultiDispatchInfo = true;
    EnqueueCopyImageHelper<>::enqueueCopyImage(pCmdQ, srcImage, dstImage);
    const auto &kernelInfo = mockCmdQ->storedMultiDispatchInfo.begin()->getKernel()->getKernelInfo();
    EXPECT_TRUE(kernelInfo.kernelDescriptor.kernelMetadata.kernelName == "CopyImage1dBufferToImage3d");
}

HWTEST_F(EnqueueCopyImageTest, WhenCopyImageToImage1dBufferThenCorrectBuitInIsSelected) {
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);
    VariableBackup<CommandQueue *> cmdQBackup(&pCmdQ, mockCmdQ.get());
    std::unique_ptr<Image> dstImage1dBuffer;
    dstImage1dBuffer.reset(Image1dBufferHelperUlt<>::create(context));
    VariableBackup<Image *> dstImageBackup(&dstImage, dstImage1dBuffer.get());
    mockCmdQ->storeMultiDispatchInfo = true;
    EnqueueCopyImageHelper<>::enqueueCopyImage(pCmdQ, srcImage, dstImage);
    const auto &kernelInfo = mockCmdQ->storedMultiDispatchInfo.begin()->getKernel()->getKernelInfo();
    EXPECT_TRUE(kernelInfo.kernelDescriptor.kernelMetadata.kernelName == "CopyImage3dToImage1dBuffer");
}

HWTEST_F(EnqueueCopyImageTest, WhenCopyImageToImageThenCorrectBuitInIsSelected) {
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);
    VariableBackup<CommandQueue *> cmdQBackup(&pCmdQ, mockCmdQ.get());
    mockCmdQ->storeMultiDispatchInfo = true;
    EnqueueCopyImageHelper<>::enqueueCopyImage(pCmdQ, srcImage, dstImage);
    const auto &kernelInfo = mockCmdQ->storedMultiDispatchInfo.begin()->getKernel()->getKernelInfo();
    EXPECT_TRUE(kernelInfo.kernelDescriptor.kernelMetadata.kernelName == "CopyImage3dToImage3d");
}
