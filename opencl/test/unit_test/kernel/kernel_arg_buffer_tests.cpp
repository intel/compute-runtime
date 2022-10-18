/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/kernel/kernel_arg_buffer_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/mocks/ult_cl_device_factory.h"

#include "CL/cl.h"
#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

struct KernelArgBufferTest : public Test<KernelArgBufferFixture> {
    struct AllocationTypeHelper {
        AllocationType allocationType;
        bool compressed;
    };
};

TEST_F(KernelArgBufferTest, GivenValidBufferWhenSettingKernelArgThenBufferAddressIsCorrect) {
    Buffer *buffer = new MockBuffer();

    auto val = (cl_mem)buffer;
    auto pVal = &val;

    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pKernelArg = (cl_mem **)(this->pKernel->getCrossThreadData() +
                                  this->pKernelInfo->argAsPtr(0).stateless);
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
        pKernelInfosStorage[0] = std::make_unique<MockKernelInfo>();
        pKernelInfosStorage[1] = std::make_unique<MockKernelInfo>();
        kernelInfos[1] = pKernelInfosStorage[0].get();
        kernelInfos[2] = pKernelInfosStorage[1].get();

        auto &hwHelper = HwHelper::get(renderCoreFamily);

        for (auto i = 0u; i < 2; i++) {
            pKernelInfosStorage[i]->heapInfo.pSsh = pSshLocal[i];
            pKernelInfosStorage[i]->heapInfo.SurfaceStateHeapSize = sizeof(pSshLocal[i]);
            pKernelInfosStorage[i]->kernelDescriptor.kernelAttributes.simdSize = hwHelper.getMinimalSIMDSize();

            auto crossThreadDataPointer = &pCrossThreadData[i];
            memcpy_s(ptrOffset(&pCrossThreadData[i], i * sizeof(void *)), sizeof(void *), &crossThreadDataPointer, sizeof(void *));
            pKernelInfosStorage[i]->crossThreadData = pCrossThreadData[i];

            pKernelInfosStorage[i]->addArgBuffer(0, static_cast<NEO::CrossThreadDataOffset>(i * sizeof(void *)), sizeof(void *));

            pKernelInfosStorage[i]->setCrossThreadDataSize(static_cast<uint16_t>((i + 1) * sizeof(void *)));
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
    std::unique_ptr<MockKernelInfo> pKernelInfosStorage[2];
    char pCrossThreadData[2][64]{};
    char pSshLocal[2][64]{};
    KernelInfoContainer kernelInfos;
    std::unique_ptr<Buffer> pBuffer;
    std::unique_ptr<MockProgram> pProgram;
};
TEST_F(MultiDeviceKernelArgBufferTest, GivenValidBufferWhenSettingKernelArgThenBufferAddressIsCorrect) {
    int32_t retVal = CL_INVALID_VALUE;
    auto pMultiDeviceKernel = std::unique_ptr<MultiDeviceKernel>(MultiDeviceKernel::create<MockKernel>(pProgram.get(), kernelInfos, &retVal));

    EXPECT_EQ(CL_SUCCESS, retVal);
    cl_mem val = pBuffer.get();
    auto pVal = &val;

    retVal = pMultiDeviceKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (auto &rootDeviceIndex : pContext->getRootDeviceIndices()) {
        auto pKernel = static_cast<MockKernel *>(pMultiDeviceKernel->getKernel(rootDeviceIndex));
        auto pKernelArg = reinterpret_cast<size_t *>(pKernel->getCrossThreadData() +
                                                     kernelInfos[rootDeviceIndex]->getArgDescriptorAt(0).as<ArgDescPointer>().stateless);
        EXPECT_EQ(pBuffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddressToPatch(), *pKernelArg);
    }
}

TEST_F(KernelArgBufferTest, GivenSvmPtrStatelessWhenSettingKernelArgThenArgumentsAreSetCorrectly) {
    Buffer *buffer = new MockBuffer();

    auto val = (cl_mem)buffer;
    auto pVal = &val;

    pKernelInfo->kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::Stateless;

    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(pKernel->requiresCoherency());

    delete buffer;
}

HWTEST_F(KernelArgBufferTest, GivenSvmPtrStatefulWhenSettingKernelArgThenArgumentsAreSetCorrectly) {
    Buffer *buffer = new MockBuffer();

    auto val = (cl_mem)buffer;
    auto pVal = &val;

    auto retVal = this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(pKernel->requiresCoherency());

    EXPECT_NE(0u, pKernel->getSurfaceStateHeapSize());

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(), pKernelInfo->argAsPtr(0).bindful));

    auto surfaceAddress = surfaceState->getSurfaceBaseAddress();
    EXPECT_EQ(buffer->getGraphicsAllocation(mockRootDeviceIndex)->getGpuAddress(), surfaceAddress);

    delete buffer;
}

