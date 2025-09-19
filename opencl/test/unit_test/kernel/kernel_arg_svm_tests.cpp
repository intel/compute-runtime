/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"
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

class KernelArgSvmFixture : public ContextFixture, public ClDeviceFixture {

    using ContextFixture::setUp;

  public:
    KernelArgSvmFixture() {
    }

  protected:
    void setUp() {
        ClDeviceFixture::setUp();
        cl_device_id device = pClDevice;
        ContextFixture::setUp(1, &device);

        // define kernel info
        pKernelInfo = std::make_unique<MockKernelInfo>();
        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

        pKernelInfo->heapInfo.pSsh = pSshLocal;
        pKernelInfo->heapInfo.surfaceStateHeapSize = sizeof(pSshLocal);

        pKernelInfo->addArgBuffer(0, 0x30, sizeof(void *));

        pProgram = new MockProgram(pContext, false, toClDeviceVector(*pClDevice));

        pKernel = new MockKernel(pProgram, *pKernelInfo, *pClDevice);
        ASSERT_EQ(CL_SUCCESS, pKernel->initialize());
        pKernel->setCrossThreadData(pCrossThreadData, sizeof(pCrossThreadData));
    }

    void tearDown() {
        delete pKernel;

        delete pProgram;
        ContextFixture::tearDown();
        ClDeviceFixture::tearDown();
    }

    cl_int retVal = CL_SUCCESS;
    MockProgram *pProgram = nullptr;
    MockKernel *pKernel = nullptr;
    std::unique_ptr<MockKernelInfo> pKernelInfo;
    SKernelBinaryHeaderCommon kernelHeader;
    char pSshLocal[64];
    char pCrossThreadData[64];
};

typedef Test<KernelArgSvmFixture> KernelArgSvmTest;

TEST_F(KernelArgSvmTest, GivenValidSvmPtrWhenSettingKernelArgThenSvmPtrIsCorrect) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    char *svmPtr = new char[256];

    auto retVal = pKernel->setArgSvm(0, 256, svmPtr, nullptr, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pKernelArg = (void **)(pKernel->getCrossThreadData() +
                                pKernelInfo->argAsPtr(0).stateless);
    EXPECT_EQ(svmPtr, *pKernelArg);

    delete[] svmPtr;
}

HWTEST_F(KernelArgSvmTest, GivenSvmPtrStatefulWhenSettingKernelArgThenArgumentsAreSetCorrectly) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    char *svmPtr = new char[256];

    pKernelInfo->argAsPtr(0).bindful = 0;
    auto retVal = pKernel->setArgSvm(0, 256, svmPtr, nullptr, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_NE(0u, pKernel->getSurfaceStateHeapSize());

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->argAsPtr(0).bindful));

    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());
    EXPECT_EQ(svmPtr, surfaceAddress);

    delete[] svmPtr;
}

HWTEST_F(KernelArgSvmTest, GivenSvmPtrBindlessWhenSettingKernelArgThenArgumentsAreSetCorrectly) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    auto svmPtr = std::make_unique<char[]>(256);

    const auto &gfxCoreHelper = pKernel->getGfxCoreHelper();
    const auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();

    const auto bindlessOffset = 0x10;
    pKernelInfo->argAsPtr(0).bindless = bindlessOffset;
    pKernelInfo->kernelDescriptor.initBindlessOffsetToSurfaceState();

    auto retVal = pKernel->setArgSvm(0, 256, svmPtr.get(), nullptr, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_NE(0u, pKernel->getSurfaceStateHeapSize());

    const auto ssIndex = pKernelInfo->kernelDescriptor.bindlessArgsMap.find(bindlessOffset)->second;
    const auto ssOffset = ssIndex * surfaceStateSize;

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  ssOffset));

    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());
    EXPECT_EQ(svmPtr.get(), surfaceAddress);
}

