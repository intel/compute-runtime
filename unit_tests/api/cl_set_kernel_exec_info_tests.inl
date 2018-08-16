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

#include "cl_api_tests.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "test.h"

using namespace OCLRT;

class KernelExecInfoFixture : public api_fixture {
  protected:
    void SetUp() override {
        api_fixture::SetUp();

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

        api_fixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    MockKernel *pMockKernel = nullptr;
    std::unique_ptr<KernelInfo> pKernelInfo;
    void *ptrSvm = nullptr;
    cl_device_svm_capabilities svmCapabilities = 0;
};

typedef Test<KernelExecInfoFixture> clSetKernelExecInfoTests;

namespace ULT {

TEST_F(clSetKernelExecInfoTests, invalidKernel) {
    retVal = clSetKernelExecInfo(
        nullptr,                      // cl_kernel kernel
        CL_KERNEL_EXEC_INFO_SVM_PTRS, // cl_kernel_exec_info param_name
        0,                            // size_t param_value_size
        nullptr                       // const void *param_value
    );
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST_F(clSetKernelExecInfoTests, invalidValue_ParamValueIsNull) {
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

TEST_F(clSetKernelExecInfoTests, invalidValue_ParamValueHasNullPointer) {
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

TEST_F(clSetKernelExecInfoTests, invalidValue_ParamSizeIsZero) {
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

TEST_F(clSetKernelExecInfoTests, invalidValue_ParamSizeIsNotValid) {
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

TEST_F(clSetKernelExecInfoTests, invalidValue_ParamNameIsNotValid) {
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

TEST_F(clSetKernelExecInfoTests, invalidOperation) {
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

TEST_F(clSetKernelExecInfoTests, success_SvmPtrListWithSinglePointer) {
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

        EXPECT_EQ(1u, pMockKernel->getKernelSvmGfxAllocations().size());
    }
}

TEST_F(clSetKernelExecInfoTests, success_SvmPtrListWithMultiplePointers) {
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

        EXPECT_EQ(3u, pMockKernel->getKernelSvmGfxAllocations().size());

        clSVMFree(pContext, ptrSvm1);
        clSVMFree(pContext, ptrSvm2);
    }
}

TEST_F(clSetKernelExecInfoTests, success_MultipleSetKernelExecInfo) {
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

        EXPECT_EQ(1u, pMockKernel->getKernelSvmGfxAllocations().size());

        retVal = clSetKernelExecInfo(
            pMockKernel,                  // cl_kernel kernel
            CL_KERNEL_EXEC_INFO_SVM_PTRS, // cl_kernel_exec_info param_name
            SvmPtrListSizeInBytes,        // size_t param_value_size
            pSvmPtrList                   // const void *param_value
        );
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_EQ(1u, pMockKernel->getKernelSvmGfxAllocations().size());
    }
}
} // namespace ULT