HWTEST_F(MultiDeviceKernelArgBufferTest, GivenSvmPtrStatefulWhenSettingKernelArgThenArgumentsAreSetCorrectly) {
    cl_mem val = pBuffer.get();
    auto pVal = &val;

    int32_t retVal = CL_INVALID_VALUE;
    for (auto &kernelInfo : pKernelInfosStorage) {
        kernelInfo->argAsPtr(0).bindful = 0;
    }
    auto pMultiDeviceKernel = std::unique_ptr<MultiDeviceKernel>(MultiDeviceKernel::create<MockKernel>(pProgram.get(), kernelInfos, &retVal));

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pMultiDeviceKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (auto &rootDeviceIndex : pContext->getRootDeviceIndices()) {
        auto pKernel = pMultiDeviceKernel->getKernel(rootDeviceIndex);
        EXPECT_FALSE(pKernel->requiresCoherency());
        EXPECT_NE(0u, pKernel->getSurfaceStateHeapSize());
        typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
        auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
            ptrOffset(pKernel->getSurfaceStateHeap(), kernelInfos[rootDeviceIndex]->getArgDescriptorAt(0).as<ArgDescPointer>().bindful));

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

    auto pKernelArg = (cl_mem **)(this->pKernel->getCrossThreadData() +
                                  this->pKernelInfo->argAsPtr(0).stateless);

    EXPECT_EQ(nullptr, *pKernelArg);
}

TEST_F(MultiDeviceKernelArgBufferTest, GivenNullPtrWhenSettingKernelArgThenKernelArgIsNull) {
    int32_t retVal = CL_INVALID_VALUE;
    auto pMultiDeviceKernel = std::unique_ptr<MultiDeviceKernel>(MultiDeviceKernel::create<MockKernel>(pProgram.get(), kernelInfos, &retVal));

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto val = nullptr;
    auto pVal = &val;
    pMultiDeviceKernel->setArg(0, sizeof(cl_mem *), pVal);
    for (auto &rootDeviceIndex : pContext->getRootDeviceIndices()) {
        auto pKernel = static_cast<MockKernel *>(pMultiDeviceKernel->getKernel(rootDeviceIndex));
        auto pKernelArg = reinterpret_cast<void **>(pKernel->getCrossThreadData() +
                                                    kernelInfos[rootDeviceIndex]->getArgDescriptorAt(0).as<ArgDescPointer>().stateless);
        EXPECT_EQ(nullptr, *pKernelArg);
    }
}

TEST_F(KernelArgBufferTest, given32BitDeviceWhenArgPtrPassedIsNullThenOnly4BytesAreBeingPatched) {
    auto val = (cl_mem *)nullptr;
    auto pVal = &val;

    auto &argAsPtr = pKernelInfo->argAsPtr(0);
    argAsPtr.pointerSize = 4;

    auto pKernelArg64bit = (uint64_t *)(this->pKernel->getCrossThreadData() + argAsPtr.stateless);
    auto pKernelArg32bit = (uint32_t *)pKernelArg64bit;

    *pKernelArg64bit = 0xffffffffffffffff;

    this->pKernel->setArg(0, sizeof(cl_mem *), pVal);
    uint64_t expValue = 0u;

    EXPECT_EQ(0u, *pKernelArg32bit);
    EXPECT_NE(expValue, *pKernelArg64bit);
}

