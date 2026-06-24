/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
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

        EXPECT_EQ(4 * MemoryConstants::kiloByte, pKernel[rootDeviceIndex]->slmTotalSizePerThreadGroup);
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

        EXPECT_EQ(4 * MemoryConstants::kiloByte, pKernel[rootDeviceIndex]->slmTotalSizePerThreadGroup);
    }
}

class KernelSlmAllocationModeTest : public ::testing::Test {
  protected:
    void SetUp() override {
        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
        program = std::make_unique<MockProgram>(toClDeviceVector(*device));

        pKernelInfo = std::make_unique<MockKernelInfo>();
        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
        pKernelInfo->setCrossThreadDataSize(crossThreadDataSize);

        pKernelInfo->addArgLocal(0, arg0SlmOffset, arg0Alignment);
        pKernelInfo->addArgLocal(1, arg1SlmOffset, arg1Alignment);
        pKernelInfo->kernelDescriptor.kernelAttributes.slmInlineSize = slmInlineSize;
    }

    std::unique_ptr<MockKernel> createKernel(KernelDescriptor::SlmAllocationMode slmAllocationMode) {
        pKernelInfo->kernelDescriptor.kernelAttributes.slmAllocationMode = slmAllocationMode;
        auto kernel = std::make_unique<MockKernel>(program.get(), *pKernelInfo, *device);
        EXPECT_EQ(CL_SUCCESS, kernel->initialize());
        return kernel;
    }

    static uint32_t readSlmOffset(MockKernel &kernel, CrossThreadDataOffset offset) {
        return *reinterpret_cast<uint32_t *>(ptrOffset(kernel.getCrossThreadData(), offset));
    }

    static constexpr CrossThreadDataOffset arg0SlmOffset = 0x20;
    static constexpr CrossThreadDataOffset arg1SlmOffset = 0x40;
    static constexpr uint8_t arg0Alignment = 4u;
    static constexpr uint8_t arg1Alignment = 8u;
    static constexpr uint32_t slmInlineSize = 256u;
    static constexpr uint16_t crossThreadDataSize = 0x100;
    static constexpr size_t arg0Size = 64u;
    static constexpr size_t arg1Size = 32u;

    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockProgram> program;
    std::unique_ptr<MockKernelInfo> pKernelInfo;
};

TEST_F(KernelSlmAllocationModeTest, givenCompilerResolvedSlmAllocationModeWhenSettingLocalArgsThenOffsetsInSlmStartAtZero) {
    auto kernel = createKernel(KernelDescriptor::SlmAllocationMode::compilerResolved);

    EXPECT_EQ(CL_SUCCESS, kernel->setArg(0, arg0Size, nullptr));
    EXPECT_EQ(CL_SUCCESS, kernel->setArg(1, arg1Size, nullptr));

    EXPECT_EQ(0u, readSlmOffset(*kernel, arg0SlmOffset));
    EXPECT_EQ(static_cast<uint32_t>(arg0Size), readSlmOffset(*kernel, arg1SlmOffset));
}

TEST_F(KernelSlmAllocationModeTest, givenRuntimeAdjustedSlmAllocationModeWhenSettingLocalArgsThenOffsetsInSlmAreShiftedByInlineSlmSize) {
    auto kernel = createKernel(KernelDescriptor::SlmAllocationMode::runtimeAdjusted);

    // explicit SLM args are patched during kernel initialization
    EXPECT_EQ(slmInlineSize, readSlmOffset(*kernel, arg0SlmOffset));
    EXPECT_EQ(slmInlineSize, readSlmOffset(*kernel, arg1SlmOffset));

    EXPECT_EQ(CL_SUCCESS, kernel->setArg(0, arg0Size, nullptr));
    EXPECT_EQ(CL_SUCCESS, kernel->setArg(1, arg1Size, nullptr));

    EXPECT_EQ(slmInlineSize, readSlmOffset(*kernel, arg0SlmOffset));
    EXPECT_EQ(slmInlineSize + static_cast<uint32_t>(arg0Size), readSlmOffset(*kernel, arg1SlmOffset));
}
