/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "gtest/gtest.h"

using namespace NEO;

class KernelSlmArgTest : public MultiRootDeviceWithSubDevicesFixture {
  protected:
    void SetUp() override {
        MultiRootDeviceWithSubDevicesFixture::SetUp();

        program = std::make_unique<MockProgram>(context.get(), false, context->getDevices());
        pKernelInfo = std::make_unique<MockKernelInfo>();

        KernelVectorType kernels;
        kernels.resize(3);

        KernelInfoContainer kernelInfos;
        kernelInfos.resize(3);
        kernelInfos[0] = kernelInfos[1] = kernelInfos[2] = pKernelInfo.get();

        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

        pKernelInfo->addArgLocal(0, 0x10, 0x1);

        pKernelInfo->addArgBuffer(1, 0x20, sizeof(void *));

        pKernelInfo->addArgLocal(2, 0x30, 0x10);

        pKernelInfo->kernelDescriptor.kernelAttributes.slmInlineSize = 3 * MemoryConstants::kiloByte;

        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
            pKernel[rootDeviceIndex] = new MockKernel(program.get(), *pKernelInfo, *deviceFactory->rootDevices[rootDeviceIndex]);
            kernels[rootDeviceIndex] = pKernel[rootDeviceIndex];
            ASSERT_EQ(CL_SUCCESS, pKernel[rootDeviceIndex]->initialize());
        }

        pMultiDeviceKernel = std::make_unique<MultiDeviceKernel>(kernels, kernelInfos);
        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
            crossThreadData[rootDeviceIndex][0x20 / sizeof(uint32_t)] = 0x12344321;

            pKernel[rootDeviceIndex]->setCrossThreadData(&crossThreadData[rootDeviceIndex], sizeof(crossThreadData[rootDeviceIndex]));
        }
    }

    void TearDown() override {
        MultiRootDeviceWithSubDevicesFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<MockProgram> program;
    MockKernel *pKernel[3] = {nullptr};
    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel;
    std::unique_ptr<MockKernelInfo> pKernelInfo;

    static const size_t slmSize0 = 0x200;
    static const size_t slmSize2 = 0x30;
    uint32_t crossThreadData[3][0x40]{};
};

TEST_F(KernelSlmArgTest, WhenSettingSizeThenAlignmentOfHigherSlmArgsIsUpdated) {
    pMultiDeviceKernel->setArg(0, slmSize0, nullptr);
    pMultiDeviceKernel->setArg(2, slmSize2, nullptr);

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        auto crossThreadData = reinterpret_cast<uint32_t *>(pKernel[rootDeviceIndex]->getCrossThreadData());
        auto slmOffset = ptrOffset(crossThreadData, 0x10);
        EXPECT_EQ(0u, *slmOffset);

        slmOffset = ptrOffset(crossThreadData, 0x20);
        EXPECT_EQ(0x12344321u, *slmOffset);

        slmOffset = ptrOffset(crossThreadData, 0x30);
        EXPECT_EQ(0x200u, *slmOffset);

        EXPECT_EQ(4 * MemoryConstants::kiloByte, pKernel[rootDeviceIndex]->slmTotalSize);
    }
}

TEST_F(KernelSlmArgTest, GivenReverseOrderWhenSettingSizeThenAlignmentOfHigherSlmArgsIsUpdated) {
    pMultiDeviceKernel->setArg(2, slmSize2, nullptr);
    pMultiDeviceKernel->setArg(0, slmSize0, nullptr);

    for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
        auto crossThreadData = reinterpret_cast<uint32_t *>(pKernel[rootDeviceIndex]->getCrossThreadData());
        auto slmOffset = ptrOffset(crossThreadData, 0x10);
        EXPECT_EQ(0u, *slmOffset);

        slmOffset = ptrOffset(crossThreadData, 0x20);
        EXPECT_EQ(0x12344321u, *slmOffset);

        slmOffset = ptrOffset(crossThreadData, 0x30);
        EXPECT_EQ(0x200u, *slmOffset);

        EXPECT_EQ(4 * MemoryConstants::kiloByte, pKernel[rootDeviceIndex]->slmTotalSize);
    }
}
