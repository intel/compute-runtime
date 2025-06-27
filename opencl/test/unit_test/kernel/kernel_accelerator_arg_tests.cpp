/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/source/accelerators/intel_motion_estimation.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "CL/cl.h"
#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

class KernelArgAcceleratorFixture : public ContextFixture, public ClDeviceFixture {

    using ContextFixture::setUp;

  public:
    KernelArgAcceleratorFixture() {
    }

  protected:
    void setUp() {
        desc = {
            CL_ME_MB_TYPE_4x4_INTEL,
            CL_ME_SUBPIXEL_MODE_QPEL_INTEL,
            CL_ME_SAD_ADJUST_MODE_HAAR_INTEL,
            CL_ME_SEARCH_PATH_RADIUS_16_12_INTEL};

        ClDeviceFixture::setUp();
        cl_device_id device = pClDevice;
        ContextFixture::setUp(1, &device);

        pKernelInfo = std::make_unique<MockKernelInfo>();
        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

        pKernelInfo->addArgAccelerator(0, 0x20, 0x04, 0x14, 0x1c, 0x0c);

        pProgram = new MockProgram(pContext, false, toClDeviceVector(*pClDevice));
        pKernel = new MockKernel(pProgram, *pKernelInfo, *pClDevice);
        ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

        pKernel->setKernelArgHandler(0, &Kernel::setArgAccelerator);

        pCrossThreadData[0x04] = desc.mb_block_type;
        pCrossThreadData[0x0c] = desc.subpixel_mode;
        pCrossThreadData[0x14] = desc.sad_adjust_mode;
        pCrossThreadData[0x1c] = desc.sad_adjust_mode;

        pKernel->setCrossThreadData(&pCrossThreadData[0], sizeof(pCrossThreadData));
    }

    void tearDown() {
        delete pKernel;

        delete pProgram;
        ContextFixture::tearDown();
        ClDeviceFixture::tearDown();
    }

    cl_motion_estimation_desc_intel desc;
    MockProgram *pProgram = nullptr;
    MockKernel *pKernel = nullptr;
    std::unique_ptr<MockKernelInfo> pKernelInfo;
    char pCrossThreadData[64];
};

typedef Test<KernelArgAcceleratorFixture> KernelArgAcceleratorTest;

TEST_F(KernelArgAcceleratorTest, WhenCreatingVmeAcceleratorThenCorrectKernelArgsAreSet) {
    cl_int status;
    cl_accelerator_intel accelerator = VmeAccelerator::create(
        pContext,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL, sizeof(desc), &desc,
        status);
    ASSERT_EQ(CL_SUCCESS, status);
    ASSERT_NE(nullptr, accelerator);

    status = this->pKernel->setArg(0, sizeof(cl_accelerator_intel), &accelerator);
    ASSERT_EQ(CL_SUCCESS, status);

    char *crossThreadData = pKernel->getCrossThreadData();

    const auto vmeDescriptor = reinterpret_cast<ArgDescVme *>(pKernelInfo->kernelDescriptor.payloadMappings.explicitArgsExtendedDescriptors[0].get());

    uint32_t *pMbBlockType = ptrOffset(reinterpret_cast<uint32_t *>(crossThreadData), vmeDescriptor->mbBlockType);
    EXPECT_EQ(desc.mb_block_type, *pMbBlockType);

    uint32_t *pSubpixelMode = ptrOffset(reinterpret_cast<uint32_t *>(crossThreadData), vmeDescriptor->subpixelMode);
    EXPECT_EQ(desc.subpixel_mode, *pSubpixelMode);

    uint32_t *pSadAdjustMode = ptrOffset(reinterpret_cast<uint32_t *>(crossThreadData), vmeDescriptor->sadAdjustMode);
    EXPECT_EQ(desc.sad_adjust_mode, *pSadAdjustMode);

    uint32_t *pSearchPathType = ptrOffset(reinterpret_cast<uint32_t *>(crossThreadData), vmeDescriptor->searchPathType);
    EXPECT_EQ(desc.search_path_type, *pSearchPathType);

    status = clReleaseAcceleratorINTEL(accelerator);
    EXPECT_EQ(CL_SUCCESS, status);
}

TEST_F(KernelArgAcceleratorTest, GivenNullWhenSettingKernelArgThenInvalidArgValueErrorIsReturned) {
    cl_int status;

    status = this->pKernel->setArg(0, sizeof(cl_accelerator_intel), nullptr);
    ASSERT_EQ(CL_INVALID_ARG_VALUE, status);
}

TEST_F(KernelArgAcceleratorTest, GivenInvalidSizeWhenSettingKernelArgThenInvalidArgSizeErrorIsReturned) {
    cl_int status;
    cl_accelerator_intel accelerator = VmeAccelerator::create(
        pContext,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL, sizeof(desc), &desc,
        status);
    ASSERT_EQ(CL_SUCCESS, status);
    ASSERT_NE(nullptr, accelerator);

    status = this->pKernel->setArg(0, sizeof(cl_accelerator_intel) - 1, accelerator);
    ASSERT_EQ(CL_INVALID_ARG_SIZE, status);

    status = clReleaseAcceleratorINTEL(accelerator);
    EXPECT_EQ(CL_SUCCESS, status);
}

TEST_F(KernelArgAcceleratorTest, GivenInvalidAcceleratorWhenSettingKernelArgThenInvalidArgValueErrorIsReturned) {
    cl_int status;

    const void *notAnAccelerator = static_cast<void *>(pKernel);

    status = this->pKernel->setArg(0, sizeof(cl_accelerator_intel), notAnAccelerator);
    ASSERT_EQ(CL_INVALID_ARG_VALUE, status);
}
