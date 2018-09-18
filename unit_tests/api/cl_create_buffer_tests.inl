/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"
#include "runtime/context/context.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/mem_obj/buffer.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include "unit_tests/mocks/mock_program.h"

using namespace OCLRT;

typedef api_tests clCreateBufferTests;

namespace ClCreateBufferTests {

class clCreateBufferTemplateTests : public api_fixture,
                                    public testing::TestWithParam<uint64_t> {
    void SetUp() override {
        api_fixture::SetUp();
    }

    void TearDown() override {
        api_fixture::TearDown();
    }
};

struct clCreateBufferValidFlagsTests : public clCreateBufferTemplateTests {
    cl_uchar pHostPtr[64];
};

TEST_P(clCreateBufferValidFlagsTests, validFlags) {
    cl_mem_flags flags = GetParam() | CL_MEM_USE_HOST_PTR;

    auto buffer = clCreateBuffer(pContext, flags, 64, pHostPtr, &retVal);
    EXPECT_NE(nullptr, buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseMemObject(buffer);
};

static cl_mem_flags validFlags[] = {
    CL_MEM_READ_WRITE | CL_MEM_HOST_READ_ONLY,
    CL_MEM_WRITE_ONLY,
    CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY,
    CL_MEM_HOST_READ_ONLY,
    CL_MEM_HOST_WRITE_ONLY,
    CL_MEM_HOST_NO_ACCESS,
};

INSTANTIATE_TEST_CASE_P(
    CreateBufferCheckFlags,
    clCreateBufferValidFlagsTests,
    testing::ValuesIn(validFlags));

struct clCreateBufferInValidFlagsTests : public clCreateBufferTemplateTests {
};

TEST_P(clCreateBufferInValidFlagsTests, inValidFlags) {
    cl_mem_flags flags = GetParam();

    auto buffer = clCreateBuffer(pContext, flags, 64, nullptr, &retVal);
    EXPECT_EQ(nullptr, buffer);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
};

cl_mem_flags inValidFlags[] = {
    CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY,
    CL_MEM_READ_WRITE | CL_MEM_READ_ONLY,
    CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY,
    CL_MEM_ALLOC_HOST_PTR | CL_MEM_USE_HOST_PTR,
    CL_MEM_USE_HOST_PTR | CL_MEM_COPY_HOST_PTR,
    CL_MEM_HOST_NO_ACCESS | CL_MEM_HOST_WRITE_ONLY,
    CL_MEM_HOST_NO_ACCESS | CL_MEM_HOST_READ_ONLY,
    CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_WRITE_ONLY,
    0xffcc,
};

INSTANTIATE_TEST_CASE_P(
    CreateBufferCheckFlags,
    clCreateBufferInValidFlagsTests,
    testing::ValuesIn(inValidFlags));

TEST_F(clCreateBufferTests, GivenValidParametersWhenCreatingBufferThenSuccessIsReturned) {
    unsigned char *pHostMem = nullptr;
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    static const unsigned int bufferSize = 16;
    cl_mem buffer = nullptr;

    pHostMem = new unsigned char[bufferSize];
    memset(pHostMem, 0xaa, bufferSize);

    buffer = clCreateBuffer(pContext, flags, bufferSize, pHostMem, &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete[] pHostMem;
}

TEST_F(clCreateBufferTests, GivenNullContextWhenCreatingBufferThenInvalidContextErrorIsReturned) {
    unsigned char *pHostMem = nullptr;
    cl_mem_flags flags = 0;
    static const unsigned int bufferSize = 16;

    clCreateBuffer(nullptr, flags, bufferSize, pHostMem, &retVal);
    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(clCreateBufferTests, GivenBufferSizeZeroWhenCreatingBufferThenInvalidBufferSizeErrorIsReturned) {
    uint8_t hostData = 0;
    clCreateBuffer(pContext, CL_MEM_USE_HOST_PTR, 0, &hostData, &retVal);
    ASSERT_EQ(CL_INVALID_BUFFER_SIZE, retVal);
}

TEST_F(clCreateBufferTests, GivenInvalidHostPointerWhenCreatingBufferThenInvalidHostPointerErrorIsReturned) {
    uint32_t hostData = 0;
    cl_mem_flags flags = 0;
    clCreateBuffer(pContext, flags, sizeof(uint32_t), &hostData, &retVal);
    ASSERT_EQ(CL_INVALID_HOST_PTR, retVal);
}

TEST_F(clCreateBufferTests, GivenNullHostPointerAndMemCopyHostPtrFlagWhenCreatingBufferThenInvalidHostPointerErrorIsReturned) {
    cl_mem_flags flags = CL_MEM_COPY_HOST_PTR;
    clCreateBuffer(pContext, flags, sizeof(uint32_t), nullptr, &retVal);
    ASSERT_EQ(CL_INVALID_HOST_PTR, retVal);
}

TEST_F(clCreateBufferTests, GivenNullHostPointerAndMemUseHostPtrFlagWhenCreatingBufferThenInvalidHostPointerErrorIsReturned) {
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    clCreateBuffer(pContext, flags, sizeof(uint32_t), nullptr, &retVal);
    ASSERT_EQ(CL_INVALID_HOST_PTR, retVal);
}

TEST_F(clCreateBufferTests, GivenMemWriteOnlyFlagAndMemReadWriteFlagWhenCreatingBufferThenInvalidValueErrorIsReturned) {
    cl_mem_flags flags = CL_MEM_WRITE_ONLY | CL_MEM_READ_WRITE;
    clCreateBuffer(pContext, flags, 16, nullptr, &retVal);
    ASSERT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clCreateBufferTests, GivenNullHostPointerAndMemCopyHostPtrFlagWhenCreatingBufferThenNullIsReturned) {
    unsigned char *pHostMem = nullptr;
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    static const unsigned int bufferSize = 16;
    cl_mem buffer = nullptr;

    pHostMem = new unsigned char[bufferSize];
    memset(pHostMem, 0xaa, bufferSize);

    buffer = clCreateBuffer(pContext, flags, bufferSize, pHostMem, nullptr);

    EXPECT_NE(nullptr, buffer);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete[] pHostMem;
}

using clCreateBufferTestsWithRestrictions = api_test_using_aligned_memory_manager;

TEST_F(clCreateBufferTestsWithRestrictions, GivenMemoryManagerRestrictionsWhenMinIsLessThanHostPtrThenUseZeroCopy) {
    std::unique_ptr<unsigned char[]> hostMem(nullptr);
    unsigned char *destMem = nullptr;
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    const unsigned int bufferSize = MemoryConstants::pageSize * 3;
    const unsigned int destBufferSize = MemoryConstants::pageSize;
    cl_mem buffer = nullptr;

    uintptr_t minAddress = 0;
    MockAllocSysMemAgnosticMemoryManager *memMngr = reinterpret_cast<MockAllocSysMemAgnosticMemoryManager *>(device->getMemoryManager());
    memMngr->ptrRestrictions = &memMngr->testRestrictions;
    EXPECT_EQ(minAddress, memMngr->ptrRestrictions->minAddress);

    hostMem.reset(new unsigned char[bufferSize]);

    destMem = hostMem.get();
    destMem += MemoryConstants::pageSize;
    destMem -= (reinterpret_cast<uintptr_t>(destMem) % MemoryConstants::pageSize);

    buffer = clCreateBuffer(context, flags, destBufferSize, destMem, &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    Buffer *bufferObj = OCLRT::castToObject<Buffer>(buffer);
    EXPECT_TRUE(bufferObj->isMemObjZeroCopy());

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateBufferTestsWithRestrictions, GivenMemoryManagerRestrictionsWhenMinIsLessThanHostPtrThenCreateCopy) {
    std::unique_ptr<unsigned char[]> hostMem(nullptr);
    unsigned char *destMem = nullptr;
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    const unsigned int realBufferSize = MemoryConstants::pageSize * 3;
    const unsigned int destBufferSize = MemoryConstants::pageSize;
    cl_mem buffer = nullptr;

    MockAllocSysMemAgnosticMemoryManager *memMngr = reinterpret_cast<MockAllocSysMemAgnosticMemoryManager *>(device->getMemoryManager());
    memMngr->ptrRestrictions = &memMngr->testRestrictions;

    hostMem.reset(new unsigned char[realBufferSize]);

    destMem = hostMem.get();
    destMem += MemoryConstants::pageSize;
    destMem -= (reinterpret_cast<uintptr_t>(destMem) % MemoryConstants::pageSize);
    memMngr->ptrRestrictions->minAddress = reinterpret_cast<uintptr_t>(destMem) + 1;

    buffer = clCreateBuffer(context, flags, destBufferSize, destMem, &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    Buffer *bufferObj = OCLRT::castToObject<Buffer>(buffer);
    EXPECT_FALSE(bufferObj->isMemObjZeroCopy());

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

} // namespace ClCreateBufferTests
