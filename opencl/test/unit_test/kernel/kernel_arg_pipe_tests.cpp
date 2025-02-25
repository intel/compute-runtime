/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "CL/cl.h"
#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

class KernelArgPipeFixture : public ContextFixture, public ClDeviceFixture {

    using ContextFixture::setUp;

  public:
    KernelArgPipeFixture() {
    }

  protected:
    void setUp() {
        ClDeviceFixture::setUp();
        cl_device_id device = pClDevice;
        ContextFixture::setUp(1, &device);

        // define kernel info
        pKernelInfo = std::make_unique<MockKernelInfo>();
        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

        pKernelInfo->addArgPipe(0, 0x30, sizeof(void *));

        pProgram = new MockProgram(pContext, false, toClDeviceVector(*pClDevice));

        pKernel = new MockKernel(pProgram, *pKernelInfo, *pClDevice);
        ASSERT_EQ(CL_SUCCESS, pKernel->initialize());
        pKernel->setKernelArgHandler(0, &Kernel::setArgPipe);
    }

    void tearDown() {
        delete pKernel;

        delete pProgram;
        ContextFixture::tearDown();
        ClDeviceFixture::tearDown();
    }

    MockProgram *pProgram = nullptr;
    MockKernel *pKernel = nullptr;
    std::unique_ptr<MockKernelInfo> pKernelInfo;
    SKernelBinaryHeaderCommon kernelHeader;
};

using KernelArgPipeTest = Test<KernelArgPipeFixture>;

TEST_F(KernelArgPipeTest, givenPointerWhenSettingKernelArgThenInvalidMemObjIsReturned) {

    auto val = reinterpret_cast<void *>(0xBADF00D);
    auto pVal = &val;

    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(KernelArgPipeTest, givenIncorrectSizeWhenSettingKernelArgThenInvalidMemObjIsReturned) {
    auto val = reinterpret_cast<void *>(0xBADF00D);
    auto pVal = &val;

    auto retVal = this->pKernel->setArg(0, 1, pVal);
    EXPECT_EQ(CL_INVALID_ARG_SIZE, retVal);
}
