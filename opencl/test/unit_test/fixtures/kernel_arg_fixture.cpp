/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/kernel_arg_fixture.h"

#include "opencl/source/program/kernel_info.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_image.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

using namespace NEO;

KernelImageArgTest::~KernelImageArgTest() = default;

void KernelImageArgTest::SetUp() {
    pKernelInfo = std::make_unique<KernelInfo>();
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
    pKernelInfo->kernelArgInfo[0].offsetFlatBaseOffset = 0x8;
    pKernelInfo->kernelArgInfo[0].offsetFlatWidth = 0x10;
    pKernelInfo->kernelArgInfo[0].offsetFlatHeight = 0x18;
    pKernelInfo->kernelArgInfo[0].offsetFlatPitch = 0x24;
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
    program = std::make_unique<MockProgram>(*pDevice->getExecutionEnvironment());
    pKernel.reset(new MockKernel(program.get(), *pKernelInfo, *pClDevice));
    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    pKernel->setKernelArgHandler(0, &Kernel::setArgImage);
    pKernel->setKernelArgHandler(1, &Kernel::setArgImage);
    pKernel->setKernelArgHandler(2, &Kernel::setArgImmediate);
    pKernel->setKernelArgHandler(3, &Kernel::setArgImage);
    pKernel->setKernelArgHandler(4, &Kernel::setArgImage);

    uint32_t crossThreadData[0x44] = {};
    crossThreadData[0x20 / sizeof(uint32_t)] = 0x12344321;
    pKernel->setCrossThreadData(crossThreadData, sizeof(crossThreadData));

    context.reset(new MockContext(pClDevice));
    image.reset(Image2dHelper<>::create(context.get()));
    pKernel->setContext(context.get());
    ASSERT_NE(nullptr, image);
}

void KernelImageArgTest::TearDown() {
    image.reset();
    pKernel.reset();
    program.reset();
    context.reset();
    DeviceFixture::TearDown();
}