HWTEST_F(KernelArgSvmTest, GivenSvmPtrBindlessAndNotInitializedBindlessOffsetToSurfaceStateWhenSettingKernelArgThenSurfaceStateIsNotEncoded) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    auto svmPtr = std::make_unique<char[]>(256);

    const auto surfaceStateHeap = pKernel->getSurfaceStateHeap();
    const auto surfaceStateHeapSize = pKernel->getSurfaceStateHeapSize();

    const auto bindlessOffset = 0x10;
    pKernelInfo->argAsPtr(0).bindless = bindlessOffset;

    auto ssHeapDataInitial = std::make_unique<char[]>(surfaceStateHeapSize);
    std::memcpy(ssHeapDataInitial.get(), surfaceStateHeap, surfaceStateHeapSize);

    pKernelInfo->kernelDescriptor.bindlessArgsMap.clear();

    auto retVal = pKernel->setArgSvm(0, 256, svmPtr.get(), nullptr, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(0, std::memcmp(ssHeapDataInitial.get(), surfaceStateHeap, surfaceStateHeapSize));
}

TEST_F(KernelArgSvmTest, GivenValidSvmAllocWhenSettingKernelArgThenArgumentsAreSetCorrectly) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    char *svmPtr = new char[256];

    MockGraphicsAllocation svmAlloc(svmPtr, 256);

    auto retVal = pKernel->setArgSvmAlloc(0, svmPtr, &svmAlloc, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pKernelArg = (void **)(pKernel->getCrossThreadData() +
                                pKernelInfo->argAsPtr(0).stateless);
    EXPECT_EQ(svmPtr, *pKernelArg);

    delete[] svmPtr;
}

TEST_F(KernelArgSvmTest, GivenSvmAllocWithUncacheableWhenSettingKernelArgThenKernelHasUncacheableArgs) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    auto svmPtr = std::make_unique<char[]>(256);

    MockGraphicsAllocation svmAlloc(svmPtr.get(), 256);
    svmAlloc.setUncacheable(true);

    auto retVal = pKernel->setArgSvmAlloc(0, svmPtr.get(), &svmAlloc, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(pKernel->hasUncacheableStatelessArgs());
}

TEST_F(KernelArgSvmTest, GivenSvmAllocWithoutUncacheableAndKenelWithUncachebleArgWhenSettingKernelArgThenKernelDoesNotHaveUncacheableArgs) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    auto svmPtr = std::make_unique<char[]>(256);

    MockGraphicsAllocation svmAlloc(svmPtr.get(), 256);
    svmAlloc.setUncacheable(true);

    auto retVal = pKernel->setArgSvmAlloc(0, svmPtr.get(), &svmAlloc, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(pKernel->hasUncacheableStatelessArgs());

    svmAlloc.setUncacheable(false);
    pKernel->kernelArguments[0].isStatelessUncacheable = true;
    retVal = pKernel->setArgSvmAlloc(0, svmPtr.get(), &svmAlloc, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(pKernel->hasUncacheableStatelessArgs());
}

HWTEST_F(KernelArgSvmTest, GivenValidSvmAllocStatefulWhenSettingKernelArgThenArgumentsAreSetCorrectly) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    char *svmPtr = new char[256];

    MockGraphicsAllocation svmAlloc(svmPtr, 256);

    pKernelInfo->argAsPtr(0).bindful = 0;
    auto retVal = pKernel->setArgSvmAlloc(0, svmPtr, &svmAlloc, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_NE(0u, pKernel->getSurfaceStateHeapSize());

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->argAsPtr(0).bindful));

    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());
    EXPECT_EQ(svmPtr, surfaceAddress);

    delete[] svmPtr;
}

HWTEST_F(KernelArgSvmTest, givenOffsetSvmPointerWhenSetArgSvmAllocIsCalledThenProperSvmAddressIsPatched) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    std::unique_ptr<char[]> svmPtr(new char[256]);

    auto offsetPtr = svmPtr.get() + 4;

    MockGraphicsAllocation svmAlloc(svmPtr.get(), 256);

    pKernelInfo->argAsPtr(0).bindful = 0;
    pKernel->setArgSvmAlloc(0, offsetPtr, &svmAlloc, 0u);

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->argAsPtr(0).bindful));

    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());
    EXPECT_EQ(offsetPtr, surfaceAddress);
}

