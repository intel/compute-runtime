/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/helpers/convert_color.h"
#include "opencl/test/unit_test/command_queue/enqueue_fill_image_fixture.h"
#include "opencl/test/unit_test/gen_common/gen_commands_common_validation.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

#include "reg_configs_common.h"

#include <algorithm>

using namespace NEO;

class EnqueueFillImageTest : public EnqueueFillImageTestFixture,
                             public ::testing::Test {
  public:
    void SetUp(void) override {
        EnqueueFillImageTestFixture::SetUp();
    }

    void TearDown(void) override {
        EnqueueFillImageTestFixture::TearDown();
    }
};

HWTEST_F(EnqueueFillImageTest, WhenFillingImageThenTaskCountIsAlignedWithCsr) {
    //this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;

    EnqueueFillImageHelper<>::enqueueFillImage(pCmdQ, image);
    EXPECT_EQ(csr.peekTaskCount(), pCmdQ->taskCount);
    EXPECT_EQ(csr.peekTaskLevel(), pCmdQ->taskLevel + 1);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueFillImageTest, WhenFillingImageThenGpgpuWalkerIsCorrect) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    enqueueFillImage<FamilyType>();

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

HWTEST_F(EnqueueFillImageTest, WhenFillingImageThenTaskLevelIsIncremented) {
    auto taskLevelBefore = pCmdQ->taskLevel;

    EnqueueFillImageHelper<>::enqueueFillImage(pCmdQ, image);
    EXPECT_GT(pCmdQ->taskLevel, taskLevelBefore);
}

HWTEST_F(EnqueueFillImageTest, WhenFillingImageThenCommandsAreAdded) {
    auto usedCmdBufferBefore = pCS->getUsed();

    EnqueueFillImageHelper<>::enqueueFillImage(pCmdQ, image);
    EXPECT_NE(usedCmdBufferBefore, pCS->getUsed());
}

HWTEST_F(EnqueueFillImageTest, GivenGpuHangAndBlockingCallWhenFillingImageThenOutOfResourcesIsReturned) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.MakeEachEnqueueBlocking.set(true);

    std::unique_ptr<ClDevice> device(new MockClDevice{MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)});
    cl_queue_properties props = {};

    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context, device.get(), &props);
    mockCommandQueueHw.waitForAllEnginesReturnValue = WaitStatus::GpuHang;

    const auto enqueueResult = EnqueueFillImageHelper<>::enqueueFillImage(&mockCommandQueueHw, image);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, enqueueResult);
    EXPECT_EQ(1, mockCommandQueueHw.waitForAllEnginesCalledCount);
}

HWTEST_F(EnqueueFillImageTest, WhenFillingImageThenIndirectDataGetsAdded) {
    auto dshBefore = pDSH->getUsed();
    auto iohBefore = pIOH->getUsed();
    auto sshBefore = pSSH->getUsed();

    const auto enqueueResult = EnqueueFillImageHelper<>::enqueueFillImage(pCmdQ, image);
    EXPECT_EQ(CL_SUCCESS, enqueueResult);

    EXPECT_TRUE(UnitTestHelper<FamilyType>::evaluateDshUsage(dshBefore, pDSH->getUsed(), nullptr, rootDeviceIndex));
    EXPECT_NE(iohBefore, pIOH->getUsed());
    EXPECT_NE(sshBefore, pSSH->getUsed());
}

