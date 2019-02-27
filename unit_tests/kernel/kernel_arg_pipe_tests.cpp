/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/kernel/kernel.h"
#include "runtime/mem_obj/pipe.h"
#include "test.h"
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_pipe.h"
#include "unit_tests/mocks/mock_program.h"

#include "CL/cl.h"
#include "gtest/gtest.h"

#include <memory>

using namespace OCLRT;

class KernelArgPipeFixture : public ContextFixture, public DeviceFixture {

    using ContextFixture::SetUp;

  public:
    KernelArgPipeFixture() {
    }

  protected:
    void SetUp() {
        DeviceFixture::SetUp();
        cl_device_id device = pDevice;
        ContextFixture::SetUp(1, &device);

        // define kernel info
        pKernelInfo = std::make_unique<KernelInfo>();

        // setup kernel arg offsets
        KernelArgPatchInfo kernelArgPatchInfo;

        kernelHeader.SurfaceStateHeapSize = sizeof(pSshLocal);
        pKernelInfo->heapInfo.pSsh = pSshLocal;
        pKernelInfo->heapInfo.pKernelHeader = &kernelHeader;
        pKernelInfo->usesSsh = true;
        pKernelInfo->requiresSshForBuffers = true;

        pKernelInfo->kernelArgInfo.resize(1);
        pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);

        pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset = 0x30;
        pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].size = (uint32_t)sizeof(void *);

        pProgram = new MockProgram(*pDevice->getExecutionEnvironment(), pContext, false);

        pKernel = new MockKernel(pProgram, *pKernelInfo, *pDevice);
        ASSERT_EQ(CL_SUCCESS, pKernel->initialize());
        pKernel->setCrossThreadData(pCrossThreadData, sizeof(pCrossThreadData));

        pKernel->setKernelArgHandler(0, &Kernel::setArgPipe);
    }

    void TearDown() override {
        delete pKernel;

        delete pProgram;
        ContextFixture::TearDown();
        DeviceFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    MockProgram *pProgram = nullptr;
    MockKernel *pKernel = nullptr;
    std::unique_ptr<KernelInfo> pKernelInfo;
    SKernelBinaryHeaderCommon kernelHeader;
    char pSshLocal[64];
    char pCrossThreadData[64];
};

typedef Test<KernelArgPipeFixture> KernelArgPipeTest;

TEST_F(KernelArgPipeTest, SetKernelArgValidPipe) {
    Pipe *pipe = new MockPipe(pContext);

    auto val = (cl_mem)pipe;
    auto pVal = &val;

    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pKernelArg = (cl_mem **)(this->pKernel->getCrossThreadData() +
                                  this->pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);
    EXPECT_EQ(pipe->getCpuAddress(), *pKernelArg);

    delete pipe;
}

TEST_F(KernelArgPipeTest, SetKernelArgValidSvmPtrStateless) {
    Pipe *pipe = new MockPipe(pContext);

    auto val = (cl_mem)pipe;
    auto pVal = &val;

    pKernelInfo->usesSsh = false;
    pKernelInfo->requiresSshForBuffers = false;

    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(0u, pKernel->getSurfaceStateHeapSize());

    delete pipe;
}

HWTEST_F(KernelArgPipeTest, SetKernelArgValidSvmPtrStateful) {
    Pipe *pipe = new MockPipe(pContext);

    auto val = (cl_mem)pipe;
    auto pVal = &val;

    pKernelInfo->usesSsh = true;
    pKernelInfo->requiresSshForBuffers = true;

    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_NE(0u, pKernel->getSurfaceStateHeapSize());

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->kernelArgInfo[0].offsetHeap));

    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());
    EXPECT_EQ(pipe->getCpuAddress(), surfaceAddress);

    delete pipe;
}

TEST_F(KernelArgPipeTest, SetKernelArgFakePipe) {
    char *ptr = new char[sizeof(Pipe)];

    auto val = (cl_mem *)ptr;
    auto pVal = &val;
    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);

    delete[] ptr;
}

TEST_F(KernelArgPipeTest, SetKernelArgBufferAsPipe) {
    Buffer *buffer = new MockBuffer();

    auto val = (cl_mem)buffer;
    auto pVal = &val;
    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_INVALID_ARG_VALUE, retVal);

    delete buffer;
}

TEST_F(KernelArgPipeTest, SetKernelArgPipeFromDifferentContext) {
    Pipe *pipe = new MockPipe(pContext);
    Context *oldContext = &pKernel->getContext();
    MockContext newContext;
    this->pKernel->setContext(&newContext);

    auto val = (cl_mem)pipe;
    auto pVal = &val;
    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);

    pKernel->setContext(oldContext);

    delete pipe;
}

TEST_F(KernelArgPipeTest, SetKernelArgInvalidSize) {
    Pipe *pipe = new MockPipe(pContext);

    auto val = (cl_mem *)pipe;
    auto pVal = &val;
    auto retVal = this->pKernel->setArg(0, 1, pVal);
    EXPECT_EQ(CL_INVALID_ARG_SIZE, retVal);

    delete pipe;
}

TEST_F(KernelArgPipeTest, SetKernelArgPtrToNull) {
    auto val = (cl_mem *)nullptr;
    auto pVal = &val;
    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(KernelArgPipeTest, SetKernelArgNull) {
    auto pVal = nullptr;
    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}