HWTEST_F(KernelArgSvmTest, GivenValidSvmAllocBindlessWhenSettingKernelArgThenArgumentsAreSetCorrectly) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    const auto &gfxCoreHelper = pKernel->getGfxCoreHelper();
    const auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();

    auto svmPtr = std::make_unique<char[]>(256);

    MockGraphicsAllocation svmAlloc(svmPtr.get(), 256);

    const auto bindlessOffset = 0x10;
    pKernelInfo->argAsPtr(0).bindless = bindlessOffset;
    pKernelInfo->kernelDescriptor.initBindlessOffsetToSurfaceState();

    auto retVal = pKernel->setArgSvmAlloc(0, svmPtr.get(), &svmAlloc, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_NE(0u, pKernel->getSurfaceStateHeapSize());

    const auto ssIndex = pKernelInfo->kernelDescriptor.bindlessArgsMap.find(bindlessOffset)->second;
    const auto ssOffset = ssIndex * surfaceStateSize;

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  ssOffset));

    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());
    EXPECT_EQ(svmPtr.get(), surfaceAddress);
}

HWTEST_F(KernelArgSvmTest, givenOffsetSvmPointerBindlessWhenSetArgSvmAllocIsCalledThenProperSvmAddressIsPatched) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    const auto &gfxCoreHelper = pKernel->getGfxCoreHelper();
    const auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();

    std::unique_ptr<char[]> svmPtr(new char[256]);

    auto offsetPtr = svmPtr.get() + 4;

    MockGraphicsAllocation svmAlloc(svmPtr.get(), 256);

    const auto bindlessOffset = 0x10;
    pKernelInfo->argAsPtr(0).bindless = bindlessOffset;
    pKernelInfo->kernelDescriptor.initBindlessOffsetToSurfaceState();

    pKernel->setArgSvmAlloc(0, offsetPtr, &svmAlloc, 0u);

    const auto ssIndex = pKernelInfo->kernelDescriptor.bindlessArgsMap.find(bindlessOffset)->second;
    const auto ssOffset = ssIndex * surfaceStateSize;

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  ssOffset));

    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());
    EXPECT_EQ(offsetPtr, surfaceAddress);
}

HWTEST_F(KernelArgSvmTest, GivenValidSvmAllocBindlessAndNotInitializedBindlessOffsetToSurfaceStateWhenSettingKernelArgThenSurfaceStateIsNotEncoded) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    const auto surfaceStateHeap = pKernel->getSurfaceStateHeap();
    const auto surfaceStateHeapSize = pKernel->getSurfaceStateHeapSize();

    auto svmPtr = std::make_unique<char[]>(256);

    MockGraphicsAllocation svmAlloc(svmPtr.get(), 256);

    const auto bindlessOffset = 0x10;
    pKernelInfo->argAsPtr(0).bindless = bindlessOffset;

    auto ssHeapDataInitial = std::make_unique<char[]>(surfaceStateHeapSize);
    std::memcpy(ssHeapDataInitial.get(), surfaceStateHeap, surfaceStateHeapSize);

    pKernelInfo->kernelDescriptor.bindlessArgsMap.clear();

    auto retVal = pKernel->setArgSvmAlloc(0, svmPtr.get(), &svmAlloc, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(0, std::memcmp(ssHeapDataInitial.get(), surfaceStateHeap, surfaceStateHeapSize));
}

HWTEST_F(KernelArgSvmTest, givenDeviceSupportingSharedSystemAllocationsWhenSetArgSvmIsCalledWithSurfaceStateThenSizeIsMaxAndAddressIsProgrammed) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    this->pClDevice->deviceInfo.sharedSystemMemCapabilities = CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS_INTEL;

    auto systemPointer = reinterpret_cast<void *>(0xfeedbac);

    pKernelInfo->argAsPtr(0).bindful = 0;
    pKernel->setArgSvmAlloc(0, systemPointer, nullptr, 0u);

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->argAsPtr(0).bindful));

    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());
    EXPECT_EQ(systemPointer, surfaceAddress);
    EXPECT_EQ(128u, surfaceState->getWidth());
    EXPECT_EQ(2048u, surfaceState->getDepth());
    EXPECT_EQ(16384u, surfaceState->getHeight());
}

