/*
 * Copyright (c) 2018, Intel Corporation
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

#include "unit_tests/fixtures/kernel_arg_fixture.h"

#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/mocks/mock_image.h"

KernelImageArgTest::~KernelImageArgTest() = default;

void KernelImageArgTest::SetUp() {
    pKernelInfo.reset(OCLRT::KernelInfo::create());
    KernelArgPatchInfo kernelArgPatchInfo;
    kernelHeader.reset(new iOpenCL::SKernelBinaryHeaderCommon{});

    kernelHeader->SurfaceStateHeapSize = sizeof(surfaceStateHeap);
    pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
    pKernelInfo->heapInfo.pKernelHeader = kernelHeader.get();
    pKernelInfo->usesSsh = true;

    pKernelInfo->kernelArgInfo.resize(5);
    pKernelInfo->kernelArgInfo[4].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
    pKernelInfo->kernelArgInfo[3].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
    pKernelInfo->kernelArgInfo[2].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
    pKernelInfo->kernelArgInfo[1].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
    pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);

    pKernelInfo->kernelArgInfo[0].offsetImgWidth = 0x4;
    pKernelInfo->kernelArgInfo[0].offsetNumSamples = 0x3c;
    offsetNumMipLevelsImage0 = 0x40;
    pKernelInfo->kernelArgInfo[0].offsetNumMipLevels = offsetNumMipLevelsImage0;
    pKernelInfo->kernelArgInfo[1].offsetImgHeight = 0xc;
    pKernelInfo->kernelArgInfo[2].kernelArgPatchInfoVector[0].crossthreadOffset = 0x20;
    pKernelInfo->kernelArgInfo[2].kernelArgPatchInfoVector[0].size = sizeof(void *);
    pKernelInfo->kernelArgInfo[3].offsetImgDepth = 0x30;
    pKernelInfo->kernelArgInfo[4].offsetHeap = 0x20;
    pKernelInfo->kernelArgInfo[4].offsetObjectId = 0x0;

    pKernelInfo->kernelArgInfo[4].isImage = true;
    pKernelInfo->kernelArgInfo[3].isImage = true;
    pKernelInfo->kernelArgInfo[2].isImage = true;
    pKernelInfo->kernelArgInfo[1].isImage = true;
    pKernelInfo->kernelArgInfo[0].isImage = true;

    DeviceFixture::SetUp();
    program = std::make_unique<OCLRT::MockProgram>(*pDevice->getExecutionEnvironment());
    pKernel.reset(new OCLRT::MockKernel(program.get(), *pKernelInfo, *pDevice));
    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    pKernel->setKernelArgHandler(0, &OCLRT::Kernel::setArgImage);
    pKernel->setKernelArgHandler(1, &OCLRT::Kernel::setArgImage);
    pKernel->setKernelArgHandler(2, &OCLRT::Kernel::setArgImmediate);
    pKernel->setKernelArgHandler(3, &OCLRT::Kernel::setArgImage);
    pKernel->setKernelArgHandler(4, &OCLRT::Kernel::setArgImage);

    uint32_t crossThreadData[0x44] = {};
    crossThreadData[0x20 / sizeof(uint32_t)] = 0x12344321;
    pKernel->setCrossThreadData(crossThreadData, sizeof(crossThreadData));

    context.reset(new OCLRT::MockContext(pDevice));
    image.reset(Image2dHelper<>::create(context.get()));
    ASSERT_NE(nullptr, image);
}

void KernelImageArgTest::TearDown() {
    image.reset();
    pKernel.reset();
    program.reset();
    context.reset();
    DeviceFixture::TearDown();
}
