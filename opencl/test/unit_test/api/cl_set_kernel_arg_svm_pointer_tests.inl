/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/unified_memory/usm_memory_support.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "cl_api_tests.h"

using namespace NEO;

class KernelArgSvmApiFixture : public ApiFixture<> {
  protected:
    void setUp() {
        ApiFixture::setUp();

        pKernelInfo = std::make_unique<MockKernelInfo>();
        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

        pKernelInfo->heapInfo.surfaceStateHeapSize = sizeof(pSshLocal);
        pKernelInfo->heapInfo.pSsh = pSshLocal;

        pKernelInfo->addArgBuffer(0, 0x30, sizeof(void *));
        pKernelInfo->setAddressQualifier(0, KernelArgMetadata::AddrGlobal);

        cl_int retVal{CL_SUCCESS};
        pMockMultiDeviceKernel = MultiDeviceKernel::create<MockKernel>(pProgram, MockKernel::toKernelInfoContainer(*pKernelInfo, testedRootDeviceIndex), retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
        pMockKernel = static_cast<MockKernel *>(pMockMultiDeviceKernel->getKernel(testedRootDeviceIndex));
        ASSERT_NE(nullptr, pMockKernel);
        pMockKernel->setCrossThreadData(pCrossThreadData, sizeof(pCrossThreadData));
    }

    void tearDown() {
        if (pMockMultiDeviceKernel) {
            delete pMockMultiDeviceKernel;
        }

        ApiFixture::tearDown();
    }

    cl_int retVal = CL_SUCCESS;
    MockKernel *pMockKernel = nullptr;
    MultiDeviceKernel *pMockMultiDeviceKernel = nullptr;
    std::unique_ptr<MockKernelInfo> pKernelInfo;
    char pSshLocal[64]{};
    char pCrossThreadData[64]{};
};

typedef Test<KernelArgSvmApiFixture> clSetKernelArgSVMPointerTests;

namespace ULT {

TEST_F(clSetKernelArgSVMPointerTests, GivenNullKernelWhenSettingKernelArgThenInvalidKernelErrorIsReturned) {
    auto retVal = clSetKernelArgSVMPointer(
        nullptr, // cl_kernel kernel
        0,       // cl_uint arg_index
        nullptr  // const void *arg_value
    );
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST_F(clSetKernelArgSVMPointerTests, GivenInvalidArgIndexWhenSettingKernelArgThenInvalidArgIndexErrorIsReturned) {
    auto retVal = clSetKernelArgSVMPointer(
        pMockMultiDeviceKernel, // cl_kernel kernel
        (cl_uint)-1,            // cl_uint arg_index
        nullptr                 // const void *arg_value
    );
    EXPECT_EQ(CL_INVALID_ARG_INDEX, retVal);
}

TEST_F(clSetKernelArgSVMPointerTests, GivenLocalAddressAndNullArgValueWhenSettingKernelArgThenInvalidArgValueErrorIsReturned) {
    pKernelInfo->setAddressQualifier(0, KernelArgMetadata::AddrLocal);

    auto retVal = clSetKernelArgSVMPointer(
        pMockMultiDeviceKernel, // cl_kernel kernel
        0,                      // cl_uint arg_index
        nullptr                 // const void *arg_value
    );
    EXPECT_EQ(CL_INVALID_ARG_VALUE, retVal);
}

TEST_F(clSetKernelArgSVMPointerTests, GivenInvalidArgValueWhenSettingKernelArgAndDebugVarSetThenInvalidArgValueErrorIsReturned) {
    pDevice->deviceInfo.sharedSystemMemCapabilities = 0u;
    pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.sharedSystemMemCapabilities = 0;
    void *ptrHost = malloc(256);
    EXPECT_NE(nullptr, ptrHost);

    DebugManagerStateRestore restore;
    debugManager.flags.DetectIncorrectPointersOnSetArgCalls.set(1);

    cl_int retVal = clSetKernelArgSVMPointer(
        pMockMultiDeviceKernel, // cl_kernel kernel
        0,                      // cl_uint arg_index
        ptrHost                 // const void *arg_value
    );
    EXPECT_EQ(CL_INVALID_ARG_VALUE, retVal);

    pDevice->deviceInfo.sharedSystemMemCapabilities = 0xF;
    pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.sharedSystemMemCapabilities = 0xF;
    debugManager.flags.EnableSharedSystemUsmSupport.set(0);

    retVal = clSetKernelArgSVMPointer(
        pMockMultiDeviceKernel, // cl_kernel kernel
        0,                      // cl_uint arg_index
        ptrHost                 // const void *arg_value
    );
    EXPECT_EQ(CL_INVALID_ARG_VALUE, retVal);

    free(ptrHost);
}

TEST_F(clSetKernelArgSVMPointerTests, GivenInvalidArgValueWhenSettingKernelArgAndDebugVarNotSetThenSuccessIsReturned) {
    pDevice->deviceInfo.sharedSystemMemCapabilities = 0u;
    pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.sharedSystemMemCapabilities = 0;
    void *ptrHost = malloc(256);
    EXPECT_NE(nullptr, ptrHost);

    DebugManagerStateRestore restore;
    debugManager.flags.DetectIncorrectPointersOnSetArgCalls.set(-1);

    auto retVal = clSetKernelArgSVMPointer(
        pMockMultiDeviceKernel, // cl_kernel kernel
        0,                      // cl_uint arg_index
        ptrHost                 // const void *arg_value
    );
    EXPECT_EQ(CL_SUCCESS, retVal);

    free(ptrHost);
}

TEST_F(clSetKernelArgSVMPointerTests, GivenInvalidArgValueWhenSettingKernelArgAndDebugVarSetAndSharedSystemCapabilitiesNonZeroThenSuccessIsReturned) {
    pDevice->deviceInfo.sharedSystemMemCapabilities = 0xF;
    pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.sharedSystemMemCapabilities = 0xF;
    void *ptrHost = malloc(256);
    EXPECT_NE(nullptr, ptrHost);

    DebugManagerStateRestore restore;
    debugManager.flags.DetectIncorrectPointersOnSetArgCalls.set(1);

    auto retVal = clSetKernelArgSVMPointer(
        pMockMultiDeviceKernel, // cl_kernel kernel
        0,                      // cl_uint arg_index
        ptrHost                 // const void *arg_value
    );
    EXPECT_EQ(CL_SUCCESS, retVal);

    free(ptrHost);
}

TEST_F(clSetKernelArgSVMPointerTests, GivenSvmAndNullArgValueWhenSettingKernelArgThenSuccessIsReturned) {
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        auto retVal = clSetKernelArgSVMPointer(
            pMockMultiDeviceKernel, // cl_kernel kernel
            0,                      // cl_uint arg_index
            nullptr                 // const void *arg_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);
    }
}

TEST_F(clSetKernelArgSVMPointerTests, GivenSvmAndValidArgValueWhenSettingKernelArgThenSuccessIsReturned) {
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, ptrSvm);

        auto retVal = clSetKernelArgSVMPointer(
            pMockMultiDeviceKernel, // cl_kernel kernel
            0,                      // cl_uint arg_index
            ptrSvm                  // const void *arg_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}

TEST_F(clSetKernelArgSVMPointerTests, GivenSvmAndConstantAddressWhenSettingKernelArgThenSuccessIsReturned) {
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, ptrSvm);

        pKernelInfo->setAddressQualifier(0, KernelArgMetadata::AddrConstant);

        auto retVal = clSetKernelArgSVMPointer(
            pMockMultiDeviceKernel, // cl_kernel kernel
            0,                      // cl_uint arg_index
            ptrSvm                  // const void *arg_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}

TEST_F(clSetKernelArgSVMPointerTests, GivenSvmAndPointerWithOffsetWhenSettingKernelArgThenSuccessIsReturned) {
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        size_t offset = 256 / 2;
        EXPECT_NE(nullptr, ptrSvm);

        auto retVal = clSetKernelArgSVMPointer(
            pMockMultiDeviceKernel, // cl_kernel kernel
            0,                      // cl_uint arg_index
            (char *)ptrSvm + offset // const void *arg_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}

TEST_F(clSetKernelArgSVMPointerTests, GivenSvmAndPointerWithInvalidOffsetWhenSettingKernelArgAndDebugVarSetThenInvalidArgValueErrorIsReturned) {
    pDevice->deviceInfo.sharedSystemMemCapabilities = 0u;
    pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.sharedSystemMemCapabilities = 0;
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
    DebugManagerStateRestore restore;
    debugManager.flags.DetectIncorrectPointersOnSetArgCalls.set(1);

    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        auto svmData = pContext->getSVMAllocsManager()->getSVMAlloc(ptrSvm);
        ASSERT_NE(nullptr, svmData);
        auto svmAlloc = svmData->gpuAllocations.getGraphicsAllocation(pContext->getDevice(0)->getRootDeviceIndex());
        EXPECT_NE(nullptr, svmAlloc);

        size_t offset = svmAlloc->getUnderlyingBufferSize() + 1;

        auto retVal = clSetKernelArgSVMPointer(
            pMockMultiDeviceKernel, // cl_kernel kernel
            0,                      // cl_uint arg_index
            (char *)ptrSvm + offset // const void *arg_value
        );
        EXPECT_EQ(CL_INVALID_ARG_VALUE, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}

TEST_F(clSetKernelArgSVMPointerTests, GivenSvmAndPointerWithInvalidOffsetWhenSettingKernelArgAndDebugVarNotSetThenSuccessIsReturned) {
    pDevice->deviceInfo.sharedSystemMemCapabilities = 0u;
    pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.sharedSystemMemCapabilities = 0;
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
    DebugManagerStateRestore restore;
    debugManager.flags.DetectIncorrectPointersOnSetArgCalls.set(-1);

    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        auto svmData = pContext->getSVMAllocsManager()->getSVMAlloc(ptrSvm);
        ASSERT_NE(nullptr, svmData);
        auto svmAlloc = svmData->gpuAllocations.getGraphicsAllocation(pContext->getDevice(0)->getRootDeviceIndex());
        EXPECT_NE(nullptr, svmAlloc);

        size_t offset = svmAlloc->getUnderlyingBufferSize() + 1;

        auto retVal = clSetKernelArgSVMPointer(
            pMockMultiDeviceKernel, // cl_kernel kernel
            0,                      // cl_uint arg_index
            (char *)ptrSvm + offset // const void *arg_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}

TEST_F(clSetKernelArgSVMPointerTests, givenSvmAndValidArgValueWhenSettingSameKernelArgThenSetArgSvmAllocCalledOnlyWhenNeeded) {
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        auto mockSvmManager = reinterpret_cast<MockSVMAllocsManager *>(pMockKernel->getContext().getSVMAllocsManager());
        EXPECT_EQ(0u, pMockKernel->setArgSvmAllocCalls);
        void *const ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, ptrSvm);
        auto callCounter = 0u;
        auto svmData = mockSvmManager->getSVMAlloc(ptrSvm);
        //  first set arg - called
        mockSvmManager->allocationsCounter = 0u;
        auto retVal = clSetKernelArgSVMPointer(
            pMockMultiDeviceKernel, // cl_kernel kernel
            0,                      // cl_uint arg_index
            ptrSvm                  // const void *arg_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(++callCounter, pMockKernel->setArgSvmAllocCalls);

        // same values but allocationsCounter == 0 - called
        retVal = clSetKernelArgSVMPointer(
            pMockMultiDeviceKernel, // cl_kernel kernel
            0,                      // cl_uint arg_index
            ptrSvm                  // const void *arg_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(++callCounter, pMockKernel->setArgSvmAllocCalls);
        ++mockSvmManager->allocationsCounter;

        // same values - not called
        retVal = clSetKernelArgSVMPointer(
            pMockMultiDeviceKernel, // cl_kernel kernel
            0,                      // cl_uint arg_index
            ptrSvm                  // const void *arg_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(callCounter, pMockKernel->setArgSvmAllocCalls);

        // same values and allocationsCounter - not called
        retVal = clSetKernelArgSVMPointer(
            pMockMultiDeviceKernel, // cl_kernel kernel
            0,                      // cl_uint arg_index
            ptrSvm                  // const void *arg_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(callCounter, pMockKernel->setArgSvmAllocCalls);
        ++mockSvmManager->allocationsCounter;

        // different pointer - called
        void *const nextPtrSvm = ptrOffset(ptrSvm, 1);
        retVal = clSetKernelArgSVMPointer(
            pMockMultiDeviceKernel, // cl_kernel kernel
            0,                      // cl_uint arg_index
            nextPtrSvm              // const void *arg_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(++callCounter, pMockKernel->setArgSvmAllocCalls);
        ++mockSvmManager->allocationsCounter;

        // different allocId - called
        pMockKernel->kernelArguments[0].allocId = svmData->getAllocId() + 1;
        retVal = clSetKernelArgSVMPointer(
            pMockMultiDeviceKernel, // cl_kernel kernel
            0,                      // cl_uint arg_index
            nextPtrSvm              // const void *arg_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(++callCounter, pMockKernel->setArgSvmAllocCalls);
        ++mockSvmManager->allocationsCounter;

        // same values - not called
        retVal = clSetKernelArgSVMPointer(
            pMockMultiDeviceKernel, // cl_kernel kernel
            0,                      // cl_uint arg_index
            nextPtrSvm              // const void *arg_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(callCounter, pMockKernel->setArgSvmAllocCalls);
        ++mockSvmManager->allocationsCounter;

        // nullptr - called
        EXPECT_FALSE(pMockKernel->getKernelArguments()[0].isSetToNullptr);
        retVal = clSetKernelArgSVMPointer(
            pMockMultiDeviceKernel, // cl_kernel kernel
            0,                      // cl_uint arg_index
            nullptr                 // const void *arg_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(++callCounter, pMockKernel->setArgSvmAllocCalls);
        EXPECT_TRUE(pMockKernel->getKernelArguments()[0].isSetToNullptr);
        ++mockSvmManager->allocationsCounter;

        // nullptr again - not called
        retVal = clSetKernelArgSVMPointer(
            pMockMultiDeviceKernel, // cl_kernel kernel
            0,                      // cl_uint arg_index
            nullptr                 // const void *arg_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(callCounter, pMockKernel->setArgSvmAllocCalls);
        EXPECT_TRUE(pMockKernel->getKernelArguments()[0].isSetToNullptr);
        ++mockSvmManager->allocationsCounter;

        // same value as before nullptr - called
        retVal = clSetKernelArgSVMPointer(
            pMockMultiDeviceKernel, // cl_kernel kernel
            0,                      // cl_uint arg_index
            nextPtrSvm              // const void *arg_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(++callCounter, pMockKernel->setArgSvmAllocCalls);
        ++mockSvmManager->allocationsCounter;

        pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.sharedSystemMemCapabilities = (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess);
        mockSvmManager->freeSVMAlloc(nextPtrSvm);
        // same values but no svmData - called
        retVal = clSetKernelArgSVMPointer(
            pMockMultiDeviceKernel, // cl_kernel kernel
            0,                      // cl_uint arg_index
            nextPtrSvm              // const void *arg_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(++callCounter, pMockKernel->setArgSvmAllocCalls);

        clSVMFree(pContext, ptrSvm);
    }
}
TEST_F(clSetKernelArgSVMPointerTests, givenSvmAndValidArgValueWhenAllocIdCacheHitThenAllocIdMemoryManagerCounterIsUpdated) {
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        auto mockSvmManager = reinterpret_cast<MockSVMAllocsManager *>(pMockKernel->getContext().getSVMAllocsManager());
        EXPECT_EQ(0u, pMockKernel->setArgSvmAllocCalls);
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, ptrSvm);
        auto callCounter = 0u;
        //  first set arg - called
        auto retVal = clSetKernelArgSVMPointer(
            pMockMultiDeviceKernel, // cl_kernel kernel
            0,                      // cl_uint arg_index
            ptrSvm                  // const void *arg_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(++callCounter, pMockKernel->setArgSvmAllocCalls);

        auto expectedAllocationsCounter = 1u;
        expectedAllocationsCounter += pContext->getDevice(0u)->getPlatform()->getHostMemAllocPool().isInitialized() ? 1u : 0u;
        expectedAllocationsCounter += pContext->getDeviceMemAllocPool().isInitialized() ? 1u : 0u;

        EXPECT_EQ(expectedAllocationsCounter, mockSvmManager->allocationsCounter);
        EXPECT_EQ(mockSvmManager->allocationsCounter, pMockKernel->getKernelArguments()[0].allocIdMemoryManagerCounter);

        ++mockSvmManager->allocationsCounter;

        // second set arg - cache hit on same allocId, updates allocIdMemoryManagerCounter
        retVal = clSetKernelArgSVMPointer(
            pMockMultiDeviceKernel, // cl_kernel kernel
            0,                      // cl_uint arg_index
            ptrSvm                  // const void *arg_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(callCounter, pMockKernel->setArgSvmAllocCalls);
        ++expectedAllocationsCounter;
        EXPECT_EQ(expectedAllocationsCounter, mockSvmManager->allocationsCounter);
        EXPECT_EQ(mockSvmManager->allocationsCounter, pMockKernel->getKernelArguments()[0].allocIdMemoryManagerCounter);

        clSVMFree(pContext, ptrSvm);
    }
}
} // namespace ULT