HWTEST_F(KernelArgSvmTest, givenBindlessArgAndDeviceSupportingSharedSystemAllocationsWhenSetArgSvmIsCalledWithSurfaceStateThenSizeIsMaxAndAddressIsProgrammed) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    const auto &gfxCoreHelper = pKernel->getGfxCoreHelper();
    const auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();

    this->pClDevice->deviceInfo.sharedSystemMemCapabilities = CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS_INTEL;

    auto systemPointer = reinterpret_cast<void *>(0xfeedbac);

    const auto bindlessOffset = 0x10;
    pKernelInfo->argAsPtr(0).bindless = bindlessOffset;
    pKernelInfo->kernelDescriptor.initBindlessOffsetToSurfaceState();

    pKernel->setArgSvmAlloc(0, systemPointer, nullptr, 0u);

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    const auto ssIndex = pKernelInfo->kernelDescriptor.bindlessArgsMap.find(bindlessOffset)->second;
    const auto ssOffset = ssIndex * surfaceStateSize;

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  ssOffset));

    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());

    EXPECT_EQ(systemPointer, surfaceAddress);
    EXPECT_EQ(128u, surfaceState->getWidth());
    EXPECT_EQ(2048u, surfaceState->getDepth());
    EXPECT_EQ(16384u, surfaceState->getHeight());
}

TEST_F(KernelArgSvmTest, WhenSettingKernelArgImmediateThenInvalidArgValueErrorIsReturned) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    auto retVal = pKernel->setArgImmediate(0, 256, nullptr);
    EXPECT_EQ(CL_INVALID_ARG_VALUE, retVal);
}

HWTEST_F(KernelArgSvmTest, WhenPatchingWithImplicitSurfaceThenPatchIsApplied) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    constexpr size_t rendSurfSize = sizeof(RENDER_SURFACE_STATE);

    std::vector<char> svmPtr;
    svmPtr.resize(256);

    pKernel->setCrossThreadData(nullptr, sizeof(void *));
    pKernel->setSshLocal(nullptr, rendSurfSize);
    {
        MockGraphicsAllocation svmAlloc(svmPtr.data(), svmPtr.size());

        pKernelInfo->setGlobalVariablesSurface(sizeof(void *), 0, 0);

        constexpr size_t patchOffset = 16;
        void *ptrToPatch = svmPtr.data() + patchOffset;
        ASSERT_GE(pKernel->getCrossThreadDataSize(), sizeof(void *));
        *reinterpret_cast<void **>(pKernel->getCrossThreadData()) = 0U;

        ASSERT_GE(pKernel->getSurfaceStateHeapSize(), rendSurfSize);
        RENDER_SURFACE_STATE *surfState = reinterpret_cast<RENDER_SURFACE_STATE *>(pKernel->getSurfaceStateHeap());
        memset(surfState, 0, rendSurfSize);

        pKernel->patchWithImplicitSurface(castToUint64(ptrToPatch), svmAlloc, pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress);

        // verify cross thread data was properly patched
        EXPECT_EQ(ptrToPatch, *reinterpret_cast<void **>(pKernel->getCrossThreadData()));

        // create surface state for comparison
        RENDER_SURFACE_STATE expectedSurfaceState;
        memset(&expectedSurfaceState, 0, rendSurfSize);
        {
            void *addressToPatch = svmAlloc.getUnderlyingBuffer();
            size_t sizeToPatch = svmAlloc.getUnderlyingBufferSize();
            Buffer::setSurfaceState(pDevice, &expectedSurfaceState, false, false,
                                    sizeToPatch, addressToPatch, 0, &svmAlloc, 0, 0, false);
        }

        // verify ssh was properly patched
        EXPECT_EQ(0, memcmp(&expectedSurfaceState, surfState, rendSurfSize));

        // when cross thread and ssh data is not available then should not do anything
        pKernel->setCrossThreadData(nullptr, 0);
        pKernel->setSshLocal(nullptr, 0);
        pKernel->patchWithImplicitSurface(castToUint64(ptrToPatch), svmAlloc, pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress);
    }
}

