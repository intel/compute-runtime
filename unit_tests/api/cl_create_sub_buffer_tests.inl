/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/context/context.h"

#include "cl_api_tests.h"

using namespace NEO;

namespace ClCreateSubbufferTests {

template <bool hasHostPtr, cl_mem_flags parentFlags>
class clCreateSubBufferTemplateTests : public api_fixture,
                                       public testing::TestWithParam<uint64_t /*cl_mem_flags*/> {
    void SetUp() override {
        api_fixture::SetUp();
        cl_mem_flags flg = parentFlags;
        void *ptr = nullptr;

        if (hasHostPtr == true) {
            flg |= CL_MEM_USE_HOST_PTR;
            ptr = pHostPtr;
        }

        buffer = clCreateBuffer(pContext, flg, 64, ptr, &retVal);
        EXPECT_NE(nullptr, buffer);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    void TearDown() override {
        clReleaseMemObject(buffer);
        api_fixture::TearDown();
    }

  protected:
    cl_mem buffer;
    cl_uchar pHostPtr[64];
};

struct clCreateSubBufferValidFlagsNoHostPtrTests
    : public clCreateSubBufferTemplateTests<false, CL_MEM_READ_WRITE> {
};

TEST_P(clCreateSubBufferValidFlagsNoHostPtrTests, GivenValidFlagsWhenCreatingSubBufferThenSubBufferIsCreatedAndSuccessIsReturned) {
    cl_buffer_region region = {0, 12};
    cl_mem_flags flags = GetParam();

    auto subBuffer = clCreateSubBuffer(buffer, flags, CL_BUFFER_CREATE_TYPE_REGION,
                                       &region, &retVal);
    EXPECT_NE(nullptr, subBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseMemObject(subBuffer);
};

static cl_mem_flags validFlags[] = {
    CL_MEM_READ_WRITE,
    CL_MEM_WRITE_ONLY,
    CL_MEM_READ_ONLY,
    CL_MEM_HOST_READ_ONLY,
    CL_MEM_HOST_WRITE_ONLY,
    CL_MEM_HOST_NO_ACCESS,
};

INSTANTIATE_TEST_CASE_P(
    CreateSubBufferCheckFlags,
    clCreateSubBufferValidFlagsNoHostPtrTests,
    testing::ValuesIn(validFlags));

struct clCreateSubBufferInvalidFlagsHostPtrTests
    : public clCreateSubBufferTemplateTests<true, CL_MEM_HOST_NO_ACCESS | CL_MEM_READ_ONLY> {
};

TEST_P(clCreateSubBufferInvalidFlagsHostPtrTests, GivenInvalidFlagsWhenCreatingSubBufferThenInvalidValueErrorIsReturned) {
    cl_buffer_region region = {4, 12};
    cl_mem_flags flags = GetParam();

    auto subBuffer = clCreateSubBuffer(buffer, flags, CL_BUFFER_CREATE_TYPE_REGION,
                                       &region, &retVal);
    EXPECT_EQ(nullptr, subBuffer);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
};

cl_mem_flags invalidFlags[] = {
    CL_MEM_READ_WRITE,
    CL_MEM_WRITE_ONLY,
    CL_MEM_HOST_WRITE_ONLY,
    CL_MEM_HOST_READ_ONLY,
    CL_MEM_USE_HOST_PTR,
    CL_MEM_ALLOC_HOST_PTR,
    CL_MEM_COPY_HOST_PTR,
    0xffcc,
};

INSTANTIATE_TEST_CASE_P(
    CreateSubBufferCheckFlags,
    clCreateSubBufferInvalidFlagsHostPtrTests,
    testing::ValuesIn(invalidFlags));

class clCreateSubBufferTests : public api_tests {
    void SetUp() override {
        api_tests::SetUp();
        cl_mem_flags flg = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR | CL_MEM_HOST_NO_ACCESS;

        buffer = clCreateBuffer(pContext, flg, 64, pHostPtr, &retVal);
        EXPECT_NE(nullptr, buffer);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    void TearDown() override {
        clReleaseMemObject(buffer);
        api_fixture::TearDown();
    }

  protected:
    cl_mem buffer;
    cl_uchar pHostPtr[64];
};

TEST_F(clCreateSubBufferTests, GivenInBoundsRegionWhenCreatingSubBufferThenSubBufferIsCreatedAndSuccessIsReturned) {
    cl_buffer_region region = {0, 12};

    auto subBuffer = clCreateSubBuffer(buffer, CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION,
                                       &region, &retVal);
    EXPECT_NE(nullptr, subBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseMemObject(subBuffer);
}

TEST_F(clCreateSubBufferTests, GivenOutOfBoundsRegionWhenCreatingSubBufferThenInvalidValueErrorIsReturned) {
    cl_buffer_region region = {4, 68};

    auto subBuffer = clCreateSubBuffer(buffer, CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION,
                                       &region, &retVal);
    EXPECT_EQ(nullptr, subBuffer);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clCreateSubBufferTests, GivenSubBufferAsBufferWhenCreatingSubBufferThenInvalidMemObjectErrorIsReturned) {
    cl_buffer_region region0 = {0, 60};
    cl_buffer_region region1 = {8, 20};

    auto subBuffer = clCreateSubBuffer(buffer, CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION,
                                       &region0, &retVal);
    EXPECT_NE(nullptr, subBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto subsubBuffer = clCreateSubBuffer(subBuffer, CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION,
                                          &region1, &retVal);
    EXPECT_EQ(nullptr, subsubBuffer);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);

    clReleaseMemObject(subBuffer);
}

TEST_F(clCreateSubBufferTests, GivenInvalidBufferObjectWhenCreatingSubBufferThenInvalidMemObjectErrorIsReturned) {
    cl_buffer_region region = {4, 60};
    cl_int trash[] = {0x01, 0x08, 0x88, 0xcc, 0xab, 0x55};

    auto subBuffer = clCreateSubBuffer(reinterpret_cast<cl_mem>(trash), CL_MEM_READ_WRITE,
                                       CL_BUFFER_CREATE_TYPE_REGION, &region, &retVal);
    EXPECT_EQ(nullptr, subBuffer);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clCreateSubBufferTests, GivenInvalidOffsetWhenCreatingSubBufferThenMisalignedSubBufferOffsetErrorIsReturned) {
    cl_buffer_region region = {1, 60};

    auto subBuffer = clCreateSubBuffer(buffer, CL_MEM_READ_WRITE,
                                       CL_BUFFER_CREATE_TYPE_REGION, &region, &retVal);
    EXPECT_EQ(nullptr, subBuffer);
    EXPECT_EQ(CL_MISALIGNED_SUB_BUFFER_OFFSET, retVal);
}

TEST_F(clCreateSubBufferTests, GivenNoRegionWhenCreatingSubBufferThenInvalidValueErrorIsReturned) {
    cl_buffer_region region = {4, 60};

    auto subBuffer = clCreateSubBuffer(buffer, CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION,
                                       nullptr, &retVal);
    EXPECT_EQ(nullptr, subBuffer);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    subBuffer = clCreateSubBuffer(buffer, CL_MEM_READ_WRITE, 0, &region, &retVal);
    EXPECT_EQ(nullptr, subBuffer);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    subBuffer = clCreateSubBuffer(buffer, CL_MEM_READ_WRITE, 0, nullptr, &retVal);
    EXPECT_EQ(nullptr, subBuffer);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clCreateSubBufferTests, GivenBufferWithFlagsWhenCreatingSubBufferThenFlagsAreInherited) {
    cl_buffer_region region = {0, 60};

    auto subBuffer = clCreateSubBuffer(buffer, CL_MEM_READ_ONLY, CL_BUFFER_CREATE_TYPE_REGION,
                                       &region, &retVal);
    EXPECT_NE(nullptr, subBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
    cl_mem_flags retFlag;
    size_t retSZ;

    retVal = clGetMemObjectInfo(subBuffer, CL_MEM_FLAGS, sizeof(cl_mem_flags), &retFlag, &retSZ);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_mem_flags), retSZ);
    EXPECT_EQ(static_cast<uint64_t /*cl_mem_flags*/>(CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR | CL_MEM_HOST_NO_ACCESS), retFlag);

    clReleaseMemObject(subBuffer);
}
} // namespace ClCreateSubbufferTests
