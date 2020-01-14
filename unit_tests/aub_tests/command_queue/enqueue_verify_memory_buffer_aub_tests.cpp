/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/api/api.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/mem_obj/buffer.h"
#include "test.h"
#include "unit_tests/aub_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/helpers/unit_test_helper.h"
#include "unit_tests/mocks/mock_context.h"

using namespace NEO;

struct VerifyMemoryBufferHw
    : public CommandEnqueueAUBFixture,
      public ::testing::TestWithParam<std::tuple<size_t, uint64_t>> {

    void SetUp() override {
        CommandEnqueueAUBFixture::SetUp();
    }

    void TearDown() override {
        CommandEnqueueAUBFixture::TearDown();
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
    DebugManager.flags.DisableZeroCopyForBuffers.set(true);

    std::unique_ptr<uint8_t[]> bufferContent(new uint8_t[testDataSize]);
    std::unique_ptr<uint8_t[]> validContent(new uint8_t[testDataSize]);
    std::unique_ptr<uint8_t[]> invalidContent1(new uint8_t[testDataSize]);
    std::unique_ptr<uint8_t[]> invalidContent2(new uint8_t[testDataSize]);

    auto pTestItem = reinterpret_cast<uint8_t *>(&testItem);
    for (size_t offset = 0; offset < testDataSize; offset += testItemSize) {
        for (size_t itemOffset = 0; itemOffset < testItemSize; itemOffset++) {
            bufferContent.get()[offset + itemOffset] = pTestItem[itemOffset];
            validContent.get()[offset + itemOffset] = pTestItem[itemOffset];
            invalidContent1.get()[offset + itemOffset] = pTestItem[itemOffset];
            invalidContent2.get()[offset + itemOffset] = pTestItem[itemOffset];
        }
    }

    // set last item for invalid contents
    auto pTestItemWrong1 = reinterpret_cast<uint8_t *>(&testItemWrong1);
    auto pTestItemWrong2 = reinterpret_cast<uint8_t *>(&testItemWrong2);
    size_t offset = testDataSize - testItemSize;
    for (size_t itemOffset = 0; itemOffset < testItemSize; itemOffset++) {
        invalidContent1.get()[offset + itemOffset] = pTestItemWrong1[itemOffset];
        invalidContent2.get()[offset + itemOffset] = pTestItemWrong2[itemOffset];
    }

    MockContext context(platform()->clDeviceMap[&this->pCmdQ->getDevice()]);
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

INSTANTIATE_TEST_CASE_P(VerifyMemoryBuffer,
                        VerifyMemoryBufferHw,
                        ::testing::Combine(
                            ::testing::ValuesIn(testDataSizeTable),
                            ::testing::ValuesIn(testFlagsTable)));
