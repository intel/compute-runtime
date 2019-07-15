/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/aub_tests/fixtures/run_kernel_fixture.h"
#include "unit_tests/fixtures/two_walker_fixture.h"

using namespace NEO;

namespace ULT {

class AUBRunKernelIntegrateTest : public RunKernelFixture<AUBRunKernelFixtureFactory>,
                                  public ::testing::Test {
    typedef RunKernelFixture<AUBRunKernelFixtureFactory> ParentClass;

  protected:
    void SetUp() override {
        ParentClass::SetUp();
    }

    void TearDown() override {
        ParentClass::TearDown();
    }
};

SKLTEST_F(AUBRunKernelIntegrateTest, ooqExecution) {
    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {16, 1, 1};
    size_t localWorkSize[3] = {16, 1, 1};
    cl_uint numEventsInWaitList = 0;
    cl_event *event0 = nullptr;
    cl_event *event1 = nullptr;
    cl_event *event2 = nullptr;
    cl_int retVal = CL_FALSE;

    std::string kernelFilename;
    retrieveBinaryKernelFilename(kernelFilename, "simple_kernels_", ".bin");
    Program *pProgram = CreateProgramFromBinary(kernelFilename);
    ASSERT_NE(nullptr, pProgram);

    cl_device_id device = pDevice;
    retVal = pProgram->build(
        1,
        &device,
        nullptr,
        nullptr,
        nullptr,
        false);
    ASSERT_EQ(CL_SUCCESS, retVal);

    const KernelInfo *pKernelInfo0 = pProgram->getKernelInfo("simple_kernel_0");
    ASSERT_NE(nullptr, pKernelInfo0);

    Kernel *pKernel0 = Kernel::create<MockKernel>(
        pProgram,
        *pKernelInfo0,
        &retVal);
    ASSERT_NE(nullptr, pKernel0);

    const KernelInfo *pKernelInfo1 = pProgram->getKernelInfo("simple_kernel_1");
    ASSERT_NE(nullptr, pKernelInfo1);

    Kernel *pKernel1 = Kernel::create<MockKernel>(
        pProgram,
        *pKernelInfo1,
        &retVal);
    ASSERT_NE(nullptr, pKernel1);

    const KernelInfo *pKernelInfo2 = pProgram->getKernelInfo("simple_kernel_2");
    ASSERT_NE(nullptr, pKernelInfo2);

    Kernel *pKernel2 = Kernel::create<MockKernel>(
        pProgram,
        *pKernelInfo2,
        &retVal);
    ASSERT_NE(nullptr, pKernel2);

    const cl_int NUM_ELEMS = 64;
    const size_t BUFFER_SIZE = NUM_ELEMS * sizeof(cl_uint);

    cl_uint *destinationMemory1;
    cl_uint *destinationMemory2;
    cl_uint expectedMemory1[NUM_ELEMS];
    cl_uint expectedMemory2[NUM_ELEMS];

    cl_uint arg0 = 2;
    cl_float arg1 = 3.0f;
    cl_uint arg3 = 4;
    cl_uint arg5 = 0xBBBBBBBB;
    cl_uint bad_value = 0; // set to non-zero to force failure

    destinationMemory1 = (cl_uint *)::alignedMalloc(BUFFER_SIZE, 4096);
    ASSERT_NE(nullptr, destinationMemory1);
    destinationMemory2 = (cl_uint *)::alignedMalloc(BUFFER_SIZE, 4096);
    ASSERT_NE(nullptr, destinationMemory2);

    for (cl_int i = 0; i < NUM_ELEMS; i++) {
        destinationMemory1[i] = 0xA1A1A1A1;
        destinationMemory2[i] = 0xA2A2A2A2;
        expectedMemory1[i] = (arg0 + static_cast<cl_uint>(arg1) + arg3 + bad_value);
        expectedMemory2[i] = arg5 + bad_value;
    }

    auto pDestinationMemory1 = &destinationMemory1[0];
    auto pDestinationMemory2 = &destinationMemory2[0];
    auto pExpectedMemory1 = &expectedMemory1[0];
    auto pExpectedMemory2 = &expectedMemory2[0];

    auto intermediateBuffer = Buffer::create(
        context,
        CL_MEM_READ_WRITE,
        BUFFER_SIZE,
        nullptr,
        retVal);
    ASSERT_NE(nullptr, intermediateBuffer);

    auto destinationBuffer1 = Buffer::create(
        context,
        CL_MEM_USE_HOST_PTR,
        BUFFER_SIZE,
        pDestinationMemory1,
        retVal);
    ASSERT_NE(nullptr, destinationBuffer1);

    //buffer may not be zero copied
    pDestinationMemory1 = reinterpret_cast<decltype(pDestinationMemory1)>(destinationBuffer1->getGraphicsAllocation()->getGpuAddress());

    auto destinationBuffer2 = Buffer::create(
        context,
        CL_MEM_USE_HOST_PTR,
        BUFFER_SIZE,
        pDestinationMemory2,
        retVal);
    ASSERT_NE(nullptr, destinationBuffer2);

    //buffer may not be zero copied
    pDestinationMemory2 = reinterpret_cast<decltype(pDestinationMemory2)>(destinationBuffer2->getGraphicsAllocation()->getGpuAddress());

    cl_mem arg2 = intermediateBuffer;
    cl_mem arg4 = destinationBuffer1;
    cl_mem arg6 = destinationBuffer2;

    //__kernel void simple_kernel_0(const uint arg0, const float arg1, __global uint *dst)
    //{ dst = arg0 + arg1 }
    retVal = clSetKernelArg(pKernel0, 0, sizeof(cl_uint), &arg0);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(pKernel0, 1, sizeof(cl_float), &arg1);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(pKernel0, 2, sizeof(cl_mem), &arg2);
    ASSERT_EQ(CL_SUCCESS, retVal);

    //__kernel void simple_kernel_1(__global uint *src, const uint arg1, __global uint *dst)
    //{ dst = src + arg1 }
    retVal = clSetKernelArg(pKernel1, 0, sizeof(cl_mem), &arg2);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(pKernel1, 1, sizeof(cl_uint), &arg3);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(pKernel1, 2, sizeof(cl_mem), &arg4);
    ASSERT_EQ(CL_SUCCESS, retVal);

    //__kernel void simple_kernel_2(const uint arg1, __global uint *dst)
    //{ dst = arg1 }
    retVal = clSetKernelArg(pKernel2, 0, sizeof(cl_mem), &arg5);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(pKernel2, 1, sizeof(cl_mem), &arg6);
    ASSERT_EQ(CL_SUCCESS, retVal);

    // Create a second command queue (beyond the default one)
    CommandQueue *pCmdQ2 = nullptr;
    pCmdQ2 = createCommandQueue(pDevice, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE);
    ASSERT_NE(nullptr, pCmdQ2);

    auto &csr = pCmdQ2->getGpgpuCommandStreamReceiver();
    csr.overrideDispatchPolicy(DispatchMode::ImmediateDispatch);

    retVal = pCmdQ2->enqueueKernel(
        pKernel0,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        nullptr,
        event0);
    ASSERT_EQ(CL_SUCCESS, retVal);

    // depends on kernel0
    retVal = pCmdQ2->enqueueKernel(
        pKernel1,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        event0,
        event1);
    ASSERT_EQ(CL_SUCCESS, retVal);

    // independent from other kernels, can be run asynchronously
    retVal = pCmdQ2->enqueueKernel(
        pKernel2,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        nullptr,
        event2);
    ASSERT_EQ(CL_SUCCESS, retVal);

    HardwareParse::parseCommands<FamilyType>(*pCmdQ2);

    // Compute our memory expecations based on kernel execution
    auto globalWorkItems = globalWorkSize[0] * globalWorkSize[1] * globalWorkSize[2];
    auto sizeWritten = globalWorkItems * sizeof(cl_uint);
    AUBCommandStreamFixture::expectMemory<FamilyType>(pDestinationMemory1, pExpectedMemory1, sizeWritten);
    AUBCommandStreamFixture::expectMemory<FamilyType>(pDestinationMemory2, pExpectedMemory2, sizeWritten);

    // ensure we didn't overwrite existing memory
    if (sizeWritten < BUFFER_SIZE) {
        auto sizeRemaining = BUFFER_SIZE - sizeWritten;
        auto pUnwrittenMemory1 = (pDestinationMemory1 + sizeWritten / sizeof(cl_uint));
        auto pUnwrittenMemory2 = (pDestinationMemory2 + sizeWritten / sizeof(cl_uint));
        auto pExpectedUnwrittenMemory1 = &destinationMemory1[globalWorkItems];
        auto pExpectedUnwrittenMemory2 = &destinationMemory2[globalWorkItems];
        AUBCommandStreamFixture::expectMemory<FamilyType>(pUnwrittenMemory1, pExpectedUnwrittenMemory1, sizeRemaining);
        AUBCommandStreamFixture::expectMemory<FamilyType>(pUnwrittenMemory2, pExpectedUnwrittenMemory2, sizeRemaining);
    }

    ::alignedFree(destinationMemory1);
    ::alignedFree(destinationMemory2);
    delete intermediateBuffer;
    delete destinationBuffer1;
    delete destinationBuffer2;
    delete pKernel0;
    delete pKernel1;
    delete pKernel2;
    delete pProgram;
    delete pCmdQ2;
}

SKLTEST_F(AUBRunKernelIntegrateTest, deviceSideVme) {
    const cl_int testWidth = 32;
    const cl_int testHeight = 16;
    const cl_uint workDim = 2;
    const size_t globalWorkSize[2] = {testWidth, testHeight};
    const size_t *localWorkSize = nullptr;
    cl_uint numEventsInWaitList = 0;
    auto retVal = CL_INVALID_VALUE;

    // VME works on 16x16 macroblocks
    const cl_int mbWidth = testWidth / 16;
    const cl_int mbHeight = testHeight / 16;

    // 1 per macroblock (there is 1 macroblock in this test):
    const int PRED_BUFFER_SIZE = mbWidth * mbHeight;
    const int SHAPES_BUFFER_SIZE = mbWidth * mbHeight;

    // 4 per macroblock (valid for 8x8 mode only):
    const int MV_BUFFER_SIZE = testWidth * mbHeight / 4;
    const int RESIDUALS_BUFFER_SIZE = MV_BUFFER_SIZE;

    std::string kernelFilename;
    retrieveBinaryKernelFilename(kernelFilename, "vme_kernels_", ".bin");
    Program *pProgram = CreateProgramFromBinary(kernelFilename);
    ASSERT_NE(nullptr, pProgram);

    cl_device_id device = pDevice;

    retVal = pProgram->build(
        1,
        &device,
        "",
        nullptr,
        nullptr,
        false);
    ASSERT_EQ(CL_SUCCESS, retVal);

    const KernelInfo *pKernelInfo = pProgram->getKernelInfo("device_side_block_motion_estimate_intel");
    EXPECT_NE(nullptr, pKernelInfo);

    Kernel *pKernel = Kernel::create<MockKernel>(
        pProgram,
        *pKernelInfo,
        &retVal);
    ASSERT_NE(pKernel, nullptr);

    EXPECT_EQ(true, pKernel->isVmeKernel());

    cl_image_format imageFormat;
    cl_image_desc imageDesc;

    imageFormat.image_channel_order = CL_R;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = testWidth;
    imageDesc.image_height = testHeight;
    imageDesc.image_depth = 0;
    imageDesc.image_array_size = 0;
    imageDesc.image_row_pitch = 0;
    imageDesc.image_slice_pitch = 0;
    imageDesc.num_mip_levels = 0;
    imageDesc.num_samples = 0;
    imageDesc.mem_object = nullptr;

    const int INPUT_SIZE = testWidth * testHeight;
    ASSERT_GT(INPUT_SIZE, 0);

    auto srcMemory = (cl_uchar *)::alignedMalloc(INPUT_SIZE, 4096);
    ASSERT_NE(srcMemory, nullptr);
    memset(srcMemory, 0x00, INPUT_SIZE);

    auto refMemory = (cl_uchar *)::alignedMalloc(INPUT_SIZE, 4096);
    ASSERT_NE(refMemory, nullptr);
    memset(refMemory, 0x00, INPUT_SIZE);

    int xMovement = 7;
    int yMovement = 9;

    // pixel movement: 0xFF, 0xFF values moved from 0x0 to 7x9 for vme kernel to detect
    srcMemory[0] = 0xFF; // 1.0
    srcMemory[1] = 0xFF; // 1.0
    refMemory[xMovement + yMovement * testWidth] = 0xFF;
    refMemory[xMovement + yMovement * testWidth + 1] = 0xFF;

    cl_mem_flags flags = CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    auto srcImage = Image::create(
        context,
        flags,
        surfaceFormat,
        &imageDesc,
        srcMemory,
        retVal);
    ASSERT_NE(nullptr, srcImage);

    auto refImage = Image::create(
        context,
        flags,
        surfaceFormat,
        &imageDesc,
        refMemory,
        retVal);
    ASSERT_NE(nullptr, refImage);

    cl_short2 *predMem = new cl_short2[PRED_BUFFER_SIZE];
    for (int i = 0; i < PRED_BUFFER_SIZE; i++) {
        predMem[i].s[0] = 0;
        predMem[i].s[1] = 0;
    }

    auto predMvBuffer = Buffer::create(
        context,
        CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        PRED_BUFFER_SIZE * sizeof(cl_short2),
        predMem,
        retVal);
    ASSERT_NE(nullptr, predMvBuffer);

    auto motionVectorBuffer = Buffer::create(
        context,
        CL_MEM_WRITE_ONLY,
        MV_BUFFER_SIZE * sizeof(cl_short2),
        nullptr,
        retVal);
    ASSERT_NE(nullptr, motionVectorBuffer);

    auto residualsBuffer = Buffer::create(
        context,
        CL_MEM_WRITE_ONLY,
        RESIDUALS_BUFFER_SIZE * sizeof(cl_short),
        nullptr,
        retVal);
    ASSERT_NE(nullptr, residualsBuffer);

    auto shapesBuffer = Buffer::create(
        context,
        CL_MEM_WRITE_ONLY,
        SHAPES_BUFFER_SIZE * sizeof(cl_char2),
        nullptr,
        retVal);
    ASSERT_NE(nullptr, shapesBuffer);

    // kernel decl:
    //void  block_motion_estimate_intel_noacc(
    //    __read_only image2d_t   srcImg,               // IN
    //    __read_only image2d_t   refImg,               // IN
    //    __global short2*        prediMVbuffer,        // IN
    //    __global short2*        motion_vector_buffer, // OUT
    //    __global ushort*        residuals_buffer,     // OUT
    //    __global uchar2*        shapes_buffer,        // OUT
    //    int                     iterations,           // IN
    //    int                     partition_mask)       // IN

    cl_mem arg0 = srcImage;
    cl_mem arg1 = refImage;
    cl_mem arg2 = predMvBuffer;
    cl_mem arg3 = motionVectorBuffer;
    cl_mem arg4 = residualsBuffer;
    cl_mem arg5 = shapesBuffer;
    cl_int arg6 = mbHeight;
    cl_int arg7 = CL_AVC_ME_PARTITION_MASK_8x8_INTEL;

    retVal = clSetKernelArg(pKernel, 0, sizeof(cl_mem), &arg0);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(pKernel, 1, sizeof(cl_mem), &arg1);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(pKernel, 2, sizeof(cl_mem), &arg2);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(pKernel, 3, sizeof(cl_mem), &arg3);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(pKernel, 4, sizeof(cl_mem), &arg4);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(pKernel, 5, sizeof(cl_mem), &arg5);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(pKernel, 6, sizeof(cl_int), &arg6);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(pKernel, 7, sizeof(cl_int), &arg7);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = pCmdQ->enqueueKernel(
        pKernel,
        workDim,
        nullptr,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        nullptr,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_short2 destinationMV[MV_BUFFER_SIZE];
    cl_short destinationResiduals[RESIDUALS_BUFFER_SIZE];
    cl_uchar2 destinationShapes[SHAPES_BUFFER_SIZE];

    motionVectorBuffer->forceDisallowCPUCopy = true;
    residualsBuffer->forceDisallowCPUCopy = true;
    shapesBuffer->forceDisallowCPUCopy = true;

    retVal = pCmdQ->enqueueReadBuffer(motionVectorBuffer, true, 0, sizeof(destinationMV), destinationMV, nullptr, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = pCmdQ->enqueueReadBuffer(residualsBuffer, true, 0, sizeof(destinationResiduals), destinationResiduals, nullptr, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = pCmdQ->enqueueReadBuffer(shapesBuffer, true, 0, sizeof(destinationShapes), destinationShapes, nullptr, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    // Check if our buffers matches expectations
    cl_short2 expectedMV[MV_BUFFER_SIZE];
    cl_short expectedResiduals[RESIDUALS_BUFFER_SIZE];
    cl_uchar2 expectedShapes[SHAPES_BUFFER_SIZE];

    // This test uses 8x8 sub blocks (4 per macroblock)
    for (int i = 0; i < SHAPES_BUFFER_SIZE; i++) {
        expectedShapes[i].s0 = CL_AVC_ME_MAJOR_8x8_INTEL;
        expectedShapes[i].s1 = CL_AVC_ME_MINOR_8x8_INTEL;
    }

    for (int i = 0; i < MV_BUFFER_SIZE; i++) {
        expectedResiduals[i] = 0;

        // Second and fourth block not moved, set 0 as default.
        expectedMV[i].s0 = 0;
        expectedMV[i].s1 = 0;

        // First 8x8 subblock moved by 7x9 vecor as xMovement is 7 and
        // yMovement is 9
        if (i == 0) {
            // times 4 since VME returns data in quarter pixels.
            expectedMV[i].s0 = 4 * xMovement;
            expectedMV[i].s1 = 4 * yMovement;
        }
        // In this test all other subblocks are empty, in 16x12 mode used in
        // this test vme should find match at -16 x -12
        else {
            expectedMV[i].s0 = 4 * -16;
            expectedMV[i].s1 = 4 * -12;
        }
    }

    AUBCommandStreamFixture::expectMemory<FamilyType>(destinationMV, expectedMV, sizeof(expectedMV));
    AUBCommandStreamFixture::expectMemory<FamilyType>(destinationResiduals, expectedResiduals, sizeof(expectedResiduals));
    AUBCommandStreamFixture::expectMemory<FamilyType>(destinationShapes, expectedShapes, sizeof(expectedShapes));

    delete predMvBuffer;
    delete motionVectorBuffer;
    delete residualsBuffer;
    delete shapesBuffer;

    delete[] predMem;

    ::alignedFree(srcMemory);
    srcMemory = nullptr;
    delete srcImage;

    ::alignedFree(refMemory);
    refMemory = nullptr;
    delete refImage;

    delete pKernel;
    delete pProgram;
}
} // namespace ULT
