/*
 * Copyright (c) 2017, Intel Corporation
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

#include "runtime/helpers/dirty_state_helpers.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/command_stream/linear_stream.h"
#include "gtest/gtest.h"
#include <memory>

namespace OCLRT {

struct HeapDirtyStateTests : ::testing::Test {
    struct MockHeapDirtyState : public HeapDirtyState {
        using HeapDirtyState::address;
        using HeapDirtyState::size;
    };

    void SetUp() override {
        stream.reset(new LinearStream(buffer, bufferSize));
        ASSERT_EQ(buffer, stream->getBase());
        ASSERT_EQ(bufferSize, stream->getMaxAvailableSpace());
    }

    void *buffer = (void *)123;
    size_t bufferSize = 456;
    std::unique_ptr<LinearStream> stream;
    MockHeapDirtyState mockHeapDirtyState;
};

TEST_F(HeapDirtyStateTests, initialValues) {
    EXPECT_EQ(0u, mockHeapDirtyState.size);
    EXPECT_EQ(nullptr, mockHeapDirtyState.address);
}

TEST_F(HeapDirtyStateTests, givenInitializedObjectWhenUpdatedMultipleTimesThenSetValuesAndReturnDirtyOnce) {
    EXPECT_TRUE(mockHeapDirtyState.updateAndCheck(stream.get()));

    EXPECT_EQ(bufferSize, mockHeapDirtyState.size);
    EXPECT_EQ(buffer, mockHeapDirtyState.address);

    EXPECT_FALSE(mockHeapDirtyState.updateAndCheck(stream.get()));

    EXPECT_EQ(bufferSize, mockHeapDirtyState.size);
    EXPECT_EQ(buffer, mockHeapDirtyState.address);
}

TEST_F(HeapDirtyStateTests, givenNonDirtyObjectWhenAddressChangedThenReturnDirty) {
    EXPECT_TRUE(mockHeapDirtyState.updateAndCheck(stream.get()));

    auto newBuffer = ptrOffset(buffer, 1);
    stream->replaceBuffer(newBuffer, bufferSize);

    EXPECT_TRUE(mockHeapDirtyState.updateAndCheck(stream.get()));

    EXPECT_EQ(bufferSize, mockHeapDirtyState.size);
    EXPECT_EQ(newBuffer, mockHeapDirtyState.address);
}

TEST_F(HeapDirtyStateTests, givenNonDirtyObjectWhenSizeChangedThenReturnDirty) {
    EXPECT_TRUE(mockHeapDirtyState.updateAndCheck(stream.get()));

    auto newBufferSize = bufferSize + 1;
    stream->replaceBuffer(stream->getBase(), newBufferSize);

    EXPECT_TRUE(mockHeapDirtyState.updateAndCheck(stream.get()));

    EXPECT_EQ(newBufferSize, mockHeapDirtyState.size);
    EXPECT_EQ(buffer, mockHeapDirtyState.address);
}

TEST_F(HeapDirtyStateTests, givenNonDirtyObjectWhenSizeAndBufferChangedThenReturnDirty) {
    EXPECT_TRUE(mockHeapDirtyState.updateAndCheck(stream.get()));

    auto newBuffer = ptrOffset(buffer, 1);
    auto newBufferSize = bufferSize + 1;
    stream->replaceBuffer(newBuffer, newBufferSize);

    EXPECT_TRUE(mockHeapDirtyState.updateAndCheck(stream.get()));

    EXPECT_EQ(newBufferSize, mockHeapDirtyState.size);
    EXPECT_EQ(newBuffer, mockHeapDirtyState.address);
}

} // namespace OCLRT
