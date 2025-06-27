/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/program/create.inl"
#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

struct GetSizeRequiredImageTest : public CommandEnqueueFixture,
                                  public ::testing::Test {

    GetSizeRequiredImageTest() {
    }

    void SetUp() override {
        REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);
        CommandEnqueueFixture::setUp();

        srcImage = Image2dHelperUlt<>::create(context);
        dstImage = Image2dHelperUlt<>::create(context);

        pDevice->setPreemptionMode(PreemptionMode::Disabled);
    }

    void TearDown() override {
        if (IsSkipped()) {
            return;
        }
        delete dstImage;
        delete srcImage;

        CommandEnqueueFixture::tearDown();
    }

    Image *srcImage = nullptr;
    Image *dstImage = nullptr;
};

HWTEST_F(GetSizeRequiredImageTest, WhenCopyingImageThenHeapsAndCommandBufferConsumedMinimumRequiredSize) {
    USE_REAL_FILE_SYSTEM();
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::Type::dynamicState, 0u);
    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::Type::indirectObject, 0u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::Type::surfaceState, 0u);
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    auto retVal = EnqueueCopyImageHelper<>::enqueueCopyImage(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyImageToImage3d,
                                                                            pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcMemObj = srcImage;
    dc.dstMemObj = dstImage;
    dc.srcOffset = EnqueueCopyImageTraits::srcOrigin;
    dc.dstOffset = EnqueueCopyImageTraits::dstOrigin;
    dc.size = {1, 1, 1};

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    auto usedAfterCS = commandStream.getUsed();
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_COPY_IMAGE, false, false, *pCmdQ, kernel, {});
    auto expectedSizeDSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredDSH(*kernel);
    size_t localWorkSizes[] = {256, 1, 1};
    auto expectedSizeIOH = HardwareCommandsHelper<FamilyType>::getSizeRequiredIOH(*kernel, localWorkSizes, pDevice->getRootDeviceEnvironment());
    auto expectedSizeSSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredSSH(*kernel);

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    EXPECT_GE(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(expectedSizeIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(expectedSizeSSH, usedAfterSSH - usedBeforeSSH);
}

HWTEST_F(GetSizeRequiredImageTest, WhenCopyingReadWriteImageThenHeapsAndCommandBufferConsumedMinimumRequiredSize) {
    USE_REAL_FILE_SYSTEM();
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::Type::dynamicState, 0u);
    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::Type::indirectObject, 0u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::Type::surfaceState, 0u);
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    std::unique_ptr<MockProgram> program(Program::createBuiltInFromSource<MockProgram>("CopyImageTo3dImage3d", context, context->getDevices(), nullptr));
    program->build(program->getDevices(), nullptr);
    cl_int retVal{CL_SUCCESS};
    std::unique_ptr<Kernel> kernel(Kernel::create<MockKernel>(program.get(), program->getKernelInfoForKernel("CopyImage3dToImage3d"), *context->getDevice(0), retVal));

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, kernel);
    // This kernel does not operate on OpenCL 2.0 Read and Write images
    EXPECT_FALSE(kernel->getKernelInfo().kernelDescriptor.kernelAttributes.flags.usesFencesForReadWriteImages);
    // Simulate that the kernel actually operates on OpenCL 2.0 Read and Write images.
    // Such kernel may require special WA DisableLSQCROPERFforOCL during construction of Command Buffer
    const_cast<KernelDescriptor &>(kernel->getKernelInfo().kernelDescriptor).kernelAttributes.flags.usesFencesForReadWriteImages = true;

    // Enqueue kernel that may require special WA DisableLSQCROPERFforOCL
    retVal = EnqueueKernelHelper<>::enqueueKernel(pCmdQ, kernel.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto usedAfterCS = commandStream.getUsed();
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_COPY_IMAGE, false, false, *pCmdQ, kernel.get(), {});
    auto expectedSizeDSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredDSH(*kernel.get());
    size_t localWorkSizes[] = {256, 1, 1};
    auto expectedSizeIOH = HardwareCommandsHelper<FamilyType>::getSizeRequiredIOH(*kernel.get(), localWorkSizes, pDevice->getRootDeviceEnvironment());
    auto expectedSizeSSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredSSH(*kernel.get());

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    const_cast<KernelDescriptor &>(kernel->getKernelInfo().kernelDescriptor).kernelAttributes.flags.usesFencesForReadWriteImages = false;

    EXPECT_GE(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(expectedSizeIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(expectedSizeSSH, usedAfterSSH - usedBeforeSSH);
}

HWTEST_F(GetSizeRequiredImageTest, WhenReadingImageNonBlockingThenHeapsAndCommandBufferConsumedMinimumRequiredSize) {
    USE_REAL_FILE_SYSTEM();
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::Type::dynamicState, 0u);
    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::Type::indirectObject, 0u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::Type::surfaceState, 0u);
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    auto retVal = EnqueueReadImageHelper<>::enqueueReadImage(
        pCmdQ,
        srcImage,
        CL_FALSE);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyImage3dToBuffer,
                                                                            pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcMemObj = srcImage;
    dc.dstPtr = EnqueueReadImageTraits::hostPtr;
    dc.srcOffset = EnqueueReadImageTraits::origin;
    dc.size = EnqueueReadImageTraits::region;
    dc.srcRowPitch = EnqueueReadImageTraits::rowPitch;
    dc.srcSlicePitch = EnqueueReadImageTraits::slicePitch;

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    auto usedAfterCS = commandStream.getUsed();
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_READ_IMAGE, false, false, *pCmdQ, kernel, {});
    auto expectedSizeDSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredDSH(*kernel);
    size_t localWorkSizes[] = {256, 1, 1};
    auto expectedSizeIOH = HardwareCommandsHelper<FamilyType>::getSizeRequiredIOH(*kernel, localWorkSizes, pDevice->getRootDeviceEnvironment());
    auto expectedSizeSSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredSSH(*kernel);

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    EXPECT_GE(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(expectedSizeIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(expectedSizeSSH, usedAfterSSH - usedBeforeSSH);
}

HWTEST_F(GetSizeRequiredImageTest, WhenReadingImageBlockingThenHeapsAndCommandBufferConsumedMinimumRequiredSize) {
    USE_REAL_FILE_SYSTEM();
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::Type::dynamicState, 0u);
    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::Type::indirectObject, 0u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::Type::surfaceState, 0u);
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    auto retVal = EnqueueReadImageHelper<>::enqueueReadImage(
        pCmdQ,
        srcImage,
        CL_TRUE);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyImage3dToBuffer,
                                                                            pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcMemObj = srcImage;
    dc.dstPtr = EnqueueReadImageTraits::hostPtr;
    dc.srcOffset = EnqueueReadImageTraits::origin;
    dc.size = EnqueueReadImageTraits::region;
    dc.srcRowPitch = EnqueueReadImageTraits::rowPitch;
    dc.srcSlicePitch = EnqueueReadImageTraits::slicePitch;

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    auto usedAfterCS = commandStream.getUsed();
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_READ_IMAGE, false, false, *pCmdQ, kernel, {});
    auto expectedSizeDSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredDSH(*kernel);
    size_t localWorkSizes[] = {256, 1, 1};
    auto expectedSizeIOH = HardwareCommandsHelper<FamilyType>::getSizeRequiredIOH(*kernel, localWorkSizes, pDevice->getRootDeviceEnvironment());
    auto expectedSizeSSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredSSH(*kernel);

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    EXPECT_GE(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(expectedSizeIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(expectedSizeSSH, usedAfterSSH - usedBeforeSSH);
}

HWTEST_F(GetSizeRequiredImageTest, WhenWritingImageNonBlockingThenHeapsAndCommandBufferConsumedMinimumRequiredSize) {
    USE_REAL_FILE_SYSTEM();
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::Type::dynamicState, 0u);
    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::Type::indirectObject, 0u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::Type::surfaceState, 0u);
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    auto retVal = EnqueueWriteImageHelper<>::enqueueWriteImage(
        pCmdQ,
        dstImage,
        CL_FALSE);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferToImage3d,
                                                                            pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcPtr = EnqueueWriteImageTraits::hostPtr;
    dc.dstMemObj = dstImage;
    dc.dstOffset = EnqueueWriteImageTraits::origin;
    dc.size = EnqueueWriteImageTraits::region;
    dc.dstRowPitch = EnqueueWriteImageTraits::rowPitch;
    dc.dstSlicePitch = EnqueueWriteImageTraits::slicePitch;

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    auto usedAfterCS = commandStream.getUsed();
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_WRITE_IMAGE, false, false, *pCmdQ, kernel, {});
    auto expectedSizeDSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredDSH(*kernel);
    size_t localWorkSizes[] = {256, 1, 1};
    auto expectedSizeIOH = HardwareCommandsHelper<FamilyType>::getSizeRequiredIOH(*kernel, localWorkSizes, pDevice->getRootDeviceEnvironment());
    auto expectedSizeSSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredSSH(*kernel);

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    EXPECT_GE(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(expectedSizeIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(expectedSizeSSH, usedAfterSSH - usedBeforeSSH);
}

HWTEST_F(GetSizeRequiredImageTest, WhenWritingImageBlockingThenHeapsAndCommandBufferConsumedMinimumRequiredSize) {
    USE_REAL_FILE_SYSTEM();
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::Type::dynamicState, 0u);
    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::Type::indirectObject, 0u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::Type::surfaceState, 0u);
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    auto retVal = EnqueueWriteImageHelper<>::enqueueWriteImage(
        pCmdQ,
        dstImage,
        CL_TRUE);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferToImage3d,
                                                                            pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcPtr = EnqueueWriteImageTraits::hostPtr;
    dc.dstMemObj = dstImage;
    dc.dstOffset = EnqueueWriteImageTraits::origin;
    dc.size = EnqueueWriteImageTraits::region;
    dc.dstRowPitch = EnqueueWriteImageTraits::rowPitch;
    dc.dstSlicePitch = EnqueueWriteImageTraits::slicePitch;

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    auto usedAfterCS = commandStream.getUsed();
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_WRITE_IMAGE, false, false, *pCmdQ, kernel, {});
    auto expectedSizeDSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredDSH(*kernel);
    size_t localWorkSizes[] = {256, 1, 1};
    auto expectedSizeIOH = HardwareCommandsHelper<FamilyType>::getSizeRequiredIOH(*kernel, localWorkSizes, pDevice->getRootDeviceEnvironment());
    auto expectedSizeSSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredSSH(*kernel);

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    EXPECT_GE(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(expectedSizeIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(expectedSizeSSH, usedAfterSSH - usedBeforeSSH);
}