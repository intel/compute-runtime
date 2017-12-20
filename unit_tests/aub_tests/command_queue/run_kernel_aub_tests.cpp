/*
 * Copyright (c) 2017, Intel Corporation
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

#include "runtime/memory_manager/surface.h"
#include "unit_tests/fixtures/run_kernel_fixture.h"
#include "unit_tests/fixtures/two_walker_fixture.h"
#include "unit_tests/aub_tests/fixtures/run_kernel_fixture.h"
#include "CL/cl_ext.h"
#include "CL/cl.h"
#include "public/cl_vebox_intel.h"

using namespace OCLRT;

namespace ULT {

class AUBRunKernelIntegrateTest : public RunKernelFixture<AUBRunKernelFixtureFactory>,
                                  public ::testing::Test {
    typedef RunKernelFixture<AUBRunKernelFixtureFactory> ParentClass;

    void SetUp() override {
        ParentClass::SetUp();
    }

    void TearDown() override {
        ParentClass::TearDown();
    }
};

TEST_F(AUBRunKernelIntegrateTest, VeBoxHotPixel) {
    auto retVal = CL_INVALID_VALUE;
    const cl_int testWidth = 16;
    const cl_int testHeight = 16;
    cl_device_id device = pDevice;

    overwriteBuiltInBinaryName(pDevice, "media_kernels_frontend");

    cl_program ve_program = clCreateProgramWithBuiltInKernels(
        context,
        1,
        &device,
        "ve_enhance_intel;",
        &retVal);

    restoreBuiltInBinaryName(pDevice);

    ASSERT_EQ(retVal, CL_SUCCESS);
    auto pProgram = castToObject<Program>(ve_program);
    auto pKernel = Kernel::create<MockKernel>(pProgram, *pProgram->getKernelInfo("ve_enhance_intel"), &retVal);
    cl_kernel ve_kernel = pKernel;
    ASSERT_EQ(retVal, CL_SUCCESS);

    cl_ve_hpc_attrib_intel HPCConfig = {true, 0, 0};
    cl_ve_attrib_desc_intel attrib = {
        CL_VE_ACCELERATOR_ATTRIB_HPC_INTEL,
        &HPCConfig};

    cl_ve_desc_intel desc = {1, &attrib};

    cl_accelerator_intel accelerator = nullptr;
    accelerator = clCreateAcceleratorINTEL(
        context,
        CL_ACCELERATOR_TYPE_VE_INTEL,
        sizeof(cl_ve_desc_intel),
        &desc,
        &retVal);

    ASSERT_EQ(retVal, CL_SUCCESS);
    ASSERT_NE(accelerator, nullptr);

    // NV12 on input
    int inputBpp = 12;
    // 32bit RGBA on output
    int outputBpp = 32;

    const int INPUT_SIZE = (testWidth * testHeight * inputBpp) / 8;
    ASSERT_GT(INPUT_SIZE, 0);

    const int Y_SIZE = testWidth * testHeight;
    ASSERT_GT(Y_SIZE, 0);

    const int UV_SIZE = INPUT_SIZE - Y_SIZE;
    ASSERT_GT(UV_SIZE, 0);

    const int OUTPUT_SIZE = (testWidth * testHeight * outputBpp) / 8;
    ASSERT_GT(OUTPUT_SIZE, 0);

    auto srcMemory = (cl_uchar *)::alignedMalloc(INPUT_SIZE, 4);
    ASSERT_NE(srcMemory, nullptr);
    memset(srcMemory, 0x00, INPUT_SIZE);

    // Init sample input (in sync with MICRO_VEBOX SLT test)
    // Luma:
    for (int i = 0; i < 7; i++) {
        srcMemory[2 * testWidth + 3 + i] = 0xFD;
        srcMemory[3 * testWidth + 5 + i] = 0xFD;
        srcMemory[10 * testWidth + 3 + i] = 0xFD;
        srcMemory[12 * testWidth + 5 + i] = 0xFD;
        srcMemory[13 * testWidth + 2 + i] = 0xFD;
    }
    srcMemory[12 * testWidth + 12] = 0xFD;
    srcMemory[13 * testWidth + 9] = 0xFD;

    // Chroma:
    memset(srcMemory + Y_SIZE, 0x80, UV_SIZE);

    cl_image_format inputFormatNV12;
    inputFormatNV12.image_channel_order = CL_NV12_INTEL;
    inputFormatNV12.image_channel_data_type = CL_UNORM_INT8;

    cl_image_format inputFormatY;
    inputFormatY.image_channel_order = CL_R;
    inputFormatY.image_channel_data_type = CL_UNORM_INT8;

    cl_image_format inputFormatUV;
    inputFormatUV.image_channel_order = CL_RG;
    inputFormatUV.image_channel_data_type = CL_UNORM_INT8;

    cl_image_format outputFormat;
    outputFormat.image_channel_order = CL_RGBA;
    outputFormat.image_channel_data_type = CL_UNORM_INT8;

    cl_image_desc imageDesc;
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = testWidth;
    imageDesc.image_height = testHeight;
    imageDesc.image_depth = 1;
    imageDesc.image_array_size = 1;
    imageDesc.image_row_pitch = testWidth;
    imageDesc.image_slice_pitch = 0;
    imageDesc.num_mip_levels = 0;
    imageDesc.num_samples = 0;
    imageDesc.mem_object = nullptr;

    cl_image_desc imageDescY = imageDesc;
    imageDescY.image_depth = 0;

    cl_image_desc imageDescUV = imageDesc;
    imageDescUV.image_width /= 2;
    imageDescUV.image_height /= 2;

    cl_mem_flags flagsNV12 = CL_MEM_NO_ACCESS_INTEL | CL_MEM_HOST_NO_ACCESS | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL;
    auto surfaceFormatNV12 = Image::getSurfaceFormatFromTable(flagsNV12, &inputFormatNV12);

    auto srcImage = Image::create(
        context,
        flagsNV12,
        surfaceFormatNV12,
        &imageDesc,
        nullptr,
        retVal);
    ASSERT_NE(nullptr, srcImage);

    cl_mem_flags flagsY = CL_MEM_READ_WRITE;
    auto surfaceFormatY = Image::getSurfaceFormatFromTable(flagsY, &inputFormatY);
    auto srcPlaneY = Image::create(
        context,
        flagsY,
        surfaceFormatY,
        &imageDescY,
        nullptr,
        retVal);
    ASSERT_NE(nullptr, srcPlaneY);

    cl_mem_flags flagsUV = CL_MEM_READ_WRITE;
    auto surfaceFormatUV = Image::getSurfaceFormatFromTable(flagsUV, &inputFormatUV);
    auto srcPlaneUV = Image::create(
        context,
        flagsUV,
        surfaceFormatUV,
        &imageDescUV,
        nullptr,
        retVal);
    ASSERT_NE(nullptr, srcPlaneUV);

    cl_mem_flags flagsOut = CL_MEM_READ_WRITE;
    auto surfaceFormatOut = Image::getSurfaceFormatFromTable(flagsOut, &outputFormat);
    auto dstImage = Image::create(
        context,
        flagsOut,
        surfaceFormatOut,
        &imageDesc,
        nullptr,
        retVal);
    ASSERT_NE(nullptr, dstImage);

    cl_int flags = 0;
    cl_mem arg2 = srcImage;
    cl_mem arg3 = dstImage;

    retVal = clSetKernelArg(ve_kernel, 0, sizeof(cl_accelerator_intel), &accelerator);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(ve_kernel, 1, sizeof(cl_int), &flags);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(ve_kernel, 2, sizeof(cl_mem), &arg2);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(ve_kernel, 3, sizeof(cl_mem), &arg3);
    ASSERT_EQ(CL_SUCCESS, retVal);

    CommandQueue *pVeCmdQ = createCommandQueue(
        pDevice,
        CL_QUEUE_VE_ENABLE_INTEL);
    ASSERT_NE(nullptr, pVeCmdQ);

    const cl_uint workDim = 1;
    const size_t globalWorkOffset[3] = {0, 0, 0};
    const size_t globalWorkSize[3] = {testWidth, testHeight, 1};
    const size_t localWorkSize[3] = {testWidth, testHeight, 1};
    cl_uint numEventsInWaitList = 0;

    retVal = pVeCmdQ->enqueueKernel(
        ve_kernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        nullptr,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    ::alignedFree(srcMemory);
    clReleaseMemObject(srcPlaneY);
    clReleaseMemObject(srcPlaneUV);
    clReleaseMemObject(srcImage);
    clReleaseMemObject(dstImage);
    clReleaseCommandQueue(pVeCmdQ);
    clReleaseAcceleratorINTEL(accelerator);
    clReleaseKernel(ve_kernel);
    clReleaseProgram(ve_program);
}
} // namespace ULT
