/*
 * Copyright (C) 2017-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/ptr_math.h"

#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "test.h"

#include "gtest/gtest.h"

using namespace NEO;

class KernelSlmArgTest : public MultiRootDeviceWithSubDevicesFixture {
  protected:
    void SetUp() override {
        MultiRootDeviceWithSubDevicesFixture::SetUp();

        program = std::make_unique<MockProgram>(context.get(), false, context->getDevices());
        KernelInfoContainer kernelInfos;
        kernelInfos.resize(3);
        KernelVectorType kernels;
        kernels.resize(3);
        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {

            pKernelInfo[rootDeviceIndex] = std::make_unique<KernelInfo>();
            pKernelInfo[rootDeviceIndex]->kernelDescriptor.kernelAttributes.simdSize = 1;

            KernelArgPatchInfo kernelArgPatchInfo;

            pKernelInfo[rootDeviceIndex]->kernelArgInfo.resize(3);
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[1].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[2].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);

            pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset = 0x10;
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].slmAlignment = 0x1;
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[0].metadata.addressQualifier = KernelArgMetadata::AddrLocal;
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[1].kernelArgPatchInfoVector[0].crossthreadOffset = 0x20;
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[1].kernelArgPatchInfoVector[0].size = sizeof(void *);
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[2].kernelArgPatchInfoVector[0].crossthreadOffset = 0x30;
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[2].slmAlignment = 0x400;
            pKernelInfo[rootDeviceIndex]->kernelArgInfo[2].metadata.addressQualifier = KernelArgMetadata::AddrLocal;
            pKernelInfo[rootDeviceIndex]->kernelDescriptor.kernelAttributes.slmInlineSize = 3 * KB;
            kernelInfos[rootDeviceIndex] = pKernelInfo[rootDeviceIndex].get();
        }

        for (auto &rootDeviceIndex : this->context->getRootDeviceIndices()) {
            pKernel[rootDeviceIndex] = new MockKernel(program.get(), *pKernelInfo[rootDeviceIndex], *deviceFactory->rootDevices[rootDeviceIndex]);
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
    std::unique_ptr<KernelInfo> pKernelInfo[3];

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
        EXPECT_EQ(0x400u, *slmOffset);

        EXPECT_EQ(5 * KB, pKernel[rootDeviceIndex]->slmTotalSize);
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
        EXPECT_EQ(0x400u, *slmOffset);

        EXPECT_EQ(5 * KB, pKernel[rootDeviceIndex]->slmTotalSize);
    }
}
