/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/address_patch.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/surface.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "gtest/gtest.h"

using namespace NEO;

class BufferSetArgTest : public ContextFixture,
                         public ClDeviceFixture,
                         public testing::Test {

    using ContextFixture::setUp;

  public:
    BufferSetArgTest() {}

  protected:
    void SetUp() override {
        ClDeviceFixture::setUp();
        cl_device_id device = pClDevice;
        ContextFixture::setUp(1, &device);
        pKernelInfo = std::make_unique<MockKernelInfo>();
        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

        constexpr uint32_t sizeOfPointer = sizeof(void *);
        pKernelInfo->addArgBuffer(0, 0x10, sizeOfPointer);
        pKernelInfo->addArgBuffer(1, 0x20, sizeOfPointer);
        pKernelInfo->addArgBuffer(2, 0x30, sizeOfPointer);

        pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
        pKernelInfo->heapInfo.surfaceStateHeapSize = sizeof(surfaceStateHeap);

        pProgram = new MockProgram(pContext, false, toClDeviceVector(*pClDevice));

        retVal = CL_INVALID_VALUE;
        pMultiDeviceKernel = MultiDeviceKernel::create<MockKernel>(pProgram, MockKernel::toKernelInfoContainer(*pKernelInfo, rootDeviceIndex), retVal);
        pKernel = static_cast<MockKernel *>(pMultiDeviceKernel->getKernel(rootDeviceIndex));
        ASSERT_NE(nullptr, pKernel);
        ASSERT_EQ(CL_SUCCESS, retVal);
        pKernel->setCrossThreadData(pCrossThreadData, sizeof(pCrossThreadData));

        pKernel->setKernelArgHandler(1, &Kernel::setArgBuffer);
        pKernel->setKernelArgHandler(2, &Kernel::setArgBuffer);
        pKernel->setKernelArgHandler(0, &Kernel::setArgBuffer);

        BufferDefaults::context = new MockContext(pClDevice);
        buffer = BufferHelper<>::create(BufferDefaults::context);
    }

    void TearDown() override {
        delete buffer;
        delete BufferDefaults::context;
        delete pMultiDeviceKernel;

        delete pProgram;
        ContextFixture::tearDown();
        ClDeviceFixture::tearDown();
    }

    cl_int retVal = CL_SUCCESS;
    MockProgram *pProgram;
    MultiDeviceKernel *pMultiDeviceKernel = nullptr;
    MockKernel *pKernel = nullptr;
    std::unique_ptr<MockKernelInfo> pKernelInfo;
    SKernelBinaryHeaderCommon kernelHeader;
    char surfaceStateHeap[0x80];
    char pCrossThreadData[64];
    Buffer *buffer = nullptr;
};

TEST_F(BufferSetArgTest, WhenSettingKernelArgBufferThenGpuAddressIsSet) {
    auto pKernelArg = (void **)(pKernel->getCrossThreadData() +
                                pKernelInfo->argAsPtr(0).stateless);

    buffer->setArgStateless(pKernelArg, pKernelInfo->argAsPtr(0).pointerSize, pClDevice->getRootDeviceIndex(), false);

    EXPECT_EQ(reinterpret_cast<void *>(buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddress()), *pKernelArg);
}

TEST_F(BufferSetArgTest, givenInvalidSizeWhenSettingKernelArgBufferThenReturnClInvalidArgSize) {
    cl_mem arg = buffer;
    cl_int err = pKernel->setArgBuffer(0, sizeof(cl_mem) + 1, arg);
    EXPECT_EQ(CL_INVALID_ARG_SIZE, err);
}

HWTEST_F(BufferSetArgTest, givenSetArgBufferWhenNullArgStatefulThenProgramNullSurfaceState) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_FORMAT = typename RENDER_SURFACE_STATE::SURFACE_FORMAT;

    pKernelInfo->argAsPtr(0).bindful = 0;
    cl_int ret = pKernel->setArgBuffer(0, sizeof(cl_mem), nullptr);

    EXPECT_EQ(CL_SUCCESS, ret);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(ptrOffset(pKernel->getSurfaceStateHeap(), pKernelInfo->argAsPtr(0).bindful));
    auto surfaceFormat = surfaceState->getSurfaceType();
    auto surfacetype = surfaceState->getSurfaceFormat();

    EXPECT_EQ(surfaceFormat, RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_NULL);
    EXPECT_EQ(surfacetype, SURFACE_FORMAT::SURFACE_FORMAT_RAW);
}

