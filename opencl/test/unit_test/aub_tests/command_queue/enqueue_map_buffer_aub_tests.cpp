/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/aub_tests/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

struct AUBMapBuffer
    : public CommandEnqueueAUBFixture,
      public ::testing::Test {

    void SetUp() override {
        CommandEnqueueAUBFixture::SetUp();
    }

    void TearDown() override {
        CommandEnqueueAUBFixture::TearDown();
    }
};

HWTEST_F(AUBMapBuffer, WhenMappingAndUnmappingThenExpectationsAreMet) {
    MockContext context(this->pCmdQ->getDevice().getSpecializedDevice<ClDevice>());
    auto retVal = CL_INVALID_VALUE;
    size_t bufferSize = 10;

    std::unique_ptr<Buffer> buffer(Buffer::create(
        &context,
        CL_MEM_READ_WRITE,
        bufferSize,
        nullptr,
        retVal));
    ASSERT_NE(nullptr, buffer);

    uint8_t pattern[] = {0xFF};
    size_t patternSize = sizeof(pattern);

    retVal = pCmdQ->enqueueFillBuffer(
        buffer.get(),
        pattern,
        patternSize,
        0,
        bufferSize,
        0,
        nullptr,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto mappedPtr = pCmdQ->enqueueMapBuffer(buffer.get(), CL_TRUE, CL_MAP_WRITE | CL_MAP_READ,
                                             0, bufferSize, 0, nullptr, nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    // write to mapped ptr
    auto mappedPtrStart = static_cast<uint8_t *>(mappedPtr);
    for (uint32_t i = 0; i < bufferSize; i++) {
        *(mappedPtrStart + i) = i;
    }

    pCmdQ->enqueueUnmapMemObject(buffer.get(), mappedPtr, 0, nullptr, nullptr);

    // verify unmap
    std::unique_ptr<uint8_t[]> readMemory(new uint8_t[bufferSize]);
    buffer->forceDisallowCPUCopy = true;

    retVal = pCmdQ->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, bufferSize, readMemory.get(), nullptr, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = pCmdQ->flush();
    ASSERT_EQ(CL_SUCCESS, retVal);

    for (size_t i = 0; i < bufferSize; i++) {
        AUBCommandStreamFixture::expectMemory<FamilyType>(&readMemory[i], &i, sizeof(uint8_t));
    }
}
