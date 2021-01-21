/*
 * Copyright (C) 2017-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/fixtures/memory_management_fixture.h"
#include "opencl/test/unit_test/kernel/kernel_arg_buffer_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/mocks/ult_cl_device_factory.h"
#include "test.h"

#include "CL/cl.h"
#include "gtest/gtest.h"
#include "hw_cmds.h"

#include <memory>

using namespace NEO;

typedef Test<KernelArgBufferFixture> KernelArgBufferTest;

TEST_F(KernelArgBufferTest, GivenValidBufferWhenSettingKernelArgThenBufferAddressIsCorrect) {
    Buffer *buffer = new MockBuffer();

    auto val = (cl_mem)buffer;
    auto pVal = &val;

    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pKernelArg = (cl_mem **)(this->pKernel->getCrossThreadData(rootDeviceIndex) +
                                  this->pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);
    EXPECT_EQ(buffer->getCpuAddress(), *pKernelArg);

    delete buffer;
}

struct MultiDeviceKernelArgBufferTest : public ::testing::Test {

    void SetUp() override {
        ClDeviceVector devicesForContext;
        devicesForContext.push_back(deviceFactory.rootDevices[1]);
        devicesForContext.push_back(deviceFactory.subDevices[4]);
        devicesForContext.push_back(deviceFactory.subDevices[5]);
        pContext = std::make_unique<MockContext>(devicesForContext);
        kernelInfos.resize(3);
        kernelInfos[0] = nullptr;
        pKernelInfosStorage[0] = std::make_unique<KernelInfo>();
        pKernelInfosStorage[1] = std::make_unique<KernelInfo>();
        kernelInfos[1] = pKernelInfosStorage[0].get();
        kernelInfos[2] = pKernelInfosStorage[1].get();

        auto &hwHelper = HwHelper::get(renderCoreFamily);

        // setup kernel arg offsets
        KernelArgPatchInfo kernelArgPatchInfo;

        for (auto i = 0u; i < 2; i++) {
            pKernelInfosStorage[i]->heapInfo.pSsh = pSshLocal[i];
            pKernelInfosStorage[i]->heapInfo.SurfaceStateHeapSize = sizeof(pSshLocal[i]);
            pKernelInfosStorage[i]->usesSsh = true;
            pKernelInfosStorage[i]->requiresSshForBuffers = true;
            pKernelInfosStorage[i]->kernelDescriptor.kernelAttributes.simdSize = hwHelper.getMinimalSIMDSize();

            auto crossThreadDataPointer = &pCrossThreadData[i];
            memcpy_s(ptrOffset(&pCrossThreadData[i], i * sizeof(void *)), sizeof(void *), &crossThreadDataPointer, sizeof(void *));
            pKernelInfosStorage[i]->crossThreadData = pCrossThreadData[i];

            pKernelInfosStorage[i]->kernelArgInfo.resize(1);
            pKernelInfosStorage[i]->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
            pKernelInfosStorage[i]->kernelArgInfo[0].isBuffer = true;

            pKernelInfosStorage[i]->patchInfo.dataParameterStream = &dataParameterStream[i];
            dataParameterStream[i].DataParameterStreamSize = (i + 1) * sizeof(void *);

            pKernelInfosStorage[i]->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset = i * sizeof(void *);
            pKernelInfosStorage[i]->kernelArgInfo[0].kernelArgPatchInfoVector[0].size = sizeof(void *);
        }

        auto retVal = CL_INVALID_PROGRAM;
        pBuffer = std::unique_ptr<Buffer>(Buffer::create(pContext.get(), 0u, MemoryConstants::pageSize, nullptr, retVal));
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, pBuffer);

        pProgram = std::make_unique<MockProgram>(pContext.get(), false, pContext->getDevices());
    }

    void TearDown() override {
        for (auto i = 0u; i < 2; i++) {
            pKernelInfosStorage[i]->crossThreadData = nullptr;
        }
    }

    UltClDeviceFactory deviceFactory{3, 2};
    std::unique_ptr<MockContext> pContext;
    SPatchDataParameterStream dataParameterStream[2]{};
    std::unique_ptr<KernelInfo> pKernelInfosStorage[2];
    char pCrossThreadData[2][64]{};
    char pSshLocal[2][64]{};
    KernelInfoContainer kernelInfos;
    std::unique_ptr<Buffer> pBuffer;
    std::unique_ptr<MockProgram> pProgram;
};
TEST_F(MultiDeviceKernelArgBufferTest, GivenValidBufferWhenSettingKernelArgThenBufferAddressIsCorrect) {

    auto pKernel = std::unique_ptr<MockKernel>(Kernel::create<MockKernel>(pProgram.get(), kernelInfos, nullptr));

    EXPECT_NE(nullptr, pKernel);
    cl_mem val = pBuffer.get();
    auto pVal = &val;

    auto retVal = pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (auto &rootDeviceIndex : pContext->getRootDeviceIndices()) {
        auto pKernelArg = reinterpret_cast<size_t *>(pKernel->getCrossThreadData(rootDeviceIndex) +
                                                     kernelInfos[rootDeviceIndex]->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);
        EXPECT_EQ(pBuffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddressToPatch(), *pKernelArg);
    }
}

TEST_F(MultiDeviceKernelArgBufferTest, WhenMakingKernelArgResidentThenMemoryIsTransferredToProperDevice) {

    auto pKernel = std::unique_ptr<MockKernel>(Kernel::create<MockKernel>(pProgram.get(), kernelInfos, nullptr));

    EXPECT_NE(nullptr, pKernel);
    cl_mem val = pBuffer.get();
    auto pVal = &val;

    auto retVal = pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto csr1 = deviceFactory.rootDevices[1]->getDefaultEngine().commandStreamReceiver;
    auto csr2 = deviceFactory.rootDevices[2]->getDefaultEngine().commandStreamReceiver;

    pKernel->makeResident(*csr1);
    EXPECT_EQ(1u, pBuffer->getMultiGraphicsAllocation().getLastUsedRootDeviceIndex());

    pKernel->makeResident(*csr2);
    EXPECT_EQ(2u, pBuffer->getMultiGraphicsAllocation().getLastUsedRootDeviceIndex());

    pKernel->makeResident(*csr1);
    EXPECT_EQ(1u, pBuffer->getMultiGraphicsAllocation().getLastUsedRootDeviceIndex());
}

TEST_F(KernelArgBufferTest, GivenSvmPtrStatelessWhenSettingKernelArgThenArgumentsAreSetCorrectly) {
    Buffer *buffer = new MockBuffer();

    auto val = (cl_mem)buffer;
    auto pVal = &val;

    pKernelInfo->usesSsh = false;
    pKernelInfo->requiresSshForBuffers = false;

    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(pKernel->requiresCoherency());

    EXPECT_EQ(0u, pKernel->getSurfaceStateHeapSize(rootDeviceIndex));

    delete buffer;
}

HWTEST_F(KernelArgBufferTest, GivenSvmPtrStatefulWhenSettingKernelArgThenArgumentsAreSetCorrectly) {
    Buffer *buffer = new MockBuffer();

    auto val = (cl_mem)buffer;
    auto pVal = &val;

    pKernelInfo->usesSsh = true;
    pKernelInfo->requiresSshForBuffers = true;

    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(pKernel->requiresCoherency());

    EXPECT_NE(0u, pKernel->getSurfaceStateHeapSize(rootDeviceIndex));

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(rootDeviceIndex), pKernelInfo->kernelArgInfo[0].offsetHeap));

    auto surfaceAddress = surfaceState->getSurfaceBaseAddress();
    EXPECT_EQ(buffer->getGraphicsAllocation(mockRootDeviceIndex)->getGpuAddress(), surfaceAddress);

    delete buffer;
}

HWTEST_F(MultiDeviceKernelArgBufferTest, GivenSvmPtrStatefulWhenSettingKernelArgThenArgumentsAreSetCorrectly) {
    cl_mem val = pBuffer.get();
    auto pVal = &val;

    for (auto i = 0; i < 2; i++) {
        pKernelInfosStorage[i]->usesSsh = true;
        pKernelInfosStorage[i]->requiresSshForBuffers = true;
    }

    auto pKernel = std::unique_ptr<MockKernel>(Kernel::create<MockKernel>(pProgram.get(), kernelInfos, nullptr));

    auto retVal = pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(pKernel->requiresCoherency());

    for (auto &rootDeviceIndex : pContext->getRootDeviceIndices()) {
        EXPECT_NE(0u, pKernel->getSurfaceStateHeapSize(rootDeviceIndex));

        typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
        auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
            ptrOffset(pKernel->getSurfaceStateHeap(rootDeviceIndex), kernelInfos[rootDeviceIndex]->kernelArgInfo[0].offsetHeap));

        auto surfaceAddress = surfaceState->getSurfaceBaseAddress();
        EXPECT_EQ(pBuffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress(), surfaceAddress);
    }
}

HWTEST_F(KernelArgBufferTest, GivenBufferFromSvmPtrWhenSettingKernelArgThenArgumentsAreSetCorrectly) {

    Buffer *buffer = new MockBuffer();
    buffer->getGraphicsAllocation(mockRootDeviceIndex)->setCoherent(true);

    auto val = (cl_mem)buffer;
    auto pVal = &val;

    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(pKernel->requiresCoherency());

    delete buffer;
}

TEST_F(KernelArgBufferTest, GivenInvalidBufferWhenSettingKernelArgThenInvalidMemObjectErrorIsReturned) {
    char *ptr = new char[sizeof(Buffer)];

    auto val = (cl_mem *)ptr;
    auto pVal = &val;
    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);

    delete[] ptr;
}

TEST_F(KernelArgBufferTest, GivenNullPtrWhenSettingKernelArgThenKernelArgIsNull) {
    auto val = (cl_mem *)nullptr;
    auto pVal = &val;
    this->pKernel->setArg(0, sizeof(cl_mem *), pVal);

    auto pKernelArg = (cl_mem **)(this->pKernel->getCrossThreadData(rootDeviceIndex) +
                                  this->pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);

    EXPECT_EQ(nullptr, *pKernelArg);
}

TEST_F(MultiDeviceKernelArgBufferTest, GivenNullPtrWhenSettingKernelArgThenKernelArgIsNull) {

    auto pKernel = std::unique_ptr<MockKernel>(Kernel::create<MockKernel>(pProgram.get(), kernelInfos, nullptr));

    auto val = nullptr;
    auto pVal = &val;
    pKernel->setArg(0, sizeof(cl_mem *), pVal);
    for (auto &rootDeviceIndex : pContext->getRootDeviceIndices()) {
        auto pKernelArg = reinterpret_cast<void **>(pKernel->getCrossThreadData(rootDeviceIndex) +
                                                    kernelInfos[rootDeviceIndex]->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);
        EXPECT_EQ(nullptr, *pKernelArg);
    }
}

TEST_F(KernelArgBufferTest, given32BitDeviceWhenArgPtrPassedIsNullThenOnly4BytesAreBeingPatched) {
    auto val = (cl_mem *)nullptr;
    auto pVal = &val;

    this->pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].size = 4;

    auto pKernelArg64bit = (uint64_t *)(this->pKernel->getCrossThreadData(rootDeviceIndex) +
                                        this->pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);

    uint32_t *pKernelArg32bit = (uint32_t *)pKernelArg64bit;

    *pKernelArg64bit = 0xffffffffffffffff;

    this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    uint64_t expValue = 0u;

    EXPECT_EQ(0u, *pKernelArg32bit);
    EXPECT_NE(expValue, *pKernelArg64bit);
}

TEST_F(KernelArgBufferTest, given32BitDeviceWhenArgPassedIsNullThenOnly4BytesAreBeingPatched) {
    auto pVal = nullptr;
    this->pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].size = 4;
    auto pKernelArg64bit = (uint64_t *)(this->pKernel->getCrossThreadData(rootDeviceIndex) +
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

TEST_F(KernelArgBufferTest, givenBufferWhenHasDirectStatelessAccessToHostMemoryIsCalledThenReturnFalse) {
    MockBuffer buffer;
    buffer.getGraphicsAllocation(mockRootDeviceIndex)->setAllocationType(GraphicsAllocation::AllocationType::BUFFER);

    auto val = (cl_mem)&buffer;
    auto pVal = &val;

    for (auto pureStatefulBufferAccess : {false, true}) {
        pKernelInfo->kernelArgInfo[0].pureStatefulBufferAccess = pureStatefulBufferAccess;

        auto retVal = pKernel->setArg(0, sizeof(cl_mem *), pVal);
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_FALSE(pKernel->hasDirectStatelessAccessToHostMemory());
    }
}

TEST_F(KernelArgBufferTest, givenBufferInHostMemoryWhenHasDirectStatelessAccessToHostMemoryIsCalledThenReturnCorrectValue) {
    MockBuffer buffer;
    buffer.getGraphicsAllocation(mockRootDeviceIndex)->setAllocationType(GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY);

    auto val = (cl_mem)&buffer;
    auto pVal = &val;

    for (auto pureStatefulBufferAccess : {false, true}) {
        pKernelInfo->kernelArgInfo[0].pureStatefulBufferAccess = pureStatefulBufferAccess;

        auto retVal = pKernel->setArg(0, sizeof(cl_mem *), pVal);
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_EQ(!pureStatefulBufferAccess, pKernel->hasDirectStatelessAccessToHostMemory());
    }
}

TEST_F(KernelArgBufferTest, givenGfxAllocationWhenHasDirectStatelessAccessToHostMemoryIsCalledThenReturnFalse) {
    char data[128];
    void *ptr = &data;
    MockGraphicsAllocation gfxAllocation(ptr, 128);
    gfxAllocation.setAllocationType(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);

    for (auto pureStatefulBufferAccess : {false, true}) {
        pKernelInfo->kernelArgInfo[0].pureStatefulBufferAccess = pureStatefulBufferAccess;

        auto retVal = pKernel->setArgSvmAlloc(0, ptr, &gfxAllocation);
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_FALSE(pKernel->hasDirectStatelessAccessToHostMemory());
    }
}

TEST_F(KernelArgBufferTest, givenGfxAllocationInHostMemoryWhenHasDirectStatelessAccessToHostMemoryIsCalledThenReturnCorrectValue) {
    char data[128];
    void *ptr = &data;
    MockGraphicsAllocation gfxAllocation(ptr, 128);
    gfxAllocation.setAllocationType(GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY);

    for (auto pureStatefulBufferAccess : {false, true}) {
        pKernelInfo->kernelArgInfo[0].pureStatefulBufferAccess = pureStatefulBufferAccess;

        auto retVal = pKernel->setArgSvmAlloc(0, ptr, &gfxAllocation);
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_EQ(!pureStatefulBufferAccess, pKernel->hasDirectStatelessAccessToHostMemory());
    }
}

TEST_F(KernelArgBufferTest, givenInvalidKernelObjWhenHasDirectStatelessAccessToHostMemoryIsCalledThenReturnFalse) {
    KernelInfo kernelInfo;
    MockKernel emptyKernel(pProgram, MockKernel::toKernelInfoContainer(kernelInfo, 0));
    EXPECT_FALSE(emptyKernel.hasDirectStatelessAccessToHostMemory());

    pKernel->kernelArguments.at(0).type = Kernel::NONE_OBJ;
    EXPECT_FALSE(pKernel->hasDirectStatelessAccessToHostMemory());

    pKernel->kernelArguments.at(0).type = Kernel::BUFFER_OBJ;
    EXPECT_FALSE(pKernel->hasDirectStatelessAccessToHostMemory());

    pKernel->kernelArguments.at(0).type = Kernel::SVM_ALLOC_OBJ;
    EXPECT_FALSE(pKernel->hasDirectStatelessAccessToHostMemory());
}

TEST_F(KernelArgBufferTest, givenKernelWithIndirectStatelessAccessWhenHasIndirectStatelessAccessToHostMemoryIsCalledThenReturnTrueForHostMemoryAllocations) {
    KernelInfo kernelInfo;
    EXPECT_FALSE(kernelInfo.hasIndirectStatelessAccess);

    MockKernel kernelWithNoIndirectStatelessAccess(pProgram, MockKernel::toKernelInfoContainer(kernelInfo, 0));
    EXPECT_FALSE(kernelWithNoIndirectStatelessAccess.hasIndirectStatelessAccessToHostMemory());

    kernelInfo.hasIndirectStatelessAccess = true;

    MockKernel kernelWithNoIndirectHostAllocations(pProgram, MockKernel::toKernelInfoContainer(kernelInfo, 0));
    EXPECT_FALSE(kernelWithNoIndirectHostAllocations.hasIndirectStatelessAccessToHostMemory());

    const auto allocationTypes = {GraphicsAllocation::AllocationType::BUFFER,
                                  GraphicsAllocation::AllocationType::BUFFER_COMPRESSED,
                                  GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY};

    MockKernel kernelWithIndirectUnifiedMemoryAllocation(pProgram, MockKernel::toKernelInfoContainer(kernelInfo, 0));
    MockGraphicsAllocation gfxAllocation;
    for (const auto type : allocationTypes) {
        gfxAllocation.setAllocationType(type);
        kernelWithIndirectUnifiedMemoryAllocation.setUnifiedMemoryExecInfo(&gfxAllocation);
        if (type == GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY) {
            EXPECT_TRUE(kernelWithIndirectUnifiedMemoryAllocation.hasIndirectStatelessAccessToHostMemory());
        } else {
            EXPECT_FALSE(kernelWithIndirectUnifiedMemoryAllocation.hasIndirectStatelessAccessToHostMemory());
        }
        kernelWithIndirectUnifiedMemoryAllocation.clearUnifiedMemoryExecInfo();
    }
}

TEST_F(KernelArgBufferTest, givenKernelExecInfoWithIndirectStatelessAccessWhenHasIndirectStatelessAccessToHostMemoryIsCalledThenReturnTrueForHostMemoryAllocations) {
    KernelInfo kernelInfo;
    kernelInfo.hasIndirectStatelessAccess = true;

    MockKernel mockKernel(pProgram, MockKernel::toKernelInfoContainer(kernelInfo, 0));
    EXPECT_FALSE(mockKernel.unifiedMemoryControls.indirectHostAllocationsAllowed);
    EXPECT_FALSE(mockKernel.hasIndirectStatelessAccessToHostMemory());

    auto svmAllocationsManager = mockKernel.getContext().getSVMAllocsManager();
    if (svmAllocationsManager == nullptr) {
        return;
    }

    mockKernel.unifiedMemoryControls.indirectHostAllocationsAllowed = true;
    EXPECT_FALSE(mockKernel.hasIndirectStatelessAccessToHostMemory());

    auto deviceProperties = SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::DEVICE_UNIFIED_MEMORY, mockKernel.getContext().getRootDeviceIndices(), mockKernel.getContext().getDeviceBitfields());
    auto unifiedDeviceMemoryAllocation = svmAllocationsManager->createUnifiedMemoryAllocation(4096u, deviceProperties);
    EXPECT_FALSE(mockKernel.hasIndirectStatelessAccessToHostMemory());

    auto hostProperties = SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::HOST_UNIFIED_MEMORY, mockKernel.getContext().getRootDeviceIndices(), mockKernel.getContext().getDeviceBitfields());
    auto unifiedHostMemoryAllocation = svmAllocationsManager->createUnifiedMemoryAllocation(4096u, hostProperties);
    EXPECT_TRUE(mockKernel.hasIndirectStatelessAccessToHostMemory());

    svmAllocationsManager->freeSVMAlloc(unifiedDeviceMemoryAllocation);
    svmAllocationsManager->freeSVMAlloc(unifiedHostMemoryAllocation);
}

TEST_F(KernelArgBufferTest, whenSettingAuxTranslationRequiredThenIsAuxTranslationRequiredReturnsCorrectValue) {
    for (auto auxTranslationRequired : {false, true}) {
        pKernel->setAuxTranslationRequired(auxTranslationRequired);
        EXPECT_EQ(auxTranslationRequired, pKernel->isAuxTranslationRequired());
    }
}

class KernelArgBufferFixtureBindless : public KernelArgBufferFixture {
  public:
    void SetUp() {
        DebugManager.flags.UseBindlessMode.set(1);
        KernelArgBufferFixture::SetUp();
    }
    void TearDown() override {
        KernelArgBufferFixture::TearDown();
    }
    DebugManagerStateRestore restorer;
};

typedef Test<KernelArgBufferFixtureBindless> KernelArgBufferTestBindless;

HWTEST_F(KernelArgBufferTestBindless, givenUsedBindlessBuffersWhenPatchingSurfaceStateOffsetsThenCorrectOffsetIsPatchedInCrossThreadData) {
    using DataPortBindlessSurfaceExtendedMessageDescriptor = typename FamilyType::DataPortBindlessSurfaceExtendedMessageDescriptor;
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseBindlessMode.set(1);

    pKernelInfo->usesSsh = true;
    pKernelInfo->requiresSshForBuffers = true;

    auto crossThreadDataOffset = pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset;
    pKernelInfo->kernelArgInfo[0].offsetHeap = 64;
    pKernelInfo->kernelArgInfo[0].isBuffer = true;

    auto patchLocation = reinterpret_cast<uint32_t *>(ptrOffset(pKernel->getCrossThreadData(rootDeviceIndex), crossThreadDataOffset));
    *patchLocation = 0xdead;

    uint32_t sshOffset = 0x1000;
    pKernel->patchBindlessSurfaceStateOffsets(*pDevice, sshOffset);
    DataPortBindlessSurfaceExtendedMessageDescriptor extMessageDesc;
    extMessageDesc.setBindlessSurfaceOffset(sshOffset + pKernelInfo->kernelArgInfo[0].offsetHeap);
    auto expectedOffset = extMessageDesc.getBindlessSurfaceOffsetToPatch();
    EXPECT_EQ(expectedOffset, *patchLocation);

    sshOffset = static_cast<uint32_t>(maxNBitValue(20) + 1) - 64;
    pKernel->patchBindlessSurfaceStateOffsets(*pDevice, sshOffset);
    extMessageDesc.setBindlessSurfaceOffset(sshOffset + pKernelInfo->kernelArgInfo[0].offsetHeap);
    expectedOffset = extMessageDesc.getBindlessSurfaceOffsetToPatch();
    EXPECT_EQ(expectedOffset, *patchLocation);
}

TEST_F(KernelArgBufferTest, givenUsedBindlessBuffersAndNonBufferArgWhenPatchingSurfaceStateOffsetsThenCrossThreadDataIsNotPatched) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseBindlessMode.set(1);

    pKernelInfo->usesSsh = true;
    pKernelInfo->requiresSshForBuffers = true;

    auto crossThreadDataOffset = pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset;
    pKernelInfo->kernelArgInfo[0].offsetHeap = 64;
    pKernelInfo->kernelArgInfo[0].isBuffer = false;

    auto patchLocation = reinterpret_cast<uint32_t *>(ptrOffset(pKernel->getCrossThreadData(rootDeviceIndex), crossThreadDataOffset));
    *patchLocation = 0xdead;

    uint32_t sshOffset = 4000;
    pKernel->patchBindlessSurfaceStateOffsets(*pDevice, sshOffset);
    EXPECT_EQ(0xdeadu, *patchLocation);
}

TEST_F(KernelArgBufferTest, givenNotUsedBindlessBuffersAndBufferArgWhenPatchingSurfaceStateOffsetsThenCrossThreadDataIsNotPatched) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseBindlessMode.set(0);

    pKernelInfo->usesSsh = true;
    pKernelInfo->requiresSshForBuffers = true;

    auto crossThreadDataOffset = pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset;
    pKernelInfo->kernelArgInfo[0].offsetHeap = 64;
    pKernelInfo->kernelArgInfo[0].isBuffer = true;

    auto patchLocation = reinterpret_cast<uint32_t *>(ptrOffset(pKernel->getCrossThreadData(rootDeviceIndex), crossThreadDataOffset));
    *patchLocation = 0xdead;

    uint32_t sshOffset = 4000;
    pKernel->patchBindlessSurfaceStateOffsets(*pDevice, sshOffset);
    EXPECT_EQ(0xdeadu, *patchLocation);
}

HWTEST_F(KernelArgBufferTestBindless, givenUsedBindlessBuffersAndBuiltinKernelWhenPatchingSurfaceStateOffsetsThenOffsetIsPatched) {
    using DataPortBindlessSurfaceExtendedMessageDescriptor = typename FamilyType::DataPortBindlessSurfaceExtendedMessageDescriptor;

    pKernelInfo->usesSsh = true;
    pKernelInfo->requiresSshForBuffers = true;

    auto crossThreadDataOffset = pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset;
    pKernelInfo->kernelArgInfo[0].offsetHeap = 64;
    pKernelInfo->kernelArgInfo[0].isBuffer = true;

    auto patchLocation = reinterpret_cast<uint32_t *>(ptrOffset(pKernel->getCrossThreadData(rootDeviceIndex), crossThreadDataOffset));
    *patchLocation = 0xdead;

    pKernel->isBuiltIn = true;

    uint32_t sshOffset = 0x1000;
    pKernel->patchBindlessSurfaceStateOffsets(*pDevice, sshOffset);
    EXPECT_NE(0xdeadu, *patchLocation);
}