TEST_F(KernelArgBufferTest, given32BitDeviceWhenArgPassedIsNullThenOnly4BytesAreBeingPatched) {
    auto pVal = nullptr;

    auto &argAsPtr = pKernelInfo->argAsPtr(0);
    argAsPtr.pointerSize = 4;

    auto pKernelArg64bit = (uint64_t *)(this->pKernel->getCrossThreadData() + argAsPtr.stateless);
    auto pKernelArg32bit = (uint32_t *)pKernelArg64bit;

    *pKernelArg64bit = 0xffffffffffffffff;

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
    buffer.getGraphicsAllocation(mockRootDeviceIndex)->setAllocationType(AllocationType::BUFFER);

    auto val = (cl_mem)&buffer;
    auto pVal = &val;

    for (auto pureStatefulBufferAccess : {false, true}) {
        pKernelInfo->setBufferStateful(0, pureStatefulBufferAccess);

        auto retVal = pKernel->setArg(0, sizeof(cl_mem *), pVal);
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_FALSE(pKernel->hasDirectStatelessAccessToHostMemory());
    }
}

TEST_F(KernelArgBufferTest, givenSharedBufferWhenHasDirectStatelessAccessToSharedBufferIsCalledThenReturnCorrectValue) {
    MockBuffer buffer;
    buffer.getGraphicsAllocation(mockRootDeviceIndex)->setAllocationType(AllocationType::SHARED_BUFFER);

    auto val = (cl_mem)&buffer;
    auto pVal = &val;

    for (auto pureStatefulBufferAccess : {false, true}) {
        pKernelInfo->setBufferStateful(0, pureStatefulBufferAccess);

        auto retVal = pKernel->setArg(0, sizeof(cl_mem *), pVal);
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_EQ(!pureStatefulBufferAccess, pKernel->hasDirectStatelessAccessToSharedBuffer());
    }
}

TEST_F(KernelArgBufferTest, givenBufferInHostMemoryWhenHasDirectStatelessAccessToHostMemoryIsCalledThenReturnCorrectValue) {
    MockBuffer buffer;
    buffer.getGraphicsAllocation(mockRootDeviceIndex)->setAllocationType(AllocationType::BUFFER_HOST_MEMORY);

    auto val = (cl_mem)&buffer;
    auto pVal = &val;

    for (auto pureStatefulBufferAccess : {false, true}) {
        pKernelInfo->setBufferStateful(0, pureStatefulBufferAccess);

        auto retVal = pKernel->setArg(0, sizeof(cl_mem *), pVal);
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_EQ(!pureStatefulBufferAccess, pKernel->hasDirectStatelessAccessToHostMemory());
    }
}

TEST_F(KernelArgBufferTest, givenGfxAllocationWhenHasDirectStatelessAccessToHostMemoryIsCalledThenReturnFalse) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    char data[128];
    void *ptr = &data;
    MockGraphicsAllocation gfxAllocation(ptr, 128);
    gfxAllocation.setAllocationType(AllocationType::BUFFER);

    for (auto pureStatefulBufferAccess : {false, true}) {
        pKernelInfo->setBufferStateful(0, pureStatefulBufferAccess);

        auto retVal = pKernel->setArgSvmAlloc(0, ptr, &gfxAllocation, 0u);
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_FALSE(pKernel->hasDirectStatelessAccessToHostMemory());
    }
}

TEST_F(KernelArgBufferTest, givenGfxAllocationInHostMemoryWhenHasDirectStatelessAccessToHostMemoryIsCalledThenReturnCorrectValue) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    char data[128];
    void *ptr = &data;
    MockGraphicsAllocation gfxAllocation(ptr, 128);
    gfxAllocation.setAllocationType(AllocationType::BUFFER_HOST_MEMORY);

    for (auto pureStatefulBufferAccess : {false, true}) {
        pKernelInfo->setBufferStateful(0, pureStatefulBufferAccess);

        auto retVal = pKernel->setArgSvmAlloc(0, ptr, &gfxAllocation, 0u);
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_EQ(!pureStatefulBufferAccess, pKernel->hasDirectStatelessAccessToHostMemory());
    }
}