HWTEST_F(EnqueueFillImageTest, WhenFillingImageThenL3ProgrammingIsCorrect) {
    enqueueFillImage<FamilyType>();
    validateL3Programming<FamilyType>(cmdList, itorWalker);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueFillImageTest, WhenEnqueueIsDoneThenStateBaseAddressIsProperlyProgrammed) {
    enqueueFillImage<FamilyType>();
    auto &ultCsr = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto &hwHelper = HwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);

    validateStateBaseAddress<FamilyType>(ultCsr.getMemoryManager()->getInternalHeapBaseAddress(ultCsr.rootDeviceIndex, pIOH->getGraphicsAllocation()->isAllocatedInLocalMemoryPool()),
                                         ultCsr.getMemoryManager()->getInternalHeapBaseAddress(ultCsr.rootDeviceIndex, !hwHelper.useSystemMemoryPlacementForISA(pDevice->getHardwareInfo())),
                                         pDSH, pIOH, pSSH, itorPipelineSelect, itorWalker, cmdList, 0llu);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueFillImageTest, WhenFillingImageThenMediaInterfaceDescriptorLoadIsCorrect) {
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueFillImage<FamilyType>();

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

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueFillImageTest, WhenFillingImageThenInterfaceDescriptorDataIsCorrect) {
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueFillImage<FamilyType>();

    // Extract the interfaceDescriptorData
    auto cmdSBA = (STATE_BASE_ADDRESS *)cmdStateBaseAddress;
    auto &interfaceDescriptorData = *(INTERFACE_DESCRIPTOR_DATA *)cmdInterfaceDescriptorData;

    // Validate the kernel start pointer.  Technically, a kernel can start at address 0 but let's force a value.
    auto kernelStartPointer = ((uint64_t)interfaceDescriptorData.getKernelStartPointerHigh() << 32) + interfaceDescriptorData.getKernelStartPointer();
    EXPECT_LE(kernelStartPointer, cmdSBA->getInstructionBufferSize() * MemoryConstants::pageSize);

    size_t maxLocalSize = 256u;
    auto localWorkSize = std::min(maxLocalSize,
                                  Image2dDefaults::imageDesc.image_width * Image2dDefaults::imageDesc.image_height);
    auto simd = 32u;
    auto numThreadsPerThreadGroup = Math::divideAndRoundUp(localWorkSize, simd);
    EXPECT_EQ(numThreadsPerThreadGroup, interfaceDescriptorData.getNumberOfThreadsInGpgpuThreadGroup());
    EXPECT_NE(0u, interfaceDescriptorData.getCrossThreadConstantDataReadLength());
    EXPECT_NE(0u, interfaceDescriptorData.getConstantIndirectUrbEntryReadLength());

    // We shouldn't have these pointers the same.
    EXPECT_NE(kernelStartPointer, interfaceDescriptorData.getBindingTablePointer());
}

HWTEST_F(EnqueueFillImageTest, WhenFillingImageThenSurfaceStateIsCorrect) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);
    VariableBackup<CommandQueue *> cmdQBackup(&pCmdQ, mockCmdQ.get());
    mockCmdQ->storeMultiDispatchInfo = true;
    enqueueFillImage<FamilyType>();

    const auto &kernelInfo = mockCmdQ->storedMultiDispatchInfo.begin()->getKernel()->getKernelInfo();
    uint32_t index = static_cast<uint32_t>(kernelInfo.getArgDescriptorAt(0).template as<ArgDescImage>().bindful) / sizeof(RENDER_SURFACE_STATE);

    const auto surfaceState = getSurfaceState<FamilyType>(&pCmdQ->getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 0), index);
    const auto &imageDesc = image->getImageDesc();
    EXPECT_EQ(imageDesc.image_width, surfaceState->getWidth());
    EXPECT_EQ(imageDesc.image_height, surfaceState->getHeight());
    EXPECT_NE(0u, surfaceState->getSurfacePitch());
    EXPECT_NE(0u, surfaceState->getSurfaceType());
    EXPECT_EQ(MockGmmResourceInfo::getHAlignSurfaceStateResult, surfaceState->getSurfaceHorizontalAlignment());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT_VALIGN_4, surfaceState->getSurfaceVerticalAlignment());
    EXPECT_EQ(image->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddress(), surfaceState->getSurfaceBaseAddress());
}

HWTEST2_F(EnqueueFillImageTest, WhenFillingImageThenNumberOfPipelineSelectsIsOne, IsAtMostXeHpcCore) {
    enqueueFillImage<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueFillImageTest, WhenFillingImageThenMediaVfeStateIsSetCorrectly) {
    enqueueFillImage<FamilyType>();
    validateMediaVFEState<FamilyType>(&pDevice->getHardwareInfo(), cmdMediaVfeState, cmdList, itorMediaVfeState);
}

