/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <algorithm>
#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "reg_configs_common.h"
#include "unit_tests/command_queue/enqueue_copy_image_to_buffer_fixture.h"
#include "unit_tests/gen_common/gen_commands_common_validation.h"
#include "unit_tests/helpers/unit_test_helper.h"
#include "unit_tests/mocks/mock_builtin_dispatch_info_builder.h"
#include "test.h"

using namespace OCLRT;

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueCopyImageToBufferTest, gpgpuWalker) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    enqueueCopyImageToBuffer<FamilyType>();

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
    EXPECT_EQ(0u, cmd->getIndirectDataLength() % GPGPU_WALKER::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);
    EXPECT_FALSE(cmd->getIndirectParameterEnable());

    // Compute the SIMD lane mask
    size_t simd =
        cmd->getSimdSize() == GPGPU_WALKER::SIMD_SIZE_SIMD32 ? 32 : cmd->getSimdSize() == GPGPU_WALKER::SIMD_SIZE_SIMD16 ? 16 : 8;
    uint64_t simdMask = (1ull << simd) - 1;

    // Mask off lanes based on the execution masks
    auto laneMaskRight = cmd->getRightExecutionMask() & simdMask;
    auto lanesPerThreadX = 0;
    while (laneMaskRight) {
        lanesPerThreadX += laneMaskRight & 1;
        laneMaskRight >>= 1;
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueCopyImageToBufferTest, alignsToCSR) {
    //this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;

    enqueueCopyImageToBuffer<FamilyType>();
    EXPECT_EQ(csr.peekTaskCount(), pCmdQ->taskCount);
    EXPECT_EQ(csr.peekTaskLevel(), pCmdQ->taskLevel + 1);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueCopyImageToBufferTest, bumpsTaskLevel) {
    auto taskLevelBefore = pCmdQ->taskLevel;

    enqueueCopyImageToBuffer<FamilyType>();
    EXPECT_GT(pCmdQ->taskLevel, taskLevelBefore);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueCopyImageToBufferTest, addsCommands) {
    auto usedCmdBufferBefore = pCS->getUsed();

    enqueueCopyImageToBuffer<FamilyType>();
    EXPECT_NE(usedCmdBufferBefore, pCS->getUsed());
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueCopyImageToBufferTest, addsIndirectData) {
    auto dshBefore = pDSH->getUsed();
    auto iohBefore = pIOH->getUsed();
    auto sshBefore = pSSH->getUsed();

    enqueueCopyImageToBuffer<FamilyType>();
    EXPECT_TRUE(UnitTestHelper<FamilyType>::evaluateDshUsage(dshBefore, pDSH->getUsed(), nullptr));
    EXPECT_NE(iohBefore, pIOH->getUsed());
    EXPECT_NE(sshBefore, pSSH->getUsed());
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueCopyImageToBufferTest, loadRegisterImmediateL3CNTLREG) {
    enqueueCopyImageToBuffer<FamilyType>();
    validateL3Programming<FamilyType>(cmdList, itorWalker);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueCopyImageToBufferTest, WhenEnqueueIsDoneThenStateBaseAddressIsProperlyProgrammed) {
    enqueueCopyImageToBuffer<FamilyType>();
    validateStateBaseAddress<FamilyType>(this->pDevice->getCommandStreamReceiver().getMemoryManager()->getInternalHeapBaseAddress(),
                                         pDSH, pIOH, pSSH, itorPipelineSelect, itorWalker, cmdList, 0llu);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueCopyImageToBufferTest, mediaInterfaceDescriptorLoad) {
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueCopyImageToBuffer<FamilyType>();

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

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueCopyImageToBufferTest, interfaceDescriptorData) {
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueCopyImageToBuffer<FamilyType>();

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
    auto threadsPerThreadGroup = (localWorkSize + simd - 1) / simd;
    EXPECT_EQ(threadsPerThreadGroup, interfaceDescriptorData.getNumberOfThreadsInGpgpuThreadGroup());
    EXPECT_NE(0u, interfaceDescriptorData.getCrossThreadConstantDataReadLength());
    EXPECT_NE(0u, interfaceDescriptorData.getConstantIndirectUrbEntryReadLength());

    // We shouldn't have these pointers the same.
    EXPECT_NE(kernelStartPointer, interfaceDescriptorData.getBindingTablePointer());
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueCopyImageToBufferTest, surfaceState) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    enqueueCopyImageToBuffer<FamilyType>();

    const auto &surfaceState = getSurfaceState<FamilyType>(0);
    const auto &imageDesc = srcImage->getImageDesc();
    // EnqueueReadImage uses multi-byte copies depending on per-pixel-size-in-bytes
    EXPECT_EQ(imageDesc.image_width, surfaceState.getWidth());
    EXPECT_EQ(imageDesc.image_height, surfaceState.getHeight());
    EXPECT_NE(0u, surfaceState.getSurfacePitch());
    EXPECT_NE(0u, surfaceState.getSurfaceType());
    auto surfaceFormat = surfaceState.getSurfaceFormat();
    bool isRedescribedFormat =
        surfaceFormat == RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32A32_UINT ||
        surfaceFormat == RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32_UINT ||
        surfaceFormat == RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_UINT ||
        surfaceFormat == RENDER_SURFACE_STATE::SURFACE_FORMAT_R16_UINT ||
        surfaceFormat == RENDER_SURFACE_STATE::SURFACE_FORMAT_R8_UINT;
    EXPECT_TRUE(isRedescribedFormat);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_4, surfaceState.getSurfaceHorizontalAlignment());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT_VALIGN_4, surfaceState.getSurfaceVerticalAlignment());
    EXPECT_EQ(reinterpret_cast<uint64_t>(srcImage->getCpuAddress()), surfaceState.getSurfaceBaseAddress());
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueCopyImageToBufferTest, pipelineSelect) {
    enqueueCopyImageToBuffer<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueCopyImageToBufferTest, mediaVFEState) {
    enqueueCopyImageToBuffer<FamilyType>();
    validateMediaVFEState<FamilyType>(&pDevice->getHardwareInfo(), cmdMediaVfeState, cmdList, itorMediaVfeState);
}

