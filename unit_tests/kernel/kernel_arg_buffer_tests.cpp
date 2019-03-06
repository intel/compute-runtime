/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/kernel/kernel.h"
#include "runtime/mem_obj/buffer.h"
#include "test.h"
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/kernel/kernel_arg_buffer_fixture.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"

#include "CL/cl.h"
#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

typedef Test<KernelArgBufferFixture> KernelArgBufferTest;

TEST_F(KernelArgBufferTest, SetKernelArgValidBuffer) {
    Buffer *buffer = new MockBuffer();

    auto val = (cl_mem)buffer;
    auto pVal = &val;

    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pKernelArg = (cl_mem **)(this->pKernel->getCrossThreadData() +
                                  this->pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);
    EXPECT_EQ(buffer->getCpuAddress(), *pKernelArg);

    delete buffer;
}

TEST_F(KernelArgBufferTest, SetKernelArgValidSvmPtrStateless) {
    Buffer *buffer = new MockBuffer();

    auto val = (cl_mem)buffer;
    auto pVal = &val;

    pKernelInfo->usesSsh = false;
    pKernelInfo->requiresSshForBuffers = false;

    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(pKernel->requiresCoherency());

    EXPECT_EQ(0u, pKernel->getSurfaceStateHeapSize());

    delete buffer;
}

HWTEST_F(KernelArgBufferTest, SetKernelArgValidSvmPtrStateful) {
    Buffer *buffer = new MockBuffer();

    auto val = (cl_mem)buffer;
    auto pVal = &val;

    pKernelInfo->usesSsh = true;
    pKernelInfo->requiresSshForBuffers = true;

    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(pKernel->requiresCoherency());

    EXPECT_NE(0u, pKernel->getSurfaceStateHeapSize());

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(), pKernelInfo->kernelArgInfo[0].offsetHeap));

    auto surfaceAddress = surfaceState->getSurfaceBaseAddress();
    EXPECT_EQ(buffer->getGraphicsAllocation()->getGpuAddress(), surfaceAddress);

    delete buffer;
}

HWTEST_F(KernelArgBufferTest, SetKernelArgBufferFromSvmPtr) {

    Buffer *buffer = new MockBuffer();
    buffer->getGraphicsAllocation()->setCoherent(true);

    auto val = (cl_mem)buffer;
    auto pVal = &val;

    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(pKernel->requiresCoherency());

    delete buffer;
}

TEST_F(KernelArgBufferTest, SetKernelArgFakeBuffer) {
    char *ptr = new char[sizeof(Buffer)];

    auto val = (cl_mem *)ptr;
    auto pVal = &val;
    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);

    delete[] ptr;
}

TEST_F(KernelArgBufferTest, SetKernelArgPtrToNull) {
    auto val = (cl_mem *)nullptr;
    auto pVal = &val;
    this->pKernel->setArg(0, sizeof(cl_mem *), pVal);

    auto pKernelArg = (cl_mem **)(this->pKernel->getCrossThreadData() +
                                  this->pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);

    EXPECT_EQ(nullptr, *pKernelArg);
}

TEST_F(KernelArgBufferTest, given32BitDeviceWhenArgPtrPassedIsNullThenOnly4BytesAreBeingPatched) {
    auto val = (cl_mem *)nullptr;
    auto pVal = &val;

    this->pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].size = 4;

    auto pKernelArg64bit = (uint64_t *)(this->pKernel->getCrossThreadData() +
                                        this->pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);

    uint32_t *pKernelArg32bit = (uint32_t *)pKernelArg64bit;

    *pKernelArg64bit = 0xffffffffffffffff;

    this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    uint64_t expValue = 0u;

    EXPECT_EQ(0u, *pKernelArg32bit);
    EXPECT_NE(expValue, *pKernelArg64bit);
}

TEST_F(KernelArgBufferTest, SetKernelArgNull) {
    auto pVal = nullptr;
    this->pKernel->setArg(0, sizeof(cl_mem *), pVal);

    auto pKernelArg = (cl_mem **)(this->pKernel->getCrossThreadData() +
                                  this->pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);

    EXPECT_EQ(nullptr, *pKernelArg);
}

TEST_F(KernelArgBufferTest, given32BitDeviceWhenArgPassedIsNullThenOnly4BytesAreBeingPatched) {
    auto pVal = nullptr;
    this->pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].size = 4;
    auto pKernelArg64bit = (uint64_t *)(this->pKernel->getCrossThreadData() +
                                        this->pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);

    *pKernelArg64bit = 0xffffffffffffffff;

    uint32_t *pKernelArg32bit = (uint32_t *)pKernelArg64bit;

    this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    uint64_t expValue = 0u;

    EXPECT_EQ(0u, *pKernelArg32bit);
    EXPECT_NE(expValue, *pKernelArg64bit);
}

TEST_F(KernelArgBufferTest, givenWritableBufferWhenSettingAsArgThenDoNotExpectAllocationInCacheFlushVector) {
    auto buffer = std::make_unique<MockBuffer>();
    buffer->mockGfxAllocation.setMemObjectsAllocationWithWritableFlags(true);
    buffer->mockGfxAllocation.setFlushL3Required(false);

    auto val = static_cast<cl_mem>(buffer.get());
    auto pVal = &val;

    auto retVal = pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, pKernel->kernelArgRequiresCacheFlush[0]);
}

TEST_F(KernelArgBufferTest, givenCacheFlushBufferWhenSettingAsArgThenExpectAllocationInCacheFlushVector) {
    auto buffer = std::make_unique<MockBuffer>();
    buffer->mockGfxAllocation.setMemObjectsAllocationWithWritableFlags(false);
    buffer->mockGfxAllocation.setFlushL3Required(true);

    auto val = static_cast<cl_mem>(buffer.get());
    auto pVal = &val;

    auto retVal = pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(&buffer->mockGfxAllocation, pKernel->kernelArgRequiresCacheFlush[0]);
}

TEST_F(KernelArgBufferTest, givenNoCacheFlushBufferWhenSettingAsArgThenNotExpectAllocationInCacheFlushVector) {
    auto buffer = std::make_unique<MockBuffer>();
    buffer->mockGfxAllocation.setMemObjectsAllocationWithWritableFlags(false);
    buffer->mockGfxAllocation.setFlushL3Required(false);

    auto val = static_cast<cl_mem>(buffer.get());
    auto pVal = &val;

    auto retVal = pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, pKernel->kernelArgRequiresCacheFlush[0]);
}
