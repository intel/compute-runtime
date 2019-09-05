/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/unified_memory_manager.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_kernel.h"

#include "cl_api_tests.h"

using namespace NEO;

class KernelArgSvmFixture : public api_fixture, public DeviceFixture {
  public:
    KernelArgSvmFixture()
        : pCrossThreadData{0} {
    }

  protected:
    void SetUp() override {
        api_fixture::SetUp();
        DeviceFixture::SetUp();

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
        pKernelInfo->kernelArgInfo[0].typeStr = "char *";
        pKernelInfo->kernelArgInfo[0].addressQualifier = CL_KERNEL_ARG_ADDRESS_GLOBAL;

        pMockKernel = new MockKernel(pProgram, *pKernelInfo, *this->pDevice);
        ASSERT_EQ(CL_SUCCESS, pMockKernel->initialize());
        pMockKernel->setCrossThreadData(pCrossThreadData, sizeof(pCrossThreadData));
    }

    void TearDown() override {
        delete pMockKernel;

        DeviceFixture::TearDown();
        api_fixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    MockKernel *pMockKernel = nullptr;
    std::unique_ptr<KernelInfo> pKernelInfo;
    SKernelBinaryHeaderCommon kernelHeader;
    char pSshLocal[64];
    char pCrossThreadData[64];
};

typedef Test<KernelArgSvmFixture> clSetKernelArgSVMPointerTests;

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
        pMockKernel, // cl_kernel kernel
        (cl_uint)-1, // cl_uint arg_index
        nullptr      // const void *arg_value
    );
    EXPECT_EQ(CL_INVALID_ARG_INDEX, retVal);
}

TEST_F(clSetKernelArgSVMPointerTests, GivenLocalAddressAndNullArgValueWhenSettingKernelArgThenInvalidArgValueErrorIsReturned) {
    pKernelInfo->kernelArgInfo[0].addressQualifier = CL_KERNEL_ARG_ADDRESS_LOCAL;

    auto retVal = clSetKernelArgSVMPointer(
        pMockKernel, // cl_kernel kernel
        0,           // cl_uint arg_index
        nullptr      // const void *arg_value
    );
    EXPECT_EQ(CL_INVALID_ARG_VALUE, retVal);
}

TEST_F(clSetKernelArgSVMPointerTests, GivenInvalidArgValueWhenSettingKernelArgThenInvalidArgValueErrorIsReturned) {
    pDevice->deviceInfo.sharedSystemMemCapabilities = 0u;
    void *ptrHost = malloc(256);
    EXPECT_NE(nullptr, ptrHost);

    auto retVal = clSetKernelArgSVMPointer(
        pMockKernel, // cl_kernel kernel
        0,           // cl_uint arg_index
        ptrHost      // const void *arg_value
    );
    EXPECT_EQ(CL_INVALID_ARG_VALUE, retVal);

    free(ptrHost);
}

TEST_F(clSetKernelArgSVMPointerTests, GivenSvmAndNullArgValueWhenSettingKernelArgThenSuccessIsReturned) {
    const DeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        auto retVal = clSetKernelArgSVMPointer(
            pMockKernel, // cl_kernel kernel
            0,           // cl_uint arg_index
            nullptr      // const void *arg_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);
    }
}

TEST_F(clSetKernelArgSVMPointerTests, GivenSvmAndValidArgValueWhenSettingKernelArgThenSuccessIsReturned) {
    const DeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, ptrSvm);

        auto retVal = clSetKernelArgSVMPointer(
            pMockKernel, // cl_kernel kernel
            0,           // cl_uint arg_index
            ptrSvm       // const void *arg_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}

TEST_F(clSetKernelArgSVMPointerTests, GivenSvmAndConstantAddressWhenSettingKernelArgThenSuccessIsReturned) {
    const DeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, ptrSvm);

        pKernelInfo->kernelArgInfo[0].addressQualifier = CL_KERNEL_ARG_ADDRESS_CONSTANT;

        auto retVal = clSetKernelArgSVMPointer(
            pMockKernel, // cl_kernel kernel
            0,           // cl_uint arg_index
            ptrSvm       // const void *arg_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}

TEST_F(clSetKernelArgSVMPointerTests, GivenSvmAndPointerWithOffsetWhenSettingKernelArgThenSuccessIsReturned) {
    const DeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        size_t offset = 256 / 2;
        EXPECT_NE(nullptr, ptrSvm);

        auto retVal = clSetKernelArgSVMPointer(
            pMockKernel,            // cl_kernel kernel
            0,                      // cl_uint arg_index
            (char *)ptrSvm + offset // const void *arg_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}

TEST_F(clSetKernelArgSVMPointerTests, GivenSvmAndPointerWithInvalidOffsetWhenSettingKernelArgThenInvalidArgValueErrorIsReturned) {
    pDevice->deviceInfo.sharedSystemMemCapabilities = 0u;
    const DeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        auto svmData = pContext->getSVMAllocsManager()->getSVMAlloc(ptrSvm);
        ASSERT_NE(nullptr, svmData);
        auto svmAlloc = svmData->gpuAllocation;
        EXPECT_NE(nullptr, svmAlloc);

        size_t offset = svmAlloc->getUnderlyingBufferSize() + 1;

        auto retVal = clSetKernelArgSVMPointer(
            pMockKernel,            // cl_kernel kernel
            0,                      // cl_uint arg_index
            (char *)ptrSvm + offset // const void *arg_value
        );
        EXPECT_EQ(CL_INVALID_ARG_VALUE, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}
} // namespace ULT