HWTEST_F(BufferSetArgTest, givenSetKernelArgOnReadOnlyBufferThatIsMisalingedWhenSurfaceStateIsSetThenCachingIsOn) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    pKernelInfo->setAddressQualifier(0, KernelArgMetadata::AddrConstant);
    pKernelInfo->argAsPtr(0).bindful = 0;

    auto graphicsAllocation = buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    graphicsAllocation->setSize(graphicsAllocation->getUnderlyingBufferSize() - 1);

    cl_mem clMemBuffer = buffer;

    cl_int ret = pKernel->setArgBuffer(0, sizeof(cl_mem), &clMemBuffer);

    EXPECT_EQ(CL_SUCCESS, ret);

    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(ptrOffset(pKernel->getSurfaceStateHeap(), pKernelInfo->argAsPtr(0).bindful));
    auto mocs = surfaceState->getMemoryObjectControlState();
    auto gmmHelper = pDevice->getGmmHelper();
    auto expectedMocs = gmmHelper->getL3EnabledMOCS();
    auto expectedMocs2 = gmmHelper->getL1EnabledMOCS();
    EXPECT_TRUE(expectedMocs == mocs || expectedMocs2 == mocs);
}

HWTEST_F(BufferSetArgTest, givenSetArgBufferWithNullArgStatelessThenDontProgramNullSurfaceState) {
    char sshOriginal[sizeof(surfaceStateHeap)];
    memcpy(sshOriginal, surfaceStateHeap, sizeof(surfaceStateHeap));

    pKernelInfo->kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::Stateless;

    cl_int ret = pKernel->setArgBuffer(0, sizeof(cl_mem), nullptr);

    EXPECT_EQ(CL_SUCCESS, ret);

    EXPECT_EQ(memcmp(sshOriginal, surfaceStateHeap, sizeof(surfaceStateHeap)), 0);
}

HWTEST_F(BufferSetArgTest, givenNonPureStatefulArgWhenCompressedBufferIsSetThenSetNonAuxMode) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    pKernelInfo->argAsPtr(0).bindful = 0;

    auto graphicsAllocation = buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    graphicsAllocation->setDefaultGmm(new Gmm(pDevice->getGmmHelper(), graphicsAllocation->getUnderlyingBuffer(), buffer->getSize(), 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    graphicsAllocation->getDefaultGmm()->setCompressionEnabled(true);
    cl_mem clMem = buffer;

    cl_int ret = pKernel->setArgBuffer(0, sizeof(cl_mem), &clMem);
    EXPECT_EQ(CL_SUCCESS, ret);

    auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(pKernel->getSurfaceStateHeap(), pKernelInfo->argAsPtr(0).bindful));
    EXPECT_TRUE(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE == surfaceState->getAuxiliarySurfaceMode());

    pKernelInfo->setBufferStateful(0);
    ret = pKernel->setArgBuffer(0, sizeof(cl_mem), &clMem);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_TRUE(EncodeSurfaceState<FamilyType>::isAuxModeEnabled(surfaceState, graphicsAllocation->getDefaultGmm()));
}

TEST_F(BufferSetArgTest, Given32BitAddressingWhenSettingArgStatelessThenGpuAddressIsSetCorrectly) {
    auto pKernelArg = (void **)(pKernel->getCrossThreadData() +
                                pKernelInfo->argAsPtr(0).stateless);

    auto gpuBase = buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddress() >> 2;
    buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->setGpuBaseAddress(gpuBase);
    buffer->setArgStateless(pKernelArg, pKernelInfo->argAsPtr(0).pointerSize, pClDevice->getRootDeviceIndex(), true);

    EXPECT_EQ(reinterpret_cast<void *>(buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddress() - gpuBase), *pKernelArg);
}

TEST_F(BufferSetArgTest, givenBufferWhenOffsetSubbufferIsPassedToSetKernelArgThenCorrectGpuVAIsPatched) {
    cl_buffer_region region;
    region.origin = 0xc0;
    region.size = 32;
    cl_int error = 0;
    auto subBuffer = buffer->createSubBuffer(buffer->getFlags(), buffer->getFlagsIntel(), &region, error);

    ASSERT_NE(nullptr, subBuffer);

    EXPECT_EQ(ptrOffset(buffer->getCpuAddress(), region.origin), subBuffer->getCpuAddress());

    auto pKernelArg = (void **)(pKernel->getCrossThreadData() +
                                pKernelInfo->argAsPtr(0).stateless);

    subBuffer->setArgStateless(pKernelArg, pKernelInfo->argAsPtr(0).pointerSize, pClDevice->getRootDeviceIndex(), false);

    EXPECT_EQ(reinterpret_cast<void *>(subBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddress() + region.origin), *pKernelArg);
    delete subBuffer;
}

