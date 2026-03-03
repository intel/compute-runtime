/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_kernel_info.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/sampler/sampler.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "cl_api_tests.h"

using namespace NEO;

class KernelArgSamplerApiFixture : public ApiFixture {
  protected:
    void setUp() {
        ApiFixture::setUp();

        pKernelInfo = std::make_unique<MockKernelInfo>();
        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

        pKernelInfo->heapInfo.pDsh = samplerStateHeap;
        pKernelInfo->heapInfo.dynamicStateHeapSize = sizeof(samplerStateHeap);

        pKernelInfo->addArgSampler(0, 0x40);

        cl_int retVal{CL_SUCCESS};
        pMockMultiDeviceKernel = MultiDeviceKernel::create<MockKernel>(pProgram, MockKernel::toKernelInfoContainer(*pKernelInfo, testedRootDeviceIndex), retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
        pMockKernel = static_cast<MockKernel *>(pMockMultiDeviceKernel->getKernel(testedRootDeviceIndex));
        ASSERT_NE(nullptr, pMockKernel);

        pMockKernel->setKernelArgHandler(0, &Kernel::setArgSampler);

        uint32_t crossThreadData[0x40] = {};
        pMockKernel->setCrossThreadData(crossThreadData, sizeof(crossThreadData));

        pSampler = Sampler::create(pContext, CL_TRUE, CL_ADDRESS_MIRRORED_REPEAT, CL_FILTER_NEAREST, retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, pSampler);
    }

    void tearDown() {
        if (pSampler) {
            pSampler->release();
        }
        if (pMockMultiDeviceKernel) {
            delete pMockMultiDeviceKernel;
        }

        ApiFixture::tearDown();
    }

    MockKernel *pMockKernel = nullptr;
    MultiDeviceKernel *pMockMultiDeviceKernel = nullptr;
    std::unique_ptr<MockKernelInfo> pKernelInfo;
    char samplerStateHeap[0x80];
    Sampler *pSampler = nullptr;
};

using clSetKernelArgSamplerTests = Test<KernelArgSamplerApiFixture>;

TEST_F(clSetKernelArgSamplerTests, GivenSamplerArgAndArgSizeSmallerThanRequiredWhenSettingKernelArgThenInvalidArgSizeIsReturned) {
    cl_sampler samplerObj = pSampler;

    auto retVal = clSetKernelArg(pMockMultiDeviceKernel, 0, sizeof(cl_sampler) / 2, &samplerObj);
    EXPECT_EQ(CL_INVALID_ARG_SIZE, retVal);
}

TEST_F(clSetKernelArgSamplerTests, GivenSamplerArgAndArgSizeLargerThanRequiredWhenSettingKernelArgThenInvalidArgSizeIsReturned) {
    cl_sampler samplerObj = pSampler;

    auto retVal = clSetKernelArg(pMockMultiDeviceKernel, 0, sizeof(cl_sampler) * 2, &samplerObj);
    EXPECT_EQ(CL_INVALID_ARG_SIZE, retVal);
}

TEST_F(clSetKernelArgSamplerTests, GivenSamplerArgAndExactArgSizeWhenSettingKernelArgThenSuccessIsReturned) {
    cl_sampler samplerObj = pSampler;

    auto retVal = clSetKernelArg(pMockMultiDeviceKernel, 0, sizeof(cl_sampler), &samplerObj);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