TEST_F(KernelArgBufferTest, givenInvalidKernelObjWhenHasDirectStatelessAccessToHostMemoryIsCalledThenReturnFalse) {
    KernelInfo kernelInfo;
    MockKernel emptyKernel(pProgram, kernelInfo, *pClDevice);
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
    auto &kernelDescriptor = kernelInfo.kernelDescriptor;
    EXPECT_FALSE(kernelDescriptor.kernelAttributes.hasIndirectStatelessAccess);

    MockKernel kernelWithNoIndirectStatelessAccess(pProgram, kernelInfo, *pClDevice);
    EXPECT_FALSE(kernelWithNoIndirectStatelessAccess.hasIndirectStatelessAccessToHostMemory());
    kernelDescriptor.kernelAttributes.hasIndirectStatelessAccess = true;

    MockKernel kernelWithNoIndirectHostAllocations(pProgram, kernelInfo, *pClDevice);
    EXPECT_FALSE(kernelWithNoIndirectHostAllocations.hasIndirectStatelessAccessToHostMemory());

    const auto allocationTypes = {AllocationType::BUFFER,
                                  AllocationType::BUFFER_HOST_MEMORY};

    MockKernel kernelWithIndirectUnifiedMemoryAllocation(pProgram, kernelInfo, *pClDevice);
    MockGraphicsAllocation gfxAllocation;
    for (const auto type : allocationTypes) {
        gfxAllocation.setAllocationType(type);
        kernelWithIndirectUnifiedMemoryAllocation.setUnifiedMemoryExecInfo(&gfxAllocation);
        if (type == AllocationType::BUFFER_HOST_MEMORY) {
            EXPECT_TRUE(kernelWithIndirectUnifiedMemoryAllocation.hasIndirectStatelessAccessToHostMemory());
        } else {
            EXPECT_FALSE(kernelWithIndirectUnifiedMemoryAllocation.hasIndirectStatelessAccessToHostMemory());
        }
        kernelWithIndirectUnifiedMemoryAllocation.clearUnifiedMemoryExecInfo();
    }
}

TEST_F(KernelArgBufferTest, givenKernelExecInfoWithIndirectStatelessAccessWhenHasIndirectStatelessAccessToHostMemoryIsCalledThenReturnTrueForHostMemoryAllocations) {
    KernelInfo kernelInfo;
    auto &kernelDescriptor = kernelInfo.kernelDescriptor;
    kernelDescriptor.kernelAttributes.hasIndirectStatelessAccess = true;

    MockKernel mockKernel(pProgram, kernelInfo, *pClDevice);
    EXPECT_FALSE(mockKernel.unifiedMemoryControls.indirectHostAllocationsAllowed);
    EXPECT_FALSE(mockKernel.hasIndirectStatelessAccessToHostMemory());

    auto svmAllocationsManager = mockKernel.getContext().getSVMAllocsManager();
    if (svmAllocationsManager == nullptr) {
        return;
    }

    mockKernel.unifiedMemoryControls.indirectHostAllocationsAllowed = true;
    EXPECT_FALSE(mockKernel.hasIndirectStatelessAccessToHostMemory());

    auto deviceProperties = SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::DEVICE_UNIFIED_MEMORY, mockKernel.getContext().getRootDeviceIndices(), mockKernel.getContext().getDeviceBitfields());
    deviceProperties.device = &pClDevice->getDevice();
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

TEST_F(KernelArgBufferTest, givenSetArgBufferOnKernelWithDirectStatelessAccessToSharedBufferWhenUpdateAuxTranslationRequiredIsCalledThenIsAuxTranslationRequiredShouldReturnTrue) {
    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.EnableStatelessCompression.set(1);

    MockBuffer buffer;
    buffer.getGraphicsAllocation(mockRootDeviceIndex)->setAllocationType(AllocationType::SHARED_BUFFER);

    auto val = (cl_mem)&buffer;
    auto pVal = &val;

    auto retVal = pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(pKernel->hasDirectStatelessAccessToSharedBuffer());

    EXPECT_FALSE(pKernel->isAuxTranslationRequired());

    pKernel->updateAuxTranslationRequired();

    EXPECT_TRUE(pKernel->isAuxTranslationRequired());
}

TEST_F(KernelArgBufferTest, givenSetArgBufferOnKernelWithDirectStatelessAccessToHostMemoryWhenUpdateAuxTranslationRequiredIsCalledThenIsAuxTranslationRequiredShouldReturnTrue) {
    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.EnableStatelessCompression.set(1);

    MockBuffer buffer;
    buffer.getGraphicsAllocation(mockRootDeviceIndex)->setAllocationType(AllocationType::BUFFER_HOST_MEMORY);

    auto val = (cl_mem)&buffer;
    auto pVal = &val;

    auto retVal = pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(pKernel->hasDirectStatelessAccessToHostMemory());

    EXPECT_FALSE(pKernel->isAuxTranslationRequired());

    pKernel->updateAuxTranslationRequired();

    EXPECT_TRUE(pKernel->isAuxTranslationRequired());
}