TEST_F(BufferSetArgTest, givenCurbeTokenThatSizeIs4BytesWhenStatelessArgIsPatchedThenOnly4BytesArePatchedInCurbe) {
    auto pKernelArg = (void **)(pKernel->getCrossThreadData() +
                                pKernelInfo->argAsPtr(0).stateless);

    // fill 8 bytes with 0xffffffffffffffff;
    uint64_t fillValue = -1;
    uint64_t *pointer64bytes = (uint64_t *)pKernelArg;
    std::memcpy(pointer64bytes, &fillValue, sizeof(fillValue));

    constexpr uint32_t sizeOf4Bytes = sizeof(uint32_t);
    pKernelInfo->argAsPtr(0).pointerSize = sizeOf4Bytes;

    buffer->setArgStateless(pKernelArg, sizeOf4Bytes, pClDevice->getRootDeviceIndex(), false);

    // make sure only 4 bytes are patched
    auto bufferAddress = buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddress();
    uint32_t address32bits = static_cast<uint32_t>(bufferAddress);
    uint64_t curbeValue;
    std::memcpy(&curbeValue, pointer64bytes, sizeof(curbeValue));
    uint32_t higherPart = curbeValue >> 32;
    uint32_t lowerPart = (curbeValue & 0xffffffff);
    EXPECT_EQ(0xffffffff, higherPart);
    EXPECT_EQ(address32bits, lowerPart);
}

TEST_F(BufferSetArgTest, WhenSettingKernelArgThenAddressToPatchIsSetCorrectlyAndSurfacesSet) {
    cl_mem memObj = buffer;

    retVal = clSetKernelArg(
        pMultiDeviceKernel,
        0,
        sizeof(memObj),
        &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto pKernelArg = (void **)(pKernel->getCrossThreadData() +
                                pKernelInfo->argAsPtr(0).stateless);

    EXPECT_EQ(reinterpret_cast<void *>(buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddressToPatch()), *pKernelArg);

    std::vector<Surface *> surfaces;
    pKernel->getResidency(surfaces);
    EXPECT_EQ(1u, surfaces.size());

    for (auto &surface : surfaces) {
        delete surface;
    }
}

TEST_F(BufferSetArgTest, GivenSvmPointerWhenSettingKernelArgThenAddressToPatchIsSetCorrectlyAndSurfacesSet) {
    void *ptrSVM = pContext->getSVMAllocsManager()->createSVMAlloc(256, {}, pContext->getRootDeviceIndices(), pContext->getDeviceBitfields());
    EXPECT_NE(nullptr, ptrSVM);

    auto svmData = pContext->getSVMAllocsManager()->getSVMAlloc(ptrSVM);
    ASSERT_NE(nullptr, svmData);
    GraphicsAllocation *svmAllocation = svmData->gpuAllocations.getGraphicsAllocation(pDevice->getRootDeviceIndex());
    EXPECT_NE(nullptr, svmAllocation);

    retVal = pKernel->setArgSvmAlloc(
        0,
        ptrSVM,
        svmAllocation,
        0u);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto pKernelArg = (void **)(pKernel->getCrossThreadData() +
                                pKernelInfo->argAsPtr(0).stateless);

    EXPECT_EQ(ptrSVM, *pKernelArg);

    std::vector<Surface *> surfaces;
    pKernel->getResidency(surfaces);
    EXPECT_EQ(1u, surfaces.size());
    for (auto &surface : surfaces) {
        delete surface;
    }

    pContext->getSVMAllocsManager()->freeSVMAlloc(ptrSVM);
}

TEST_F(BufferSetArgTest, WhenGettingKernelArgThenBufferIsReturned) {
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
    debugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);
    cl_mem memObj = buffer;

    retVal = pKernel->setArg(
        0,
        sizeof(memObj),
        &memObj);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(1u, pKernel->getPatchInfoDataList().size());

    EXPECT_EQ(PatchInfoAllocationType::kernelArg, pKernel->getPatchInfoDataList()[0].sourceType);
    EXPECT_EQ(PatchInfoAllocationType::indirectObjectHeap, pKernel->getPatchInfoDataList()[0].targetType);
    EXPECT_EQ(buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddressToPatch(), pKernel->getPatchInfoDataList()[0].sourceAllocation);
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