TEST_F(KernelArgSvmTest, WhenPatchingBufferOffsetThenPatchIsApplied) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    std::vector<char> svmPtr;
    svmPtr.resize(256);

    pKernel->setCrossThreadData(nullptr, sizeof(uint32_t));
    {
        constexpr uint32_t initVal = 7U;
        constexpr uint32_t svmOffset = 13U;

        MockGraphicsAllocation svmAlloc(svmPtr.data(), 256);
        uint32_t *expectedPatchPtr = reinterpret_cast<uint32_t *>(pKernel->getCrossThreadData());

        ArgDescPointer arg;
        void *returnedPtr = nullptr;

        arg.bufferOffset = undefined<NEO::CrossThreadDataOffset>;
        *expectedPatchPtr = initVal;
        returnedPtr = pKernel->patchBufferOffset(arg, svmPtr.data(), &svmAlloc);
        EXPECT_EQ(svmPtr.data(), returnedPtr);
        EXPECT_EQ(initVal, *expectedPatchPtr);

        arg.bufferOffset = undefined<NEO::CrossThreadDataOffset>;
        *expectedPatchPtr = initVal;
        returnedPtr = pKernel->patchBufferOffset(arg, svmPtr.data(), nullptr);
        EXPECT_EQ(svmPtr.data(), returnedPtr);
        EXPECT_EQ(initVal, *expectedPatchPtr);

        arg.bufferOffset = 0U;
        *expectedPatchPtr = initVal;
        returnedPtr = pKernel->patchBufferOffset(arg, svmPtr.data(), &svmAlloc);
        EXPECT_EQ(svmPtr.data(), returnedPtr);
        EXPECT_EQ(0U, *expectedPatchPtr);

        arg.bufferOffset = 0U;
        *expectedPatchPtr = initVal;
        returnedPtr = pKernel->patchBufferOffset(arg, svmPtr.data() + svmOffset, nullptr);
        void *expectedPtr = alignDown(svmPtr.data() + svmOffset, 4);
        // expecting to see DWORD alignment restriction in offset
        uint32_t expectedOffset = static_cast<uint32_t>(ptrDiff(svmPtr.data() + svmOffset, expectedPtr));
        EXPECT_EQ(expectedPtr, returnedPtr);
        EXPECT_EQ(expectedOffset, *expectedPatchPtr);

        arg.bufferOffset = 0U;
        *expectedPatchPtr = initVal;
        returnedPtr = pKernel->patchBufferOffset(arg, svmPtr.data() + svmOffset, &svmAlloc);
        EXPECT_EQ(svmPtr.data(), returnedPtr);
        EXPECT_EQ(svmOffset, *expectedPatchPtr);
    }
}

template <typename SetArgHandlerT>
class KernelArgSvmTestTyped : public KernelArgSvmTest {
};

struct SetArgHandlerSetArgSvm {
    static void setArg(Kernel &kernel, uint32_t argNum, void *ptrToPatch, size_t allocSize, GraphicsAllocation &alloc) {
        kernel.setArgSvm(argNum, allocSize, ptrToPatch, &alloc, 0u);
    }

    static constexpr bool supportsOffsets() {
        return true;
    }
};

struct SetArgHandlerSetArgSvmAlloc {
    static void setArg(Kernel &kernel, uint32_t argNum, void *ptrToPatch, size_t allocSize, GraphicsAllocation &alloc) {
        kernel.setArgSvmAlloc(argNum, ptrToPatch, &alloc, 0u);
    }

    static constexpr bool supportsOffsets() {
        return true;
    }
};

struct SetArgHandlerSetArgBuffer {
    static void setArg(Kernel &kernel, uint32_t argNum, void *ptrToPatch, size_t allocSize, GraphicsAllocation &alloc) {
        MockBuffer mb{alloc};
        cl_mem memObj = &mb;
        kernel.setArgBuffer(argNum, sizeof(cl_mem), &memObj);
    }

    static constexpr bool supportsOffsets() {
        return false;
    }
};

using SetArgHandlers = ::testing::Types<SetArgHandlerSetArgSvm, SetArgHandlerSetArgSvmAlloc, SetArgHandlerSetArgBuffer>;

