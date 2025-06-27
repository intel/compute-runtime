/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/kernel_arg_fixture.h"

#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

using namespace NEO;

KernelImageArgTest::~KernelImageArgTest() = default;

void KernelImageArgTest::SetUp() {
    pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

    pKernelInfo->heapInfo.surfaceStateHeapSize = sizeof(surfaceStateHeap);
    pKernelInfo->heapInfo.pSsh = surfaceStateHeap;

    constexpr int numImages = 5;

    pKernelInfo->addArgImage(0, 0);
    auto &img0 = pKernelInfo->argAsImg(0);
    img0.metadataPayload.imgWidth = 0x4;
    img0.metadataPayload.flatBaseOffset = 0x8;
    img0.metadataPayload.flatWidth = 0x10;
    img0.metadataPayload.flatHeight = 0x18;
    img0.metadataPayload.flatPitch = 0x24;
    img0.metadataPayload.numSamples = 0x3c;
    img0.metadataPayload.numMipLevels = offsetNumMipLevelsImage0;

    pKernelInfo->addArgImage(1, 0);
    pKernelInfo->argAsImg(1).metadataPayload.imgHeight = 0xc;

    pKernelInfo->addArgImmediate(2, sizeof(void *), 0x20);

    pKernelInfo->addArgImage(3, 0);

    pKernelInfo->addArgImage(4, 0x20);

    pKernelInfo->kernelDescriptor.kernelAttributes.bufferAddressingMode = debugManager.flags.UseBindlessMode.get() == 1 ? KernelDescriptor::AddressingMode::BindlessAndStateless : KernelDescriptor::AddressingMode::BindfulAndStateless;
    pKernelInfo->kernelDescriptor.kernelAttributes.imageAddressingMode = debugManager.flags.UseBindlessMode.get() == 1 ? KernelDescriptor::AddressingMode::Bindless : KernelDescriptor::AddressingMode::Bindful;

    ClDeviceFixture::setUp();
    context.reset(new MockContext(pClDevice));
    program = std::make_unique<MockProgram>(context.get(), false, toClDeviceVector(*pClDevice));
    int32_t retVal = CL_INVALID_VALUE;
    pMultiDeviceKernel.reset(MultiDeviceKernel::create<MockKernel>(program.get(), MockKernel::toKernelInfoContainer(*pKernelInfo, rootDeviceIndex), retVal));
    pKernel = static_cast<MockKernel *>(pMultiDeviceKernel->getKernel(rootDeviceIndex));
    ASSERT_EQ(CL_SUCCESS, retVal);

    pKernel->setKernelArgHandler(0, &Kernel::setArgImage);
    pKernel->setKernelArgHandler(1, &Kernel::setArgImage);
    pKernel->setKernelArgHandler(2, &Kernel::setArgImmediate);
    pKernel->setKernelArgHandler(3, &Kernel::setArgImage);
    pKernel->setKernelArgHandler(4, &Kernel::setArgImage);

    uint32_t crossThreadData[numImages * 0x20] = {};
    crossThreadData[0x20 / sizeof(uint32_t)] = 0x12344321;
    pKernel->setCrossThreadData(crossThreadData, sizeof(crossThreadData));

    image.reset(Image2dHelperUlt<>::create(context.get()));
    ASSERT_NE(nullptr, image);
}

void KernelImageArgTest::TearDown() {
    image.reset();
    pMultiDeviceKernel.reset();
    program.reset();
    context.reset();
    ClDeviceFixture::tearDown();
}
