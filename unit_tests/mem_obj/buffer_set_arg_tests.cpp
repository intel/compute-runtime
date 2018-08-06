/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/gmm_helper/gmm.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/kernel/kernel.h"
#include "runtime/memory_manager/surface.h"
#include "runtime/memory_manager/svm_memory_manager.h"
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "gtest/gtest.h"
#include "test.h"

using namespace OCLRT;

class BufferSetArgTest : public ContextFixture,
                         public DeviceFixture,
                         public testing::Test {

    using ContextFixture::SetUp;

  public:
    BufferSetArgTest()

    {
    }

  protected:
    void SetUp() override {
        DeviceFixture::SetUp();
        cl_device_id device = pDevice;
        ContextFixture::SetUp(1, &device);
        pKernelInfo = KernelInfo::create();
        ASSERT_NE(nullptr, pKernelInfo);

        // define kernel info
        // setup kernel arg offsets
        KernelArgPatchInfo kernelArgPatchInfo;

        pKernelInfo->kernelArgInfo.resize(3);
        pKernelInfo->kernelArgInfo[2].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
        pKernelInfo->kernelArgInfo[1].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
        pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);

        uint32_t sizeOfPointer = sizeof(void *);

        pKernelInfo->kernelArgInfo[2].kernelArgPatchInfoVector[0].crossthreadOffset = 0x10;
        pKernelInfo->kernelArgInfo[1].kernelArgPatchInfoVector[0].crossthreadOffset = 0x20;
        pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset = 0x30;

        pKernelInfo->kernelArgInfo[2].kernelArgPatchInfoVector[0].size = sizeOfPointer;
        pKernelInfo->kernelArgInfo[1].kernelArgPatchInfoVector[0].size = sizeOfPointer;
        pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].size = sizeOfPointer;

        kernelHeader.SurfaceStateHeapSize = sizeof(surfaceStateHeap);
        pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
        pKernelInfo->heapInfo.pKernelHeader = &kernelHeader;
        pKernelInfo->usesSsh = true;

        pProgram = new MockProgram(*pDevice->getExecutionEnvironment(), pContext, false);

        pKernel = new MockKernel(pProgram, *pKernelInfo, *pDevice);
        ASSERT_NE(nullptr, pKernel);
        ASSERT_EQ(CL_SUCCESS, pKernel->initialize());
        pKernel->setCrossThreadData(pCrossThreadData, sizeof(pCrossThreadData));

        pKernel->setKernelArgHandler(1, &Kernel::setArgBuffer);
        pKernel->setKernelArgHandler(2, &Kernel::setArgBuffer);
        pKernel->setKernelArgHandler(0, &Kernel::setArgBuffer);

        BufferDefaults::context = new MockContext(pDevice);
        buffer = BufferHelper<>::create(BufferDefaults::context);
    }

    void TearDown() override {
        delete buffer;
        delete BufferDefaults::context;
        delete pKernel;
        delete pKernelInfo;
        delete pProgram;
        ContextFixture::TearDown();
        DeviceFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    MockProgram *pProgram;
    MockKernel *pKernel = nullptr;
    KernelInfo *pKernelInfo = nullptr;
    SKernelBinaryHeaderCommon kernelHeader;
    char surfaceStateHeap[0x80];
    char pCrossThreadData[64];
    Buffer *buffer = nullptr;
};

TEST_F(BufferSetArgTest, setKernelArgBuffer) {
    auto pKernelArg = (void **)(pKernel->getCrossThreadData() +
                                pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);

    auto tokenSize = pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].size;

    buffer->setArgStateless(pKernelArg, tokenSize);

    EXPECT_EQ((void *)((uintptr_t)buffer->getGraphicsAllocation()->getGpuAddress()), *pKernelArg);
}

TEST_F(BufferSetArgTest, givenInvalidSizeWhenSettingKernelArgBufferThenReturnClInvalidArgSize) {
    cl_mem arg = buffer;
    cl_int err = pKernel->setArgBuffer(0, sizeof(cl_mem) + 1, arg);
    EXPECT_EQ(CL_INVALID_ARG_SIZE, err);
}