TEST_F(EnqueueFillImageTest, givenSrgbFormatWhenConvertingThenUseNormalizingFactor) {
    float *fillColor;
    int iFillColor[4] = {0};
    float lessThanZeroArray[4] = {-1.0f, -1.0f, -1.0f, 1.0f};
    float moreThanOneArray[4] = {2.0f, 2.0f, 2.0f, 1.0f};
    float naNArray[4] = {NAN, NAN, NAN, 1.0f};
    float distance;

    cl_image_format oldImageFormat = {CL_sRGBA, CL_UNORM_INT8};
    cl_image_format newImageFormat = {CL_RGBA, CL_UNSIGNED_INT8};

    fillColor = lessThanZeroArray;

    convertFillColor(static_cast<const void *>(fillColor), iFillColor, oldImageFormat, newImageFormat);

    for (int i = 0; i < 3; i++) {
        distance = std::fabs(0.0f - static_cast<float>(iFillColor[i]));
        EXPECT_GE(0.6f, distance);
    }
    EXPECT_EQ(255, iFillColor[3]);

    fillColor = moreThanOneArray;

    convertFillColor(static_cast<const void *>(fillColor), iFillColor, oldImageFormat, newImageFormat);

    for (int i = 0; i < 3; i++) {
        distance = std::fabs(255.0f - static_cast<float>(iFillColor[i]));
        EXPECT_GE(0.6f, distance);
    }
    EXPECT_EQ(255, iFillColor[3]);

    fillColor = naNArray;

    convertFillColor(static_cast<const void *>(fillColor), iFillColor, oldImageFormat, newImageFormat);

    for (int i = 0; i < 3; i++) {
        distance = std::fabs(0.0f - static_cast<float>(iFillColor[i]));
        EXPECT_GE(0.6f, distance);
    }
    EXPECT_EQ(255, iFillColor[3]);
}

TEST(ColorConvertTest, givenSnorm8FormatWhenConvertingThenUseNormalizingFactor) {
    float fFillColor[4] = {0.3f, -0.3f, 0.0f, 1.0f};
    int32_t iFillColor[4] = {};
    int32_t expectedIFillColor[4] = {};

    cl_image_format oldFormat = {CL_R, CL_SNORM_INT8};
    cl_image_format newFormat = {CL_R, CL_UNSIGNED_INT8};
    auto normalizingFactor = selectNormalizingFactor(oldFormat.image_channel_data_type);
    for (size_t i = 0; i < 4; i++) {
        expectedIFillColor[i] = static_cast<int32_t>(normalizingFactor * fFillColor[i]);
        expectedIFillColor[i] = expectedIFillColor[i] & 0xFF;
    }

    convertFillColor(static_cast<const void *>(fFillColor), iFillColor, oldFormat, newFormat);

    EXPECT_TRUE(memcmp(expectedIFillColor, iFillColor, 4 * sizeof(int32_t)) == 0);
}

TEST(ColorConvertTest, givenSnorm16FormatWhenConvertingThenUseNormalizingFactor) {
    float fFillColor[4] = {0.3f, -0.3f, 0.0f, 1.0f};
    int32_t iFillColor[4] = {};
    int32_t expectedIFillColor[4] = {};

    cl_image_format oldFormat = {CL_R, CL_SNORM_INT16};
    cl_image_format newFormat = {CL_R, CL_UNSIGNED_INT16};
    auto normalizingFactor = selectNormalizingFactor(oldFormat.image_channel_data_type);
    for (size_t i = 0; i < 4; i++) {
        expectedIFillColor[i] = static_cast<int32_t>(normalizingFactor * fFillColor[i]);
        expectedIFillColor[i] = expectedIFillColor[i] & 0xFFFF;
    }

    convertFillColor(static_cast<const void *>(fFillColor), iFillColor, oldFormat, newFormat);

    EXPECT_TRUE(memcmp(expectedIFillColor, iFillColor, 4 * sizeof(int32_t)) == 0);
}