TYPED_TEST_SUITE(KernelArgSvmTestTyped, SetArgHandlers);
HWTEST_TYPED_TEST(KernelArgSvmTestTyped, GivenBufferKernelArgWhenBufferOffsetIsNeededThenSetArgSetsIt) {
    const ClDeviceInfo &devInfo = KernelArgSvmFixture::pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    constexpr size_t rendSurfSize = sizeof(RENDER_SURFACE_STATE);

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    uint32_t svmSize = MemoryConstants::pageSize;
    char *svmPtr = reinterpret_cast<char *>(alignedMalloc(svmSize, MemoryConstants::pageSize));

    auto &arg = this->pKernelInfo->argAsPtr(0);
    arg.bindful = 0;
    arg.stateless = 0;
    arg.pointerSize = sizeof(void *);
    arg.bufferOffset = arg.pointerSize;

    this->pKernel->setCrossThreadData(nullptr, arg.bufferOffset + sizeof(uint32_t));
    this->pKernel->setSshLocal(nullptr, rendSurfSize);
    {
        MockGraphicsAllocation svmAlloc(svmPtr, svmSize);

        constexpr size_t patchOffset = 16;
        void *ptrToPatch = svmPtr + patchOffset;
        size_t sizeToPatch = svmSize - patchOffset;
        ASSERT_GE(this->pKernel->getCrossThreadDataSize(), arg.bufferOffset + sizeof(uint32_t));

        void **expectedPointerPatchPtr = reinterpret_cast<void **>(this->pKernel->getCrossThreadData());
        uint32_t *expectedOffsetPatchPtr = reinterpret_cast<uint32_t *>(ptrOffset(this->pKernel->getCrossThreadData(), arg.bufferOffset));
        *expectedPointerPatchPtr = reinterpret_cast<void *>(0U);
        *expectedOffsetPatchPtr = 0U;

        ASSERT_GE(this->pKernel->getSurfaceStateHeapSize(), rendSurfSize);
        RENDER_SURFACE_STATE *surfState = reinterpret_cast<RENDER_SURFACE_STATE *>(this->pKernel->getSurfaceStateHeap());
        memset(surfState, 0, rendSurfSize);

        TypeParam::setArg(*this->pKernel, 0U, ptrToPatch, sizeToPatch, svmAlloc);

        // surface state for comparison
        RENDER_SURFACE_STATE expectedSurfaceState;
        memset(&expectedSurfaceState, 0, rendSurfSize);

        if (TypeParam::supportsOffsets()) {
            // setArgSvm, setArgSvmAlloc
            EXPECT_EQ(ptrToPatch, *expectedPointerPatchPtr);
            EXPECT_EQ(patchOffset, *expectedOffsetPatchPtr);
        } else {
            // setArgBuffer
            EXPECT_EQ(svmAlloc.getUnderlyingBuffer(), *expectedPointerPatchPtr);
            EXPECT_EQ(0U, *expectedOffsetPatchPtr);
        }

        Buffer::setSurfaceState(device.get(), &expectedSurfaceState, false, false, svmAlloc.getUnderlyingBufferSize(),
                                svmAlloc.getUnderlyingBuffer(), 0, &svmAlloc, 0, 0, false);

        // verify ssh was properly patched
        int32_t cmpResult = memcmp(&expectedSurfaceState, surfState, rendSurfSize);
        EXPECT_EQ(0, cmpResult);
    }

    alignedFree(svmPtr);
}

TEST_F(KernelArgSvmTest, givenCpuAddressIsNullWhenGpuAddressIsValidThenExpectSvmArgUseGpuAddress) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    char svmPtr[256];

    pKernelInfo->argAsPtr(0).bufferOffset = 0u;

    MockGraphicsAllocation svmAlloc(nullptr, reinterpret_cast<uint64_t>(svmPtr), 256);

    auto retVal = pKernel->setArgSvmAlloc(0, svmPtr, &svmAlloc, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pKernelArg = (void **)(pKernel->getCrossThreadData() +
                                pKernelInfo->argAsPtr(0).stateless);
    EXPECT_EQ(svmPtr, *pKernelArg);
}

