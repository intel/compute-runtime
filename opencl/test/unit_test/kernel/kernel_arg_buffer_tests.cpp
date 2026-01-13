/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_resource_usage_ocl_buffer.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/kernel/kernel_arg_buffer_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
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

        auto &gfxCoreHelper = pContext->getDevice(0)->getGfxCoreHelper();

        for (auto i = 0u; i < 2; i++) {
            pKernelInfosStorage[i]->heapInfo.pSsh = pSshLocal[i];
            pKernelInfosStorage[i]->heapInfo.surfaceStateHeapSize = sizeof(pSshLocal[i]);
            pKernelInfosStorage[i]->kernelDescriptor.kernelAttributes.simdSize = gfxCoreHelper.getMinimalSIMDSize();

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
    auto pMultiDeviceKernel = std::unique_ptr<MultiDeviceKernel>(MultiDeviceKernel::create<MockKernel>(pProgram.get(), kernelInfos, retVal));

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
    auto pMultiDeviceKernel = std::unique_ptr<MultiDeviceKernel>(MultiDeviceKernel::create<MockKernel>(pProgram.get(), kernelInfos, retVal));

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

class KernelArgBufferFixtureBindless : public KernelArgBufferFixture {
  public:
    void setUp() {
        debugManager.flags.UseBindlessMode.set(1);
        KernelArgBufferFixture::setUp();

        pBuffer = new MockBuffer();
        ASSERT_NE(nullptr, pBuffer);

        pKernelInfo->argAsPtr(0).bindless = bindlessOffset;
        pKernelInfo->argAsPtr(0).stateless = undefined<CrossThreadDataOffset>;
        pKernelInfo->argAsPtr(0).bindful = undefined<SurfaceStateHeapOffset>;

        pKernelInfo->kernelDescriptor.initBindlessOffsetToSurfaceState();
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

HWTEST_F(KernelArgBufferTestBindless, givenUsedBindlessBuffersWhenSettingKernelArgThenOffsetInCrossThreadDataIsNotPatched) {
    auto patchLocation = reinterpret_cast<uint32_t *>(ptrOffset(pKernel->getCrossThreadData(), bindlessOffset));
    *patchLocation = 0xdead;

    cl_mem memObj = pBuffer;
    retVal = pKernel->setArg(0, sizeof(memObj), &memObj);

    EXPECT_EQ(0xdeadu, *patchLocation);
}

HWTEST_F(KernelArgBufferTestBindless, givenBindlessArgBufferWhenSettingKernelArgThenSurfaceStateIsEncodedAtProperOffset) {
    const auto &gfxCoreHelper = pKernel->getGfxCoreHelper();
    const auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();
    const auto surfaceStateHeapSize = pKernel->getSurfaceStateHeapSize();

    EXPECT_EQ(pKernelInfo->kernelDescriptor.kernelAttributes.numArgsStateful * surfaceStateSize, surfaceStateHeapSize);

    cl_mem memObj = pBuffer;
    retVal = pKernel->setArg(0, sizeof(memObj), &memObj);

    const auto ssIndex = pKernelInfo->kernelDescriptor.bindlessArgsMap.find(bindlessOffset)->second;
    const auto ssOffset = ssIndex * surfaceStateSize;

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    const auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(ptrOffset(pKernel->getSurfaceStateHeap(), ssOffset));
    const auto surfaceAddress = surfaceState->getSurfaceBaseAddress();

    const auto bufferAddress = pBuffer->getGraphicsAllocation(pDevice->getRootDeviceIndex())->getGpuAddress();
    EXPECT_EQ(bufferAddress, surfaceAddress);
}

HWTEST_F(KernelArgBufferTestBindless, givenBindlessArgBufferAndNotInitializedBindlessOffsetToSurfaceStateWhenSettingKernelArgThenSurfaceStateIsNotEncoded) {
    const auto surfaceStateHeap = pKernel->getSurfaceStateHeap();
    const auto surfaceStateHeapSize = pKernel->getSurfaceStateHeapSize();

    auto ssHeapDataInitial = std::make_unique<char[]>(surfaceStateHeapSize);
    std::memcpy(ssHeapDataInitial.get(), surfaceStateHeap, surfaceStateHeapSize);

    pKernelInfo->kernelDescriptor.bindlessArgsMap.clear();

    cl_mem memObj = pBuffer;
    retVal = pKernel->setArg(0, sizeof(memObj), &memObj);

    EXPECT_EQ(0, std::memcmp(ssHeapDataInitial.get(), surfaceStateHeap, surfaceStateHeapSize));
}

HWTEST_F(KernelArgBufferTestBindless, givenBindlessBuffersWhenPatchBindlessOffsetCalledThenBindlessOffsetToSurfaceStateWrittenInCrossThreadData) {

    pClDevice->getExecutionEnvironment()->rootDeviceEnvironments[pClDevice->getRootDeviceIndex()]->createBindlessHeapsHelper(pDevice,
                                                                                                                             pClDevice->getNumGenericSubDevices() > 1);

    auto patchLocation = reinterpret_cast<uint32_t *>(ptrOffset(pKernel->getCrossThreadData(), bindlessOffset));
    *patchLocation = 0xdead;

    pKernel->patchBindlessSurfaceState(pBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex()), bindlessOffset);

    EXPECT_NE(0xdeadu, *patchLocation);
}

TEST_F(KernelArgBufferTest, givenBufferAsHostMemoryWhenSettingKernelArgThenKernelUsesSystemMemory) {
    MockBuffer buffer;
    buffer.getGraphicsAllocation(mockRootDeviceIndex)->setAllocationType(AllocationType::bufferHostMemory);

    auto memVal = (cl_mem)&buffer;
    auto val = &memVal;

    EXPECT_FALSE(pKernel->isAnyKernelArgumentUsingSystemMemory());

    auto retVal = pKernel->setArg(0, sizeof(cl_mem *), val);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(pKernel->isAnyKernelArgumentUsingSystemMemory());
}

TEST_F(KernelArgBufferTest, givenBufferAsDeviceMemoryWhenSettingKernelArgThenKernelNotUsesSystemMemory) {
    MockBuffer buffer;
    buffer.getGraphicsAllocation(mockRootDeviceIndex)->setAllocationType(AllocationType::buffer);

    auto memVal = (cl_mem)&buffer;
    auto val = &memVal;

    EXPECT_FALSE(pKernel->isAnyKernelArgumentUsingSystemMemory());

    auto retVal = pKernel->setArg(0, sizeof(cl_mem *), val);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(pKernel->isAnyKernelArgumentUsingSystemMemory());
}

TEST_F(KernelArgBufferTest, givenBufferAsDeviceMemoryAndKernelIsAlreadySetToUseSystemWhenSettingKernelArgThenKernelUsesSystemMemory) {
    MockBuffer buffer;
    buffer.getGraphicsAllocation(mockRootDeviceIndex)->setAllocationType(AllocationType::buffer);

    auto memVal = (cl_mem)&buffer;
    auto val = &memVal;

    EXPECT_FALSE(pKernel->isAnyKernelArgumentUsingSystemMemory());
    pKernel->anyKernelArgumentUsingSystemMemory = true;

    auto retVal = pKernel->setArg(0, sizeof(cl_mem *), val);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(pKernel->isAnyKernelArgumentUsingSystemMemory());
}
