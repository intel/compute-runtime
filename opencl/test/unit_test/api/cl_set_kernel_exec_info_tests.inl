/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/test/unit_test/command_stream/thread_arbitration_policy_helper.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "cl_api_tests.h"

using namespace NEO;

class KernelExecInfoFixture : public ApiFixture<> {
  protected:
    void SetUp() override {
        ApiFixture::SetUp();

        pKernelInfo = std::make_unique<KernelInfo>();
        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

        pMockMultiDeviceKernel = MultiDeviceKernel::create<MockKernel>(pProgram, MockKernel::toKernelInfoContainer(*pKernelInfo, testedRootDeviceIndex), nullptr);
        pMockKernel = static_cast<MockKernel *>(pMockMultiDeviceKernel->getKernel(testedRootDeviceIndex));
        ASSERT_NE(nullptr, pMockKernel);
        svmCapabilities = pDevice->getDeviceInfo().svmCapabilities;
        if (svmCapabilities != 0) {
            ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
            EXPECT_NE(nullptr, ptrSvm);
        }
    }

    void TearDown() override {
        if (svmCapabilities != 0) {
            clSVMFree(pContext, ptrSvm);
        }

        if (pMockMultiDeviceKernel) {
            delete pMockMultiDeviceKernel;
        }

        ApiFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    MockKernel *pMockKernel = nullptr;
    MultiDeviceKernel *pMockMultiDeviceKernel = nullptr;
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

TEST_F(clSetKernelExecInfoTests, GivenDeviceNotSupportingSvmWhenSettingKernelExecInfoThenErrorIsReturnedOnSvmRelatedParams) {
    auto &hwHelper = NEO::ClHwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);
    if (!hwHelper.isSupportedKernelThreadArbitrationPolicy()) {
        GTEST_SKIP();
    }
    auto hwInfo = executionEnvironment->rootDeviceEnvironments[ApiFixture::testedRootDeviceIndex]->getMutableHardwareInfo();
    VariableBackup<bool> ftrSvm{&hwInfo->capabilityTable.ftrSvm, false};

    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel(MultiDeviceKernel::create<MockKernel>(
        pProgram, MockKernel::toKernelInfoContainer(*pKernelInfo, testedRootDeviceIndex), nullptr));

    uint32_t newPolicy = CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_ROUND_ROBIN_INTEL;
    retVal = clSetKernelExecInfo(
        pMockMultiDeviceKernel,                              // cl_kernel kernel
        CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_INTEL, // cl_kernel_exec_info param_name
        sizeof(newPolicy),                                   // size_t param_value_size
        &newPolicy                                           // const void *param_value
    );
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_kernel_exec_info svmParams[] = {CL_KERNEL_EXEC_INFO_SVM_PTRS, CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM};
    for (auto svmParam : svmParams) {
        retVal = clSetKernelExecInfo(
            pMockMultiDeviceKernel, // cl_kernel kernel
            svmParam,               // cl_kernel_exec_info param_name
            0,                      // size_t param_value_size
            nullptr                 // const void *param_value
        );
        EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    }
}

TEST_F(clSetKernelExecInfoTests, GivenNullParamValueWhenSettingAdditionalKernelInfoThenInvalidValueErrorIsReturned) {
    REQUIRE_SVM_OR_SKIP(defaultHwInfo);
    void **pSvmPtrList = nullptr;
    size_t svmPtrListSizeInBytes = 1 * sizeof(void *);

    retVal = clSetKernelExecInfo(
        pMockMultiDeviceKernel,       // cl_kernel kernel
        CL_KERNEL_EXEC_INFO_SVM_PTRS, // cl_kernel_exec_info param_name
        svmPtrListSizeInBytes,        // size_t param_value_size
        pSvmPtrList                   // const void *param_value
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clSetKernelExecInfoTests, GivenNullPointerInParamValueWhenSettingAdditionalKernelInfoThenInvalidValueErrorIsReturned) {
    REQUIRE_SVM_OR_SKIP(defaultHwInfo);
    void *pSvmPtrList[] = {nullptr};
    size_t svmPtrListSizeInBytes = 1 * sizeof(void *);

    retVal = clSetKernelExecInfo(
        pMockMultiDeviceKernel,       // cl_kernel kernel
        CL_KERNEL_EXEC_INFO_SVM_PTRS, // cl_kernel_exec_info param_name
        svmPtrListSizeInBytes,        // size_t param_value_size
        pSvmPtrList                   // const void *param_value
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clSetKernelExecInfoTests, GivenParamSizeZeroWhenSettingAdditionalKernelInfoThenInvalidValueErrorIsReturned) {
    REQUIRE_SVM_OR_SKIP(defaultHwInfo);
    void *pSvmPtrList[] = {ptrSvm};
    size_t svmPtrListSizeInBytes = 0;

    retVal = clSetKernelExecInfo(
        pMockMultiDeviceKernel,       // cl_kernel kernel
        CL_KERNEL_EXEC_INFO_SVM_PTRS, // cl_kernel_exec_info param_name
        svmPtrListSizeInBytes,        // size_t param_value_size
        pSvmPtrList                   // const void *param_value
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clSetKernelExecInfoTests, GivenInvalidParamSizeWhenSettingAdditionalKernelInfoThenInvalidValueErrorIsReturned) {
    REQUIRE_SVM_OR_SKIP(defaultHwInfo);
    void *pSvmPtrList[] = {ptrSvm};
    size_t svmPtrListSizeInBytes = (size_t)(-1);

    retVal = clSetKernelExecInfo(
        pMockMultiDeviceKernel,       // cl_kernel kernel
        CL_KERNEL_EXEC_INFO_SVM_PTRS, // cl_kernel_exec_info param_name
        svmPtrListSizeInBytes,        // size_t param_value_size
        pSvmPtrList                   // const void *param_value
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clSetKernelExecInfoTests, GivenInvalidParamNameWhenSettingAdditionalKernelInfoThenInvalidValueErrorIsReturned) {
    void *pSvmPtrList[] = {ptrSvm};
    size_t svmPtrListSizeInBytes = 1 * sizeof(void *);

    retVal = clSetKernelExecInfo(
        pMockMultiDeviceKernel, // cl_kernel kernel
        0,                      // cl_kernel_exec_info param_name
        svmPtrListSizeInBytes,  // size_t param_value_size
        pSvmPtrList             // const void *param_value
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clSetKernelExecInfoTests, GivenInvalidOperationWhenSettingAdditionalKernelInfoThenInvalidOperationErrorIsReturned) {
    void *pSvmPtrList[] = {ptrSvm};
    size_t svmPtrListSizeInBytes = 1 * sizeof(void *);

    retVal = clSetKernelExecInfo(
        pMockMultiDeviceKernel,                    // cl_kernel kernel
        CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM, // cl_kernel_exec_info param_name
        svmPtrListSizeInBytes,                     // size_t param_value_size
        pSvmPtrList                                // const void *param_value
    );
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

TEST_F(clSetKernelExecInfoTests, GivenValidPointerListWithOnePointerWhenSettingAdditionalKernelInfoThenSuccessIsReturned) {
    if (svmCapabilities != 0) {
        void *pSvmPtrList[] = {ptrSvm};
        size_t svmPtrListSizeInBytes = 1 * sizeof(void *);

        retVal = clSetKernelExecInfo(
            pMockMultiDeviceKernel,       // cl_kernel kernel
            CL_KERNEL_EXEC_INFO_SVM_PTRS, // cl_kernel_exec_info param_name
            svmPtrListSizeInBytes,        // size_t param_value_size
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
        size_t svmPtrListSizeInBytes = 3 * sizeof(void *);

        retVal = clSetKernelExecInfo(
            pMockMultiDeviceKernel,       // cl_kernel kernel
            CL_KERNEL_EXEC_INFO_SVM_PTRS, // cl_kernel_exec_info param_name
            svmPtrListSizeInBytes,        // size_t param_value_size
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
        size_t svmPtrListSizeInBytes = 2 * sizeof(void *);

        retVal = clSetKernelExecInfo(
            pMockMultiDeviceKernel,       // cl_kernel kernel
            CL_KERNEL_EXEC_INFO_SVM_PTRS, // cl_kernel_exec_info param_name
            svmPtrListSizeInBytes,        // size_t param_value_size
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
        size_t svmPtrListSizeInBytes = 1 * sizeof(void *);

        retVal = clSetKernelExecInfo(
            pMockMultiDeviceKernel,       // cl_kernel kernel
            CL_KERNEL_EXEC_INFO_SVM_PTRS, // cl_kernel_exec_info param_name
            svmPtrListSizeInBytes,        // size_t param_value_size
            pSvmPtrList                   // const void *param_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_EQ(1u, pMockKernel->kernelSvmGfxAllocations.size());

        retVal = clSetKernelExecInfo(
            pMockMultiDeviceKernel,       // cl_kernel kernel
            CL_KERNEL_EXEC_INFO_SVM_PTRS, // cl_kernel_exec_info param_name
            svmPtrListSizeInBytes,        // size_t param_value_size
            pSvmPtrList                   // const void *param_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_EQ(1u, pMockKernel->kernelSvmGfxAllocations.size());
    }
}

TEST_F(clSetKernelExecInfoTests, givenNonExistingParamNameWithValuesWhenSettingAdditionalKernelInfoThenInvalidValueIsReturned) {
    uint32_t paramName = 1234u;
    size_t size = sizeof(cl_bool);
    retVal = clSetKernelExecInfo(pMockMultiDeviceKernel, paramName, size, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    size = 2 * sizeof(cl_bool);
    cl_bool paramValue = CL_TRUE;
    retVal = clSetKernelExecInfo(pMockMultiDeviceKernel, paramName, size, &paramValue);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = clSetKernelExecInfo(pMockMultiDeviceKernel, paramName, size, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    size = sizeof(cl_bool);
    paramValue = CL_FALSE;
    retVal = clSetKernelExecInfo(pMockMultiDeviceKernel, paramName, size, &paramValue);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    paramValue = CL_TRUE;
    retVal = clSetKernelExecInfo(pMockMultiDeviceKernel, paramName, size, &paramValue);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

HWTEST_F(clSetKernelExecInfoTests, givenKernelExecInfoThreadArbitrationPolicyWhenSettingAdditionalKernelInfoThenSuccessIsReturned) {
    auto &hwHelper = NEO::ClHwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);
    if (!hwHelper.isSupportedKernelThreadArbitrationPolicy()) {
        GTEST_SKIP();
    }
    uint32_t newThreadArbitrationPolicy = CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_ROUND_ROBIN_INTEL;
    size_t ptrSizeInBytes = sizeof(uint32_t *);

    retVal = clSetKernelExecInfo(
        pMockMultiDeviceKernel,                              // cl_kernel kernel
        CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_INTEL, // cl_kernel_exec_info param_name
        ptrSizeInBytes,                                      // size_t param_value_size
        &newThreadArbitrationPolicy                          // const void *param_value
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(getNewKernelArbitrationPolicy(newThreadArbitrationPolicy), pMockKernel->threadArbitrationPolicy);
    EXPECT_EQ(getNewKernelArbitrationPolicy(newThreadArbitrationPolicy), pMockKernel->getThreadArbitrationPolicy());
}

HWTEST_F(clSetKernelExecInfoTests, givenKernelExecInfoThreadArbitrationPolicyWhenNotSupportedAndSettingAdditionalKernelInfoThenClInvalidDeviceIsReturned) {
    auto &hwHelper = NEO::ClHwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);
    if (hwHelper.isSupportedKernelThreadArbitrationPolicy()) {
        GTEST_SKIP();
    }
    uint32_t newThreadArbitrationPolicy = CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_ROUND_ROBIN_INTEL;
    size_t ptrSizeInBytes = sizeof(uint32_t *);

    retVal = clSetKernelExecInfo(
        pMockMultiDeviceKernel,                              // cl_kernel kernel
        CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_INTEL, // cl_kernel_exec_info param_name
        ptrSizeInBytes,                                      // size_t param_value_size
        &newThreadArbitrationPolicy                          // const void *param_value
    );
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

HWTEST_F(clSetKernelExecInfoTests, givenInvalidThreadArbitrationPolicyWhenSettingAdditionalKernelInfoThenClInvalidValueIsReturned) {
    auto &hwHelper = NEO::ClHwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);
    if (!hwHelper.isSupportedKernelThreadArbitrationPolicy()) {
        GTEST_SKIP();
    }
    uint32_t invalidThreadArbitrationPolicy = 0;
    size_t ptrSizeInBytes = 1 * sizeof(uint32_t *);

    retVal = clSetKernelExecInfo(
        pMockMultiDeviceKernel,                              // cl_kernel kernel
        CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_INTEL, // cl_kernel_exec_info param_name
        ptrSizeInBytes,                                      // size_t param_value_size
        &invalidThreadArbitrationPolicy                      // const void *param_value
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

HWTEST_F(clSetKernelExecInfoTests, givenInvalidParamSizeWhenSettingKernelExecutionTypeThenClInvalidValueErrorIsReturned) {
    cl_execution_info_kernel_type_intel kernelExecutionType = 0;

    retVal = clSetKernelExecInfo(
        pMockMultiDeviceKernel,                          // cl_kernel kernel
        CL_KERNEL_EXEC_INFO_KERNEL_TYPE_INTEL,           // cl_kernel_exec_info param_name
        sizeof(cl_execution_info_kernel_type_intel) - 1, // size_t param_value_size
        &kernelExecutionType                             // const void *param_value
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

HWTEST_F(clSetKernelExecInfoTests, givenInvalidParamValueWhenSettingKernelExecutionTypeThenClInvalidValueErrorIsReturned) {
    retVal = clSetKernelExecInfo(
        pMockMultiDeviceKernel,                      // cl_kernel kernel
        CL_KERNEL_EXEC_INFO_KERNEL_TYPE_INTEL,       // cl_kernel_exec_info param_name
        sizeof(cl_execution_info_kernel_type_intel), // size_t param_value_size
        nullptr                                      // const void *param_value
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

HWTEST_F(clSetKernelExecInfoTests, givenDifferentExecutionTypesWhenSettingAdditionalKernelInfoThenCorrectValuesAreSet) {
    cl_kernel_exec_info paramName = CL_KERNEL_EXEC_INFO_KERNEL_TYPE_INTEL;
    size_t paramSize = sizeof(cl_execution_info_kernel_type_intel);
    cl_execution_info_kernel_type_intel kernelExecutionType = -1;

    retVal = clSetKernelExecInfo(
        pMockMultiDeviceKernel, // cl_kernel kernel
        paramName,              // cl_kernel_exec_info param_name
        paramSize,              // size_t param_value_size
        &kernelExecutionType    // const void *param_value
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    kernelExecutionType = CL_KERNEL_EXEC_INFO_DEFAULT_TYPE_INTEL;
    retVal = clSetKernelExecInfo(
        pMockMultiDeviceKernel, // cl_kernel kernel
        paramName,              // cl_kernel_exec_info param_name
        paramSize,              // size_t param_value_size
        &kernelExecutionType    // const void *param_value
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(KernelExecutionType::Default, pMockKernel->executionType);

    kernelExecutionType = CL_KERNEL_EXEC_INFO_CONCURRENT_TYPE_INTEL;
    retVal = clSetKernelExecInfo(
        pMockMultiDeviceKernel, // cl_kernel kernel
        paramName,              // cl_kernel_exec_info param_name
        paramSize,              // size_t param_value_size
        &kernelExecutionType    // const void *param_value
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(KernelExecutionType::Concurrent, pMockKernel->executionType);
}

} // namespace ULT