HWTEST_F(BufferSetArgTest, givenSetArgBufferWhenNullArgStatefulThenProgramNullSurfaceState) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_FORMAT = typename RENDER_SURFACE_STATE::SURFACE_FORMAT;

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->kernelArgInfo[0].offsetHeap));

    pKernelInfo->requiresSshForBuffers = true;

    cl_int ret = pKernel->setArgBuffer(0, sizeof(cl_mem), nullptr);

    EXPECT_EQ(CL_SUCCESS, ret);

    auto surfaceFormat = surfaceState->getSurfaceType();
    auto surfacetype = surfaceState->getSurfaceFormat();

    EXPECT_EQ(surfaceFormat, RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_NULL);
    EXPECT_EQ(surfacetype, SURFACE_FORMAT::SURFACE_FORMAT_RAW);
}

HWTEST_F(BufferSetArgTest, givenSetArgBufferWithNullArgStatelessThenDontProgramNullSurfaceState) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_FORMAT = typename RENDER_SURFACE_STATE::SURFACE_FORMAT;

    char sshOriginal[sizeof(surfaceStateHeap)];
    memcpy(sshOriginal, surfaceStateHeap, sizeof(surfaceStateHeap));

    pKernelInfo->requiresSshForBuffers = false;

    cl_int ret = pKernel->setArgBuffer(0, sizeof(cl_mem), nullptr);

    EXPECT_EQ(CL_SUCCESS, ret);

    EXPECT_EQ(memcmp(sshOriginal, surfaceStateHeap, sizeof(surfaceStateHeap)), 0);
}

HWTEST_F(BufferSetArgTest, givenNonPureStatefulArgWhenRenderCompressedBufferIsSetThenSetNonAuxMode) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(pKernel->getSurfaceStateHeap(), pKernelInfo->kernelArgInfo[0].offsetHeap));
    buffer->getGraphicsAllocation()->setAllocationType(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
    buffer->getGraphicsAllocation()->gmm = new Gmm(buffer->getGraphicsAllocation()->getUnderlyingBuffer(), buffer->getSize(), false);
    buffer->getGraphicsAllocation()->gmm->isRenderCompressed = true;
    pKernelInfo->requiresSshForBuffers = true;
    cl_mem clMem = buffer;

    pKernelInfo->kernelArgInfo.at(0).pureStatefulBufferAccess = false;
    cl_int ret = pKernel->setArgBuffer(0, sizeof(cl_mem), &clMem);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_TRUE(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE == surfaceState->getAuxiliarySurfaceMode());

    pKernelInfo->kernelArgInfo.at(0).pureStatefulBufferAccess = true;
    ret = pKernel->setArgBuffer(0, sizeof(cl_mem), &clMem);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_TRUE(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E == surfaceState->getAuxiliarySurfaceMode());
}

TEST_F(BufferSetArgTest, setKernelArgBufferFor32BitAddressing) {
    auto pKernelArg = (void **)(pKernel->getCrossThreadData() +
                                pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);

    auto tokenSize = pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].size;

    uintptr_t gpuBase = (uintptr_t)buffer->getGraphicsAllocation()->getGpuAddress() >> 2;
    buffer->getGraphicsAllocation()->gpuBaseAddress = gpuBase;
    buffer->setArgStateless(pKernelArg, tokenSize, true);

    EXPECT_EQ((uintptr_t)buffer->getGraphicsAllocation()->getGpuAddress() - gpuBase, (uintptr_t)*pKernelArg);
}

TEST_F(BufferSetArgTest, givenBufferWhenOffsetedSubbufferIsPassedToSetKernelArgThenCorrectGpuVAIsPatched) {
    cl_buffer_region region;
    region.origin = 0xc0;
    region.size = 32;
    cl_int error = 0;
    auto subBuffer = buffer->createSubBuffer(buffer->getFlags(), &region, error);

    ASSERT_NE(nullptr, subBuffer);

    EXPECT_EQ(ptrOffset(buffer->getCpuAddress(), region.origin), subBuffer->getCpuAddress());

    auto pKernelArg = (void **)(pKernel->getCrossThreadData() +
                                pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);

    auto tokenSize = pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].size;

    subBuffer->setArgStateless(pKernelArg, tokenSize);

    EXPECT_EQ((void *)((uintptr_t)subBuffer->getGraphicsAllocation()->getGpuAddress() + region.origin), *pKernelArg);
    delete subBuffer;
}

