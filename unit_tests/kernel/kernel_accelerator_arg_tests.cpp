/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/accelerators/intel_accelerator.h"
#include "runtime/accelerators/intel_motion_estimation.h"
#include "runtime/kernel/kernel.h"
#include "runtime/mem_obj/buffer.h"
#include "test.h"
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"

#include "CL/cl.h"
#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

class KernelArgAcceleratorFixture : public ContextFixture, public DeviceFixture {

    using ContextFixture::SetUp;

  public:
    KernelArgAcceleratorFixture() {
    }

  protected:
    void SetUp() {
        desc = {
            CL_ME_MB_TYPE_4x4_INTEL,
            CL_ME_SUBPIXEL_MODE_QPEL_INTEL,
            CL_ME_SAD_ADJUST_MODE_HAAR_INTEL,
            CL_ME_SEARCH_PATH_RADIUS_16_12_INTEL};

        DeviceFixture::SetUp();
        cl_device_id device = pClDevice;
        ContextFixture::SetUp(1, &device);

        pKernelInfo = std::make_unique<KernelInfo>();
        KernelArgPatchInfo kernelArgPatchInfo;

        pKernelInfo->kernelArgInfo.resize(1);
        pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);

        pKernelInfo->kernelArgInfo[0].samplerArgumentType = iOpenCL::SAMPLER_OBJECT_VME;
        pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset = 0x20;
        pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].size = (uint32_t)sizeof(uint32_t);
        pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].size = 1;

        pKernelInfo->kernelArgInfo[0].offsetVmeMbBlockType = 0x04;
        pKernelInfo->kernelArgInfo[0].offsetVmeSubpixelMode = 0x0c;
        pKernelInfo->kernelArgInfo[0].offsetVmeSadAdjustMode = 0x14;
        pKernelInfo->kernelArgInfo[0].offsetVmeSearchPathType = 0x1c;

        pProgram = new MockProgram(*pDevice->getExecutionEnvironment(), pContext, false);
        pKernel = new MockKernel(pProgram, *pKernelInfo, *pClDevice);
        ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

        pKernel->setKernelArgHandler(0, &Kernel::setArgAccelerator);

        pCrossThreadData[0x04] = desc.mb_block_type;
        pCrossThreadData[0x0c] = desc.subpixel_mode;
        pCrossThreadData[0x14] = desc.sad_adjust_mode;
        pCrossThreadData[0x1c] = desc.sad_adjust_mode;

        pKernel->setCrossThreadData(&pCrossThreadData[0], sizeof(pCrossThreadData));
    }

    void TearDown() override {
        delete pKernel;

        delete pProgram;
        ContextFixture::TearDown();
        DeviceFixture::TearDown();
    }

    cl_motion_estimation_desc_intel desc;
    MockProgram *pProgram = nullptr;
    MockKernel *pKernel = nullptr;
    std::unique_ptr<KernelInfo> pKernelInfo;
    char pCrossThreadData[64];
};

typedef Test<KernelArgAcceleratorFixture> KernelArgAcceleratorTest;

TEST_F(KernelArgAcceleratorTest, SetKernelArgValidAccelerator) {
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

    const auto &arginfo = pKernelInfo->kernelArgInfo[0];

    uint32_t *pMbBlockType = ptrOffset(reinterpret_cast<uint32_t *>(crossThreadData),
                                       arginfo.offsetVmeMbBlockType);
    EXPECT_EQ(desc.mb_block_type, *pMbBlockType);

    uint32_t *pSubpixelMode = ptrOffset(reinterpret_cast<uint32_t *>(crossThreadData),
                                        arginfo.offsetVmeSubpixelMode);
    EXPECT_EQ(desc.subpixel_mode, *pSubpixelMode);

    uint32_t *pSadAdjustMode = ptrOffset(reinterpret_cast<uint32_t *>(crossThreadData),
                                         arginfo.offsetVmeSadAdjustMode);
    EXPECT_EQ(desc.sad_adjust_mode, *pSadAdjustMode);

    uint32_t *pSearchPathType = ptrOffset(reinterpret_cast<uint32_t *>(crossThreadData),
                                          arginfo.offsetVmeSearchPathType);
    EXPECT_EQ(desc.search_path_type, *pSearchPathType);

    status = clReleaseAcceleratorINTEL(accelerator);
    EXPECT_EQ(CL_SUCCESS, status);
}

TEST_F(KernelArgAcceleratorTest, SetKernelArgNullAccelerator) {
    cl_int status;

    status = this->pKernel->setArg(0, sizeof(cl_accelerator_intel), nullptr);
    ASSERT_EQ(CL_INVALID_ARG_VALUE, status);
}

TEST_F(KernelArgAcceleratorTest, SetKernelArgInvalidSize) {
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

TEST_F(KernelArgAcceleratorTest, SetKernelArgInvalidAccelerator) {
    cl_int status;

    const void *notAnAccelerator = static_cast<void *>(pKernel);

    status = this->pKernel->setArg(0, sizeof(cl_accelerator_intel), notAnAccelerator);
    ASSERT_EQ(CL_INVALID_ARG_VALUE, status);
}