TEST_F(KernelArgSvmTest, givenCpuAddressIsNullWhenGpuAddressIsValidThenPatchBufferOffsetWithGpuAddress) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    std::vector<char> svmPtr;
    svmPtr.resize(256);

    pKernel->setCrossThreadData(nullptr, sizeof(uint32_t));

    constexpr uint32_t initVal = 7U;

    MockGraphicsAllocation svmAlloc(nullptr, reinterpret_cast<uint64_t>(svmPtr.data()), 256);
    uint32_t *expectedPatchPtr = reinterpret_cast<uint32_t *>(pKernel->getCrossThreadData());

    ArgDescPointer arg;
    void *returnedPtr = nullptr;

    arg.bufferOffset = 0U;
    *expectedPatchPtr = initVal;
    returnedPtr = pKernel->patchBufferOffset(arg, svmPtr.data(), &svmAlloc);
    EXPECT_EQ(svmPtr.data(), returnedPtr);
    EXPECT_EQ(0U, *expectedPatchPtr);
}

TEST_F(KernelArgSvmTest, GivenZeroCopySvmPtrWhenSettingKernelArgThenKernelUsesSystemMemory) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    void *alloc = pContext->getSVMAllocsManager()->createSVMAlloc(
        4096,
        MemObjHelper::getSvmAllocationProperties(CL_MEM_READ_ONLY),
        pContext->getRootDeviceIndices(),
        pContext->getDeviceBitfields());

    auto svmData = pContext->getSVMAllocsManager()->getSVMAlloc(alloc);
    auto gpuAllocation = svmData->gpuAllocations.getGraphicsAllocation(*pContext->getRootDeviceIndices().begin());
    gpuAllocation->setAllocationType(NEO::AllocationType::svmZeroCopy);

    EXPECT_FALSE(pKernel->isAnyKernelArgumentUsingSystemMemory());

    auto retVal = pKernel->setArgSvmAlloc(0, alloc, gpuAllocation, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(pKernel->isAnyKernelArgumentUsingSystemMemory());

    pContext->getSVMAllocsManager()->freeSVMAlloc(alloc);
}

TEST_F(KernelArgSvmTest, GivenGpuSvmPtrWhenSettingKernelArgThenKernelNotUsesSystemMemory) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    void *alloc = pContext->getSVMAllocsManager()->createSVMAlloc(
        4096,
        MemObjHelper::getSvmAllocationProperties(CL_MEM_READ_ONLY),
        pContext->getRootDeviceIndices(),
        pContext->getDeviceBitfields());

    auto svmData = pContext->getSVMAllocsManager()->getSVMAlloc(alloc);
    auto gpuAllocation = svmData->gpuAllocations.getGraphicsAllocation(*pContext->getRootDeviceIndices().begin());
    gpuAllocation->setAllocationType(NEO::AllocationType::svmGpu);

    EXPECT_FALSE(pKernel->isAnyKernelArgumentUsingSystemMemory());

    auto retVal = pKernel->setArgSvmAlloc(0, alloc, gpuAllocation, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(pKernel->isAnyKernelArgumentUsingSystemMemory());

    pContext->getSVMAllocsManager()->freeSVMAlloc(alloc);
}

TEST_F(KernelArgSvmTest, GivenGpuSvmPtrAndKernelIsAlreadySetToUseSystemWhenSettingKernelArgThenKernelUsesSystemMemory) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    void *alloc = pContext->getSVMAllocsManager()->createSVMAlloc(
        4096,
        MemObjHelper::getSvmAllocationProperties(CL_MEM_READ_ONLY),
        pContext->getRootDeviceIndices(),
        pContext->getDeviceBitfields());

    auto svmData = pContext->getSVMAllocsManager()->getSVMAlloc(alloc);
    auto gpuAllocation = svmData->gpuAllocations.getGraphicsAllocation(*pContext->getRootDeviceIndices().begin());
    gpuAllocation->setAllocationType(NEO::AllocationType::svmGpu);

    EXPECT_FALSE(pKernel->isAnyKernelArgumentUsingSystemMemory());
    pKernel->anyKernelArgumentUsingSystemMemory = true;

    auto retVal = pKernel->setArgSvmAlloc(0, alloc, gpuAllocation, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(pKernel->isAnyKernelArgumentUsingSystemMemory());

    pContext->getSVMAllocsManager()->freeSVMAlloc(alloc);
}
