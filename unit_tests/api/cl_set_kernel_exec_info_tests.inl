/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"
#include "unit_tests/command_stream/thread_arbitration_policy_helper.h"
#include "unit_tests/mocks/mock_kernel.h"

#include "cl_api_tests.h"

using namespace NEO;

class KernelExecInfoFixture : public ApiFixture {
  protected:
    void SetUp() override {
        ApiFixture::SetUp();

        pKernelInfo = std::make_unique<KernelInfo>();

        pMockKernel = new MockKernel(pProgram, *pKernelInfo, *pPlatform->getDevice(0));
        ASSERT_EQ(CL_SUCCESS, pMockKernel->initialize());
        svmCapabilities = pPlatform->getDevice(0)->getDeviceInfo().svmCapabilities;
        if (svmCapabilities != 0) {
            ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
            EXPECT_NE(nullptr, ptrSvm);
        }
    }

    void TearDown() override {
        if (svmCapabilities != 0) {
            clSVMFree(pContext, ptrSvm);
        }

        delete pMockKernel;

        ApiFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    MockKernel *pMockKernel = nullptr;
    std::unique_ptr<KernelInfo> pKernelInfo;
    void *ptrSvm = nullptr;
    cl_device_svm_capabilities svmCapabilities = 0;
};

typedef Test<KernelExecInfoFixture> clSetKernelExecInfoTests;

namespace ULT {

TEST_F(clSetKernelExecInfoTests, GivenNullKernelWhenSettingAdditionalKernelInfoThenInvalidKernelErrorIsReturned) {
    retVal = clSetKernelExecInfo(
        nullptr,                      // cl_kernel kernel
        CL_KERNEL_EXEC_INFO_SVM_PTRS, // cl_kernel_exec_info param_name
        0,                            // size_t param_value_size
        nullptr                       // const void *param_value
    );
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST_F(clSetKernelExecInfoTests, GivenNullParamValueWhenSettingAdditionalKernelInfoThenInvalidValueErrorIsReturned) {
    void **pSvmPtrList = nullptr;
    size_t SvmPtrListSizeInBytes = 1 * sizeof(void *);

    retVal = clSetKernelExecInfo(
        pMockKernel,                  // cl_kernel kernel
        CL_KERNEL_EXEC_INFO_SVM_PTRS, // cl_kernel_exec_info param_name
        SvmPtrListSizeInBytes,        // size_t param_value_size
        pSvmPtrList                   // const void *param_value
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clSetKernelExecInfoTests, GivenNullPointerInParamValueWhenSettingAdditionalKernelInfoThenInvalidValueErrorIsReturned) {
    void *pSvmPtrList[] = {nullptr};
    size_t SvmPtrListSizeInBytes = 1 * sizeof(void *);

    retVal = clSetKernelExecInfo(
        pMockKernel,                  // cl_kernel kernel
        CL_KERNEL_EXEC_INFO_SVM_PTRS, // cl_kernel_exec_info param_name
        SvmPtrListSizeInBytes,        // size_t param_value_size
        pSvmPtrList                   // const void *param_value
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clSetKernelExecInfoTests, GivenParamSizeZeroWhenSettingAdditionalKernelInfoThenInvalidValueErrorIsReturned) {
    void *pSvmPtrList[] = {ptrSvm};
    size_t SvmPtrListSizeInBytes = 0;

    retVal = clSetKernelExecInfo(
        pMockKernel,                  // cl_kernel kernel
        CL_KERNEL_EXEC_INFO_SVM_PTRS, // cl_kernel_exec_info param_name
        SvmPtrListSizeInBytes,        // size_t param_value_size
        pSvmPtrList                   // const void *param_value
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clSetKernelExecInfoTests, GivenInvalidParamSizeWhenSettingAdditionalKernelInfoThenInvalidValueErrorIsReturned) {
    void *pSvmPtrList[] = {ptrSvm};
    size_t SvmPtrListSizeInBytes = (size_t)(-1);

    retVal = clSetKernelExecInfo(
        pMockKernel,                  // cl_kernel kernel
        CL_KERNEL_EXEC_INFO_SVM_PTRS, // cl_kernel_exec_info param_name
        SvmPtrListSizeInBytes,        // size_t param_value_size
        pSvmPtrList                   // const void *param_value
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clSetKernelExecInfoTests, GivenInvalidParamNameWhenSettingAdditionalKernelInfoThenInvalidValueErrorIsReturned) {
    void *pSvmPtrList[] = {ptrSvm};
    size_t SvmPtrListSizeInBytes = 1 * sizeof(void *);

    retVal = clSetKernelExecInfo(
        pMockKernel,           // cl_kernel kernel
        0,                     // cl_kernel_exec_info param_name
        SvmPtrListSizeInBytes, // size_t param_value_size
        pSvmPtrList            // const void *param_value
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clSetKernelExecInfoTests, GivenInvalidOperationWhenSettingAdditionalKernelInfoThenInvalidOperationErrorIsReturned) {
    void *pSvmPtrList[] = {ptrSvm};
    size_t SvmPtrListSizeInBytes = 1 * sizeof(void *);

    retVal = clSetKernelExecInfo(
        pMockKernel,                               // cl_kernel kernel
        CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM, // cl_kernel_exec_info param_name
        SvmPtrListSizeInBytes,                     // size_t param_value_size
        pSvmPtrList                                // const void *param_value
    );
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

TEST_F(clSetKernelExecInfoTests, GivenValidPointerListWithOnePointerWhenSettingAdditionalKernelInfoThenSuccessIsReturned) {
    if (svmCapabilities != 0) {
        void *pSvmPtrList[] = {ptrSvm};
        size_t SvmPtrListSizeInBytes = 1 * sizeof(void *);

        retVal = clSetKernelExecInfo(
            pMockKernel,                  // cl_kernel kernel
            CL_KERNEL_EXEC_INFO_SVM_PTRS, // cl_kernel_exec_info param_name
            SvmPtrListSizeInBytes,        // size_t param_value_size
            pSvmPtrList                   // const void *param_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_EQ(1u, pMockKernel->kernelSvmGfxAllocations.size());
    }
}

TEST_F(clSetKernelExecInfoTests, GivenValidPointerListWithMultiplePointersWhenSettingAdditionalKernelInfoThenSuccessIsReturned) {
    if (svmCapabilities != 0) {
        void *ptrSvm1 = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, ptrSvm1);

        void *ptrSvm2 = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, ptrSvm2);

        void *pSvmPtrList[] = {ptrSvm, ptrSvm1, ptrSvm2};
        size_t SvmPtrListSizeInBytes = 3 * sizeof(void *);

        retVal = clSetKernelExecInfo(
            pMockKernel,                  // cl_kernel kernel
            CL_KERNEL_EXEC_INFO_SVM_PTRS, // cl_kernel_exec_info param_name
            SvmPtrListSizeInBytes,        // size_t param_value_size
            pSvmPtrList                   // const void *param_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_EQ(3u, pMockKernel->kernelSvmGfxAllocations.size());
        EXPECT_TRUE(pMockKernel->svmAllocationsRequireCacheFlush);

        clSVMFree(pContext, ptrSvm1);
        clSVMFree(pContext, ptrSvm2);
    }
}

TEST_F(clSetKernelExecInfoTests, givenReadOnlySvmPtrListWhenUsedAsKernelPointersThenCacheFlushIsNotRequired) {
    if (svmCapabilities != 0) {
        void *ptrSvm1 = clSVMAlloc(pContext, CL_MEM_READ_ONLY, 256, 4);
        EXPECT_NE(nullptr, ptrSvm1);

        void *ptrSvm2 = clSVMAlloc(pContext, CL_MEM_READ_ONLY, 256, 4);
        EXPECT_NE(nullptr, ptrSvm2);

        void *pSvmPtrList[] = {ptrSvm1, ptrSvm2};
        size_t SvmPtrListSizeInBytes = 2 * sizeof(void *);

        retVal = clSetKernelExecInfo(
            pMockKernel,                  // cl_kernel kernel
            CL_KERNEL_EXEC_INFO_SVM_PTRS, // cl_kernel_exec_info param_name
            SvmPtrListSizeInBytes,        // size_t param_value_size
            pSvmPtrList                   // const void *param_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_EQ(2u, pMockKernel->kernelSvmGfxAllocations.size());
        EXPECT_FALSE(pMockKernel->svmAllocationsRequireCacheFlush);

        clSVMFree(pContext, ptrSvm1);
        clSVMFree(pContext, ptrSvm2);
    }
}

TEST_F(clSetKernelExecInfoTests, GivenMultipleSettingKernelInfoOperationsWhenSettingAdditionalKernelInfoThenSuccessIsReturned) {
    if (svmCapabilities != 0) {
        void *pSvmPtrList[] = {ptrSvm};
        size_t SvmPtrListSizeInBytes = 1 * sizeof(void *);

        retVal = clSetKernelExecInfo(
            pMockKernel,                  // cl_kernel kernel
            CL_KERNEL_EXEC_INFO_SVM_PTRS, // cl_kernel_exec_info param_name
            SvmPtrListSizeInBytes,        // size_t param_value_size
            pSvmPtrList                   // const void *param_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_EQ(1u, pMockKernel->kernelSvmGfxAllocations.size());

        retVal = clSetKernelExecInfo(
            pMockKernel,                  // cl_kernel kernel
            CL_KERNEL_EXEC_INFO_SVM_PTRS, // cl_kernel_exec_info param_name
            SvmPtrListSizeInBytes,        // size_t param_value_size
            pSvmPtrList                   // const void *param_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_EQ(1u, pMockKernel->kernelSvmGfxAllocations.size());
    }
}

HWTEST_F(clSetKernelExecInfoTests, givenKernelExecInfoThreadArbitrationPolicyWhenSettingAdditionalKernelInfoThenSuccessIsReturned) {
    uint32_t newThreadArbitrationPolicy = CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_ROUND_ROBIN_INTEL;
    size_t ptrSizeInBytes = sizeof(uint32_t *);

    retVal = clSetKernelExecInfo(
        pMockKernel,                                         // cl_kernel kernel
        CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_INTEL, // cl_kernel_exec_info param_name
        ptrSizeInBytes,                                      // size_t param_value_size
        &newThreadArbitrationPolicy                          // const void *param_value
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(getNewKernelArbitrationPolicy(newThreadArbitrationPolicy), pMockKernel->threadArbitrationPolicy);
    EXPECT_EQ(getNewKernelArbitrationPolicy(newThreadArbitrationPolicy), pMockKernel->getThreadArbitrationPolicy());
}

HWTEST_F(clSetKernelExecInfoTests, givenInvalidThreadArbitrationPolicyWhenSettingAdditionalKernelInfoThenClInvalidValueIsReturned) {
    uint32_t invalidThreadArbitrationPolicy = 0;
    size_t ptrSizeInBytes = 1 * sizeof(uint32_t *);

    retVal = clSetKernelExecInfo(
        pMockKernel,                                         // cl_kernel kernel
        CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_INTEL, // cl_kernel_exec_info param_name
        ptrSizeInBytes,                                      // size_t param_value_size
        &invalidThreadArbitrationPolicy                      // const void *param_value
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}
} // namespace ULT
