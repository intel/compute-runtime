/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/image_set_arg_fixture.h"

#include "shared/test/common/test_macros/test.h"

#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "gtest/gtest.h"
namespace NEO {
void ImageSetArgTest::SetUp() {
    ClDeviceFixture::setUp();
    pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

    // define kernel info
    pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
    pKernelInfo->heapInfo.SurfaceStateHeapSize = sizeof(surfaceStateHeap);

    // setup kernel arg offsets
    pKernelInfo->addArgImage(0, 0x00);
    pKernelInfo->addArgImage(1, 0x40);

    program = std::make_unique<MockProgram>(toClDeviceVector(*pClDevice));
    retVal = CL_INVALID_VALUE;
    pMultiDeviceKernel = MultiDeviceKernel::create<MockKernel>(program.get(), MockKernel::toKernelInfoContainer(*pKernelInfo, rootDeviceIndex), &retVal);
    pKernel = static_cast<MockKernel *>(pMultiDeviceKernel->getKernel(rootDeviceIndex));
    ASSERT_NE(nullptr, pKernel);
    ASSERT_EQ(CL_SUCCESS, retVal);

    pKernel->setKernelArgHandler(0, &Kernel::setArgImage);
    pKernel->setKernelArgHandler(1, &Kernel::setArgImage);
    context = new MockContext(pClDevice);
    srcImage = Image3dHelper<>::create(context);
    srcAllocation = srcImage->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    ASSERT_NE(nullptr, srcImage);
}

void ImageSetArgTest::TearDown() {
    delete srcImage;
    delete pMultiDeviceKernel;

    delete context;
    ClDeviceFixture::tearDown();
};

void ImageMediaBlockSetArgTest::SetUp() {
    ClDeviceFixture::setUp();
    pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

    // define kernel info
    pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
    pKernelInfo->heapInfo.SurfaceStateHeapSize = sizeof(surfaceStateHeap);

    // setup kernel arg offsets
    pKernelInfo->addArgImage(0, 0x00, iOpenCL::IMAGE_MEMORY_OBJECT_2D_MEDIA_BLOCK);
    pKernelInfo->addArgImage(0, 0x40, iOpenCL::IMAGE_MEMORY_OBJECT_2D_MEDIA_BLOCK);

    program = std::make_unique<MockProgram>(toClDeviceVector(*pClDevice));
    retVal = CL_INVALID_VALUE;
    pMultiDeviceKernel = MultiDeviceKernel::create<MockKernel>(program.get(), MockKernel::toKernelInfoContainer(*pKernelInfo, rootDeviceIndex), &retVal);
    pKernel = static_cast<MockKernel *>(pMultiDeviceKernel->getKernel(rootDeviceIndex));
    ASSERT_NE(nullptr, pKernel);
    ASSERT_EQ(CL_SUCCESS, retVal);

    pKernel->setKernelArgHandler(0, &Kernel::setArgImage);
    pKernel->setKernelArgHandler(1, &Kernel::setArgImage);
    context = new MockContext(pClDevice);
    srcImage = Image3dHelper<>::create(context);
    srcAllocation = srcImage->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    ASSERT_NE(nullptr, srcImage);
}
} // namespace NEO