TEST_F(BufferSetArgTest, givenCurbeTokenThatSizeIs4BytesWhenStatelessArgIsPatchedThenOnly4BytesArePatchedInCurbe) {
    auto pKernelArg = (void **)(pKernel->getCrossThreadData() +
                                pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);

    //fill 8 bytes with 0xffffffffffffffff;
    uint64_t fillValue = -1;
    uint64_t *pointer64bytes = (uint64_t *)pKernelArg;
    *pointer64bytes = fillValue;

    uint32_t sizeOf4Bytes = sizeof(uint32_t);

    pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].size = sizeOf4Bytes;

    buffer->setArgStateless(pKernelArg, sizeOf4Bytes);

    //make sure only 4 bytes are patched
    uintptr_t bufferAddress = (uintptr_t)buffer->getGraphicsAllocation()->getGpuAddress();
    uint32_t address32bits = (uint32_t)bufferAddress;
    uint64_t curbeValue = *pointer64bytes;
    uint32_t higherPart = curbeValue >> 32;
    uint32_t lowerPart = (curbeValue & 0xffffffff);
    EXPECT_EQ(0xffffffff, higherPart);
    EXPECT_EQ(address32bits, lowerPart);
}

TEST_F(BufferSetArgTest, clSetKernelArgBuffer) {
    cl_mem memObj = buffer;

    retVal = clSetKernelArg(
        pKernel,
        0,
        sizeof(memObj),
        &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto pKernelArg = (void **)(pKernel->getCrossThreadData() +
                                pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);

    EXPECT_EQ((void *)buffer->getGraphicsAllocation()->getGpuAddressToPatch(), *pKernelArg);

    std::vector<Surface *> surfaces;
    pKernel->getResidency(surfaces);
    EXPECT_EQ(1u, surfaces.size());

    for (auto &surface : surfaces) {
        delete surface;
    }
}

TEST_F(BufferSetArgTest, clSetKernelArgSVMPointer) {
    void *ptrSVM = pContext->getSVMAllocsManager()->createSVMAlloc(256);
    EXPECT_NE(nullptr, ptrSVM);

    GraphicsAllocation *pSvmAlloc = pContext->getSVMAllocsManager()->getSVMAlloc(ptrSVM);
    EXPECT_NE(nullptr, pSvmAlloc);

    retVal = pKernel->setArgSvmAlloc(
        0,
        ptrSVM,
        pSvmAlloc);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto pKernelArg = (void **)(pKernel->getCrossThreadData() +
                                pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);

    EXPECT_EQ(ptrSVM, *pKernelArg);

    std::vector<Surface *> surfaces;
    pKernel->getResidency(surfaces);
    EXPECT_EQ(1u, surfaces.size());
    for (auto &surface : surfaces) {
        delete surface;
    }

    pContext->getSVMAllocsManager()->freeSVMAlloc(ptrSVM);
}

TEST_F(BufferSetArgTest, getKernelArgShouldReturnBuffer) {
    cl_mem memObj = buffer;

    retVal = pKernel->setArg(
        0,
        sizeof(memObj),
        &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(memObj, pKernel->getKernelArg(0));
}

TEST_F(BufferSetArgTest, givenKernelArgBufferWhenAddPathInfoDataIsSetThenPatchInfoDataIsCollected) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);
    cl_mem memObj = buffer;

    retVal = pKernel->setArg(
        0,
        sizeof(memObj),
        &memObj);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(1u, pKernel->getPatchInfoDataList().size());

    EXPECT_EQ(PatchInfoAllocationType::KernelArg, pKernel->getPatchInfoDataList()[0].sourceType);
    EXPECT_EQ(PatchInfoAllocationType::IndirectObjectHeap, pKernel->getPatchInfoDataList()[0].targetType);
    EXPECT_EQ(buffer->getGraphicsAllocation()->getGpuAddressToPatch(), pKernel->getPatchInfoDataList()[0].sourceAllocation);
    EXPECT_EQ(reinterpret_cast<uint64_t>(pKernel->getCrossThreadData()), pKernel->getPatchInfoDataList()[0].targetAllocation);
    EXPECT_EQ(0u, pKernel->getPatchInfoDataList()[0].sourceAllocationOffset);
}

TEST_F(BufferSetArgTest, givenKernelArgBufferWhenAddPathInfoDataIsNotSetThenPatchInfoDataIsNotCollected) {
    cl_mem memObj = buffer;

    retVal = pKernel->setArg(
        0,
        sizeof(memObj),
        &memObj);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, pKernel->getPatchInfoDataList().size());
}