TEST_F(KernelArgBufferTest, givenSetArgBufferOnKernelWithNoDirectStatelessAccessToHostMemoryWhenUpdateAuxTranslationRequiredIsCalledThenIsAuxTranslationRequiredShouldReturnFalse) {
    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.EnableStatelessCompression.set(1);

    MockBuffer buffer;

    auto val = (cl_mem)&buffer;
    auto pVal = &val;

    auto retVal = pKernel->setArg(0, sizeof(cl_mem *), pVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(pKernel->hasDirectStatelessAccessToHostMemory());

    EXPECT_FALSE(pKernel->isAuxTranslationRequired());

    pKernel->updateAuxTranslationRequired();

    EXPECT_FALSE(pKernel->isAuxTranslationRequired());
}

TEST_F(KernelArgBufferTest, givenSetArgSvmAllocOnKernelWithDirectStatelessAccessToHostMemoryWhenUpdateAuxTranslationRequiredIsCalledThenIsAuxTranslationRequiredShouldReturnTrue) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.EnableStatelessCompression.set(1);

    char data[128];
    void *ptr = &data;
    MockGraphicsAllocation gfxAllocation(ptr, 128);
    gfxAllocation.setAllocationType(AllocationType::BUFFER_HOST_MEMORY);

    auto retVal = pKernel->setArgSvmAlloc(0, ptr, &gfxAllocation, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(pKernel->hasDirectStatelessAccessToHostMemory());

    EXPECT_FALSE(pKernel->isAuxTranslationRequired());

    pKernel->updateAuxTranslationRequired();

    EXPECT_TRUE(pKernel->isAuxTranslationRequired());
}

TEST_F(KernelArgBufferTest, givenSetArgSvmAllocOnKernelWithNoDirectStatelessAccessToHostMemoryWhenUpdateAuxTranslationRequiredIsCalledThenIsAuxTranslationRequiredShouldReturnFalse) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.EnableStatelessCompression.set(1);

    char data[128];
    void *ptr = &data;
    MockGraphicsAllocation gfxAllocation(ptr, 128);

    auto retVal = pKernel->setArgSvmAlloc(0, ptr, &gfxAllocation, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(pKernel->hasDirectStatelessAccessToHostMemory());

    EXPECT_FALSE(pKernel->isAuxTranslationRequired());

    pKernel->updateAuxTranslationRequired();

    EXPECT_FALSE(pKernel->isAuxTranslationRequired());
}

TEST_F(KernelArgBufferTest, givenSetUnifiedMemoryExecInfoOnKernelWithNoIndirectStatelessAccessWhenUpdateAuxTranslationRequiredIsCalledThenIsAuxTranslationRequiredShouldReturnFalse) {
    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.EnableStatelessCompression.set(1);

    pKernelInfo->kernelDescriptor.kernelAttributes.hasIndirectStatelessAccess = false;

    MockGraphicsAllocation gfxAllocation;
    gfxAllocation.setAllocationType(AllocationType::BUFFER_HOST_MEMORY);

    pKernel->setUnifiedMemoryExecInfo(&gfxAllocation);

    EXPECT_FALSE(pKernel->hasIndirectStatelessAccessToHostMemory());

    EXPECT_FALSE(pKernel->isAuxTranslationRequired());

    pKernel->updateAuxTranslationRequired();

    EXPECT_FALSE(pKernel->isAuxTranslationRequired());
}

TEST_F(KernelArgBufferTest, givenSetUnifiedMemoryExecInfoOnKernelWithIndirectStatelessAccessWhenUpdateAuxTranslationRequiredIsCalledThenIsAuxTranslationRequiredShouldReturnTrueForHostMemoryAllocation) {
    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.EnableStatelessCompression.set(1);

    pKernelInfo->kernelDescriptor.kernelAttributes.hasIndirectStatelessAccess = true;

    const auto allocationTypes = {AllocationType::BUFFER,
                                  AllocationType::BUFFER_HOST_MEMORY};

    MockGraphicsAllocation gfxAllocation;

    for (const auto type : allocationTypes) {
        gfxAllocation.setAllocationType(type);

        pKernel->setUnifiedMemoryExecInfo(&gfxAllocation);

        if (type == AllocationType::BUFFER_HOST_MEMORY) {
            EXPECT_TRUE(pKernel->hasIndirectStatelessAccessToHostMemory());
        } else {
            EXPECT_FALSE(pKernel->hasIndirectStatelessAccessToHostMemory());
        }

        EXPECT_FALSE(pKernel->isAuxTranslationRequired());

        pKernel->updateAuxTranslationRequired();

        if (type == AllocationType::BUFFER_HOST_MEMORY) {
            EXPECT_TRUE(pKernel->isAuxTranslationRequired());
        } else {
            EXPECT_FALSE(pKernel->isAuxTranslationRequired());
        }

        pKernel->clearUnifiedMemoryExecInfo();
        pKernel->setAuxTranslationRequired(false);
    }
}

TEST_F(KernelArgBufferTest, givenSetUnifiedMemoryExecInfoOnKernelWithIndirectStatelessAccessWhenFillWithKernelObjsForAuxTranslationIsCalledThenSetKernelObjectsForAuxTranslation) {
    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.EnableStatelessCompression.set(1);

    pKernelInfo->kernelDescriptor.kernelAttributes.hasIndirectStatelessAccess = true;

    constexpr std::array<AllocationTypeHelper, 4> allocationTypes = {{{AllocationType::BUFFER, false},
                                                                      {AllocationType::BUFFER, true},
                                                                      {AllocationType::BUFFER_HOST_MEMORY, false},
                                                                      {AllocationType::SVM_GPU, true}}};

    auto gmm = std::make_unique<Gmm>(pDevice->getRootDeviceEnvironment().getGmmHelper(), nullptr, 0, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, StorageInfo{}, true);
    MockGraphicsAllocation gfxAllocation;
    gfxAllocation.setDefaultGmm(gmm.get());

    for (const auto type : allocationTypes) {
        gfxAllocation.setAllocationType(type.allocationType);

        pKernel->setUnifiedMemoryExecInfo(&gfxAllocation);
        gmm->isCompressionEnabled = type.compressed;

        auto kernelObjsForAuxTranslation = pKernel->fillWithKernelObjsForAuxTranslation();

        if (type.compressed) {
            EXPECT_EQ(1u, kernelObjsForAuxTranslation->size());
            auto kernelObj = *kernelObjsForAuxTranslation->find({KernelObjForAuxTranslation::Type::GFX_ALLOC, &gfxAllocation});
            EXPECT_NE(nullptr, kernelObj.object);
            EXPECT_EQ(KernelObjForAuxTranslation::Type::GFX_ALLOC, kernelObj.type);
            kernelObjsForAuxTranslation->erase(kernelObj);
        } else {
            EXPECT_EQ(0u, kernelObjsForAuxTranslation->size());
        }

        pKernel->clearUnifiedMemoryExecInfo();
        pKernel->setAuxTranslationRequired(false);
    }
}

TEST_F(KernelArgBufferTest, givenSVMAllocsManagerWithCompressedSVMAllocationsWhenFillWithKernelObjsForAuxTranslationIsCalledThenSetKernelObjectsForAuxTranslation) {
    if (pContext->getSVMAllocsManager() == nullptr) {
        return;
    }

    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.EnableStatelessCompression.set(1);

    constexpr std::array<AllocationTypeHelper, 4> allocationTypes = {{{AllocationType::BUFFER, false},
                                                                      {AllocationType::BUFFER, true},
                                                                      {AllocationType::BUFFER_HOST_MEMORY, false},
                                                                      {AllocationType::SVM_GPU, true}}};

    auto gmm = std::make_unique<Gmm>(pDevice->getRootDeviceEnvironment().getGmmHelper(), nullptr, 0, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, StorageInfo{}, true);

    MockGraphicsAllocation gfxAllocation;
    gfxAllocation.setDefaultGmm(gmm.get());

    SvmAllocationData allocData(0);
    allocData.gpuAllocations.addAllocation(&gfxAllocation);
    allocData.device = &pClDevice->getDevice();

    for (const auto type : allocationTypes) {
        gfxAllocation.setAllocationType(type.allocationType);

        gmm->isCompressionEnabled = type.compressed;

        pContext->getSVMAllocsManager()->insertSVMAlloc(allocData);

        auto kernelObjsForAuxTranslation = pKernel->fillWithKernelObjsForAuxTranslation();

        if (type.compressed) {
            EXPECT_EQ(1u, kernelObjsForAuxTranslation->size());
            auto kernelObj = *kernelObjsForAuxTranslation->find({KernelObjForAuxTranslation::Type::GFX_ALLOC, &gfxAllocation});
            EXPECT_NE(nullptr, kernelObj.object);
            EXPECT_EQ(KernelObjForAuxTranslation::Type::GFX_ALLOC, kernelObj.type);
            kernelObjsForAuxTranslation->erase(kernelObj);
        } else {
            EXPECT_EQ(0u, kernelObjsForAuxTranslation->size());
        }

        pContext->getSVMAllocsManager()->removeSVMAlloc(allocData);
    }
}

class KernelArgBufferFixtureBindless : public KernelArgBufferFixture {
  public:
    void setUp() {
        DebugManager.flags.UseBindlessMode.set(1);
        KernelArgBufferFixture::setUp();

        pBuffer = new MockBuffer();
        ASSERT_NE(nullptr, pBuffer);

        pKernelInfo->argAsPtr(0).bindless = bindlessOffset;
        pKernelInfo->argAsPtr(0).stateless = undefined<CrossThreadDataOffset>;
        pKernelInfo->argAsPtr(0).bindful = undefined<SurfaceStateHeapOffset>;
    }
    void tearDown() {
        delete pBuffer;
        KernelArgBufferFixture::tearDown();
    }
    DebugManagerStateRestore restorer;
    MockBuffer *pBuffer;
    const CrossThreadDataOffset bindlessOffset = 0x10;
};

typedef Test<KernelArgBufferFixtureBindless> KernelArgBufferTestBindless;

HWTEST_F(KernelArgBufferTestBindless, givenUsedBindlessBuffersWhenPatchingSurfaceStateOffsetsThenCorrectOffsetIsPatchedInCrossThreadData) {
    using DataPortBindlessSurfaceExtendedMessageDescriptor = typename FamilyType::DataPortBindlessSurfaceExtendedMessageDescriptor;
    auto patchLocation = reinterpret_cast<uint32_t *>(ptrOffset(pKernel->getCrossThreadData(), bindlessOffset));
    *patchLocation = 0xdead;

    cl_mem memObj = pBuffer;
    retVal = pKernel->setArg(0, sizeof(memObj), &memObj);

    EXPECT_NE(0xdeadu, *patchLocation);
}

TEST_F(KernelArgBufferTest, givenBufferAsHostMemoryWhenSettingKernelArgThenKernelUsesSystemMemory) {
    MockBuffer buffer;
    buffer.getGraphicsAllocation(mockRootDeviceIndex)->setAllocationType(AllocationType::BUFFER_HOST_MEMORY);

    auto memVal = (cl_mem)&buffer;
    auto val = &memVal;

    EXPECT_FALSE(pKernel->isAnyKernelArgumentUsingSystemMemory());

    auto retVal = pKernel->setArg(0, sizeof(cl_mem *), val);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(pKernel->isAnyKernelArgumentUsingSystemMemory());
}

TEST_F(KernelArgBufferTest, givenBufferAsDeviceMemoryWhenSettingKernelArgThenKernelNotUsesSystemMemory) {
    MockBuffer buffer;
    buffer.getGraphicsAllocation(mockRootDeviceIndex)->setAllocationType(AllocationType::BUFFER);

    auto memVal = (cl_mem)&buffer;
    auto val = &memVal;

    EXPECT_FALSE(pKernel->isAnyKernelArgumentUsingSystemMemory());

    auto retVal = pKernel->setArg(0, sizeof(cl_mem *), val);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(pKernel->isAnyKernelArgumentUsingSystemMemory());
}

TEST_F(KernelArgBufferTest, givenBufferAsDeviceMemoryAndKernelIsAlreadySetToUseSystemWhenSettingKernelArgThenKernelUsesSystemMemory) {
    MockBuffer buffer;
    buffer.getGraphicsAllocation(mockRootDeviceIndex)->setAllocationType(AllocationType::BUFFER);

    auto memVal = (cl_mem)&buffer;
    auto val = &memVal;

    EXPECT_FALSE(pKernel->isAnyKernelArgumentUsingSystemMemory());
    pKernel->anyKernelArgumentUsingSystemMemory = true;

    auto retVal = pKernel->setArg(0, sizeof(cl_mem *), val);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(pKernel->isAnyKernelArgumentUsingSystemMemory());
}