typedef EnqueueCopyImageToBufferMipMapTest MipMapCopyImageToBufferTest;

HWTEST_P(MipMapCopyImageToBufferTest, GivenImageWithMipLevelNonZeroWhenCopyImageToBufferIsCalledThenProperMipLevelIsSet) {
    auto image_type = (cl_mem_object_type)GetParam();
    auto &builtIns = pCmdQ->getDevice().getBuiltIns();
    auto &origBuilder = builtIns.getBuiltinDispatchInfoBuilder(
        EBuiltInOps::CopyImage3dToBuffer,
        pCmdQ->getContext(),
        pCmdQ->getDevice());

    // substitute original builder with mock builder
    auto oldBuilder = builtIns.setBuiltinDispatchInfoBuilder(
        EBuiltInOps::CopyImage3dToBuffer,
        pCmdQ->getContext(),
        pCmdQ->getDevice(),
        std::unique_ptr<OCLRT::BuiltinDispatchInfoBuilder>(new MockBuiltinDispatchInfoBuilder(builtIns, &origBuilder)));

    cl_int retVal = CL_SUCCESS;
    cl_image_desc imageDesc = {};
    uint32_t expectedMipLevel = 3;
    imageDesc.image_type = image_type;
    imageDesc.num_mip_levels = 10;
    imageDesc.image_width = 4;
    imageDesc.image_height = 1;
    imageDesc.image_depth = 1;
    size_t origin[] = {0, 0, 0, 0};
    size_t region[] = {imageDesc.image_width, 1, 1};
    std::unique_ptr<Image> image;
    switch (image_type) {
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

    retVal = pCmdQ->enqueueCopyImageToBuffer(image.get(),
                                             dstBuffer,
                                             origin,
                                             region,
                                             0,
                                             0,
                                             nullptr,
                                             nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto &mockBuilder = static_cast<MockBuiltinDispatchInfoBuilder &>(builtIns.getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyImage3dToBuffer,
                                                                                                             pCmdQ->getContext(),
                                                                                                             pCmdQ->getDevice()));
    auto params = mockBuilder.getBuiltinOpParams();

    EXPECT_EQ(expectedMipLevel, params->srcMipLevel);

    // restore original builder and retrieve mock builder
    auto newBuilder = builtIns.setBuiltinDispatchInfoBuilder(
        EBuiltInOps::CopyImage3dToBuffer,
        pCmdQ->getContext(),
        pCmdQ->getDevice(),
        std::move(oldBuilder));
    EXPECT_NE(nullptr, newBuilder);
}

INSTANTIATE_TEST_CASE_P(MipMapCopyImageToBufferTest_GivenImageWithMipLevelNonZeroWhenCopyImageToBufferIsCalledThenProperMipLevelIsSet,
                        MipMapCopyImageToBufferTest, ::testing::Values(CL_MEM_OBJECT_IMAGE1D, CL_MEM_OBJECT_IMAGE1D_ARRAY, CL_MEM_OBJECT_IMAGE2D, CL_MEM_OBJECT_IMAGE2D_ARRAY, CL_MEM_OBJECT_IMAGE3D));
