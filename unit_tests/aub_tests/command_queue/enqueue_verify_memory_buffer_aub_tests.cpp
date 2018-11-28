/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/api/api.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/mem_obj/buffer.h"
#include "unit_tests/aub_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/unit_test_helper.h"

#include "test.h"

using namespace OCLRT;

struct TestDataItem {
    uint8_t *item;
    uint8_t *itemWrongSmaller;
    uint8_t *itemWrongGreater;
    size_t size;
};

struct VerifyMemoryBufferHw
    : public CommandEnqueueAUBFixture,
      public ::testing::TestWithParam<std::tuple<TestDataItem, size_t, uint64_t>> {

    void SetUp() override {
        CommandEnqueueAUBFixture::SetUp();
    }

    void TearDown() override {
        CommandEnqueueAUBFixture::TearDown();
    }
};

cl_int testClInt = 5;
cl_int testClIntSmaller = 4;
cl_int testClIntGreater = 6;
cl_float testClFloat = 20.0;
cl_float testClFloatSmaller = 19.5;
cl_float testClFloatGreater = 20.5;

TestDataItem testDataItemTable[] = {
    {reinterpret_cast<uint8_t *>(&testClInt),
     reinterpret_cast<uint8_t *>(&testClIntSmaller),
     reinterpret_cast<uint8_t *>(&testClIntGreater),
     sizeof(testClInt)},
    {reinterpret_cast<uint8_t *>(&testClFloat),
     reinterpret_cast<uint8_t *>(&testClFloatSmaller),
     reinterpret_cast<uint8_t *>(&testClFloatGreater),
     sizeof(testClFloat)}};

size_t testDataSizeTable[] = {
    16,
    MemoryConstants::cacheLineSize};

cl_mem_flags testFlagsTable[] = {
    0,
    CL_MEM_COPY_HOST_PTR};

HWTEST_P(VerifyMemoryBufferHw, givenDifferentBuffersWhenValidatingMemoryThenSuccessIsReturned) {
    const auto &testDataItem = std::get<0>(GetParam());
    const auto &testDataSize = std::get<1>(GetParam());
    EXPECT_FALSE(testDataSize < testDataItem.size);
    const auto &flags = std::get<2>(GetParam());
    const auto usesHostPointer = ((flags & CL_MEM_USE_HOST_PTR) ||
                                  (flags & CL_MEM_COPY_HOST_PTR));

    DebugManagerStateRestore restore;
    DebugManager.flags.DisableZeroCopyForBuffers.set(true);

    // for each buffer size check also one object smaller and one object greater
    for (auto dataSize = testDataSize - testDataItem.size;
         dataSize <= testDataSize + testDataItem.size;
         dataSize += testDataItem.size) {

        std::unique_ptr<uint8_t[]> bufferContent(new uint8_t[dataSize]);
        std::unique_ptr<uint8_t[]> validContent(new uint8_t[dataSize]);
        std::unique_ptr<uint8_t[]> invalidContent1(new uint8_t[dataSize]);
        std::unique_ptr<uint8_t[]> invalidContent2(new uint8_t[dataSize]);

        for (size_t offset = 0; offset < dataSize; offset += testDataItem.size) {
            for (size_t itemOffset = 0; itemOffset < testDataItem.size; itemOffset++) {
                bufferContent.get()[offset + itemOffset] = testDataItem.item[itemOffset];
                validContent.get()[offset + itemOffset] = testDataItem.item[itemOffset];
                invalidContent1.get()[offset + itemOffset] = testDataItem.item[itemOffset];
                invalidContent2.get()[offset + itemOffset] = testDataItem.item[itemOffset];
            }
        }

        // set last item for invalid contents
        size_t offset = dataSize - testDataItem.size;
        for (size_t itemOffset = 0; itemOffset < testDataItem.size; itemOffset++) {
            invalidContent1.get()[offset + itemOffset] = testDataItem.itemWrongSmaller[itemOffset];
            invalidContent2.get()[offset + itemOffset] = testDataItem.itemWrongGreater[itemOffset];
        }

        MockContext context(&this->pCmdQ->getDevice());
        cl_int retVal = CL_INVALID_VALUE;

        std::unique_ptr<Buffer> buffer(Buffer::create(
            &context,
            flags,
            dataSize,
            (usesHostPointer ? bufferContent.get() : nullptr),
            retVal));
        EXPECT_NE(nullptr, buffer);

        if (!usesHostPointer) {
            retVal = pCmdQ->enqueueFillBuffer(
                buffer.get(),
                testDataItem.item,
                testDataItem.size,
                0,
                dataSize,
                0,
                nullptr,
                nullptr);
            EXPECT_EQ(CL_SUCCESS, retVal);
        }

        auto mappedAddress = clEnqueueMapBuffer(pCmdQ, buffer.get(), CL_TRUE, CL_MAP_READ, 0, dataSize, 0, nullptr, nullptr, nullptr);

        retVal = clEnqueueVerifyMemory(pCmdQ, mappedAddress, validContent.get(), dataSize, CL_MEM_COMPARE_EQUAL);
        EXPECT_EQ(CL_SUCCESS, retVal);

        if (UnitTestHelper<FamilyType>::isExpectMemoryNotEqualSupported()) {
            retVal = clEnqueueVerifyMemory(pCmdQ, mappedAddress, invalidContent1.get(), dataSize, CL_MEM_COMPARE_NOT_EQUAL);
            EXPECT_EQ(CL_SUCCESS, retVal);
            retVal = clEnqueueVerifyMemory(pCmdQ, mappedAddress, invalidContent2.get(), dataSize, CL_MEM_COMPARE_NOT_EQUAL);
            EXPECT_EQ(CL_SUCCESS, retVal);
        }
    }
}

INSTANTIATE_TEST_CASE_P(VerifyMemoryBuffer,
                        VerifyMemoryBufferHw,
                        ::testing::Combine(
                            ::testing::ValuesIn(testDataItemTable),
                            ::testing::ValuesIn(testDataSizeTable),
                            ::testing::ValuesIn(testFlagsTable)));
