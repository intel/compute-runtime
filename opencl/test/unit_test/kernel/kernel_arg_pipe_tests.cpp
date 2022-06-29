/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/pipe.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_pipe.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "CL/cl.h"
#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

class KernelArgPipeFixture : public ContextFixture, public ClDeviceFixture {

    using ContextFixture::SetUp;

  public:
    KernelArgPipeFixture() {
    }

  protected:
    void SetUp() {
        ClDeviceFixture::SetUp();
        cl_device_id device = pClDevice;
        ContextFixture::SetUp(1, &device);

        // define kernel info
        pKernelInfo = std::make_unique<MockKernelInfo>();
        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

        pKernelInfo->heapInfo.pSsh = pSshLocal;
        pKernelInfo->heapInfo.SurfaceStateHeapSize = sizeof(pSshLocal);

        pKernelInfo->addArgPipe(0, 0x30, sizeof(void *));

        pProgram = new MockProgram(pContext, false, toClDeviceVector(*pClDevice));

        pKernel = new MockKernel(pProgram, *pKernelInfo, *pClDevice);
        ASSERT_EQ(CL_SUCCESS, pKernel->initialize());
        pKernel->setCrossThreadData(pCrossThreadData, sizeof(pCrossThreadData));

        pKernel->setKernelArgHandler(0, &Kernel::setArgPipe);
    }

    void TearDown() {
        delete pKernel;

        delete pProgram;
        ContextFixture::TearDown();
        ClDeviceFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    MockProgram *pProgram = nullptr;
    MockKernel *pKernel = nullptr;
    std::unique_ptr<MockKernelInfo> pKernelInfo;
    SKernelBinaryHeaderCommon kernelHeader;
    char pSshLocal[64];
    char pCrossThreadData[64];
};

typedef Test<KernelArgPipeFixture> KernelArgPipeTest;

TEST_F(KernelArgPipeTest, GivenValidPipeWhenSettingKernelArgThenPipeAddressIsCorrect) {
    Pipe *pipe = new MockPipe(pContext);

    auto val = (cl_mem)pipe;
    auto pVal = &val;

    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pKernelArg = (cl_mem **)(this->pKernel->getCrossThreadData() +
                                  this->pKernelInfo->argAsPtr(0).stateless);
    EXPECT_EQ(pipe->getCpuAddress(), *pKernelArg);

    delete pipe;
}

TEST_F(KernelArgPipeTest, GivenSvmPtrStatelessWhenSettingKernelArgThenArgumentsAreSetCorrectly) {
    Pipe *pipe = new MockPipe(pContext);

    auto val = (cl_mem)pipe;
    auto pVal = &val;

    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete pipe;
}

HWTEST_F(KernelArgPipeTest, GivenSvmPtrStatefulWhenSettingKernelArgThenArgumentsAreSetCorrectly) {
    Pipe *pipe = new MockPipe(pContext);

    auto val = (cl_mem)pipe;
    auto pVal = &val;

    pKernelInfo->argAsPtr(0).bindful = 0;
    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_NE(0u, pKernel->getSurfaceStateHeapSize());

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->argAsPtr(0).bindful));

    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());
    EXPECT_EQ(pipe->getCpuAddress(), surfaceAddress);

    delete pipe;
}

TEST_F(KernelArgPipeTest, GivenInvalidPipeWhenSettingKernelArgThenInvalidMemObjectErrorIsReturned) {
    char *ptr = new char[sizeof(Pipe)];

    auto val = (cl_mem *)ptr;
    auto pVal = &val;
    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);

    delete[] ptr;
}

TEST_F(KernelArgPipeTest, GivenBufferWhenSettingKernelArgThenInvalidArgValueErrorIsReturned) {
    Buffer *buffer = new MockBuffer();

    auto val = (cl_mem)buffer;
    auto pVal = &val;
    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_INVALID_ARG_VALUE, retVal);

    delete buffer;
}

TEST_F(KernelArgPipeTest, GivenPipeFromDifferentContextWhenSettingKernelArgThenInvalidMemObjectErrorIsReturned) {
    MockContext newContext;
    Pipe *pipe = new MockPipe(&newContext);

    auto val = (cl_mem)pipe;
    auto pVal = &val;
    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);

    delete pipe;
}

TEST_F(KernelArgPipeTest, GivenInvalidSizeWhenSettingKernelArgThenInvalidArgSizeErrorIsReturned) {
    Pipe *pipe = new MockPipe(pContext);

    auto val = (cl_mem *)pipe;
    auto pVal = &val;
    auto retVal = this->pKernel->setArg(0, 1, pVal);
    EXPECT_EQ(CL_INVALID_ARG_SIZE, retVal);

    delete pipe;
}

TEST_F(KernelArgPipeTest, GivenPtrToNullWhenSettingKernelArgThenInvalidMemObjectErrorIsReturned) {
    auto val = (cl_mem *)nullptr;
    auto pVal = &val;
    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(KernelArgPipeTest, GivenNullWhenSettingKernelArgThenInvalidMemObjectErrorIsReturned) {
    auto pVal = nullptr;
    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}
