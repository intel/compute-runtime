/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/string.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/api/api.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/aub_tests/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <tuple>

using namespace NEO;

struct VerifyMemoryBufferHw
    : public CommandEnqueueAUBFixture,
      public ::testing::TestWithParam<std::tuple<size_t, uint64_t>> {

    void SetUp() override {
        CommandEnqueueAUBFixture::setUp();
    }

    void TearDown() override {
        CommandEnqueueAUBFixture::tearDown();
    }
};

size_t testDataSizeTable[] = {
    16,
    MemoryConstants::megaByte};

cl_mem_flags testFlagsTable[] = {
    0,
    CL_MEM_COPY_HOST_PTR};

HWTEST_P(VerifyMemoryBufferHw, givenDifferentBuffersWhenValidatingMemoryThenSuccessIsReturned) {
    cl_uint testItem = 5;
    cl_uint testItemWrong1 = 4;
    cl_uint testItemWrong2 = 6;
    auto testItemSize = sizeof(testItem);

    const auto testDataSize = std::get<0>(GetParam());
    EXPECT_FALSE(testDataSize < testItemSize);
    const auto flags = std::get<1>(GetParam());
    const auto usesHostPointer = ((flags & CL_MEM_USE_HOST_PTR) ||
                                  (flags & CL_MEM_COPY_HOST_PTR));

    DebugManagerStateRestore restore;
    debugManager.flags.DisableZeroCopyForBuffers.set(true);

    std::unique_ptr<uint8_t[]> bufferContent(new uint8_t[testDataSize]);
    std::unique_ptr<uint8_t[]> validContent(new uint8_t[testDataSize]);
    std::unique_ptr<uint8_t[]> invalidContent1(new uint8_t[testDataSize]);
    std::unique_ptr<uint8_t[]> invalidContent2(new uint8_t[testDataSize]);

    auto *buf32 = reinterpret_cast<cl_uint *>(bufferContent.get());
    std::fill(buf32, buf32 + testDataSize / testItemSize, testItem);
    memcpy_s(validContent.get(), testDataSize, bufferContent.get(), testDataSize);
    memcpy_s(invalidContent1.get(), testDataSize, bufferContent.get(), testDataSize);
    memcpy_s(invalidContent2.get(), testDataSize, bufferContent.get(), testDataSize);

    // set last item for invalid contents
    size_t offset = testDataSize - testItemSize;
    memcpy_s(invalidContent1.get() + offset, testItemSize, &testItemWrong1, testItemSize);
    memcpy_s(invalidContent2.get() + offset, testItemSize, &testItemWrong2, testItemSize);

    MockContext context(this->pCmdQ->getDevice().getSpecializedDevice<ClDevice>());
    cl_int retVal = CL_INVALID_VALUE;

    std::unique_ptr<Buffer> buffer(Buffer::create(
        &context,
        flags,
        testDataSize,
        (usesHostPointer ? bufferContent.get() : nullptr),
        retVal));
    EXPECT_NE(nullptr, buffer);

    if (!usesHostPointer) {
        retVal = pCmdQ->enqueueFillBuffer(
            buffer.get(),
            &testItem,
            testItemSize,
            0,
            testDataSize,
            0,
            nullptr,
            nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    auto mappedAddress = clEnqueueMapBuffer(pCmdQ, buffer.get(), CL_FALSE, CL_MAP_READ, 0, testDataSize, 0, nullptr, nullptr, nullptr);
    clFlush(pCmdQ);

    retVal = clEnqueueVerifyMemoryINTEL(pCmdQ, mappedAddress, validContent.get(), testDataSize, CL_MEM_COMPARE_EQUAL);
    EXPECT_EQ(CL_SUCCESS, retVal);

    if (UnitTestHelper<FamilyType>::isExpectMemoryNotEqualSupported()) {
        retVal = clEnqueueVerifyMemoryINTEL(pCmdQ, mappedAddress, invalidContent1.get(), testDataSize, CL_MEM_COMPARE_NOT_EQUAL);
        EXPECT_EQ(CL_SUCCESS, retVal);
        retVal = clEnqueueVerifyMemoryINTEL(pCmdQ, mappedAddress, invalidContent2.get(), testDataSize, CL_MEM_COMPARE_NOT_EQUAL);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    clFinish(pCmdQ);
}

INSTANTIATE_TEST_SUITE_P(VerifyMemoryBuffer,
                         VerifyMemoryBufferHw,
                         ::testing::Combine(
                             ::testing::ValuesIn(testDataSizeTable),
                             ::testing::ValuesIn(testFlagsTable)));
