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

#include "runtime/command_stream/linear_stream.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "unit_tests/command_stream/linear_stream_fixture.h"

using namespace OCLRT;

TEST(LinearStreamCtorTest, establishInitialValues) {
    LinearStream linearStream;
    EXPECT_EQ(nullptr, linearStream.getCpuBase());
    EXPECT_EQ(0u, linearStream.getMaxAvailableSpace());
}

TEST_F(LinearStreamTest, getSpaceTestSizeZero) {
    EXPECT_NE(nullptr, linearStream.getSpace(0));
}

TEST_F(LinearStreamTest, getSpaceTestSizeNonZero) {
    EXPECT_NE(nullptr, linearStream.getSpace(sizeof(uint32_t)));
}

TEST_F(LinearStreamTest, getSpaceIncrementsPointerCorrectly) {
    size_t allocSize = 1;
    auto ptr1 = linearStream.getSpace(allocSize);
    ASSERT_NE(nullptr, ptr1);

    auto ptr2 = linearStream.getSpace(2);
    ASSERT_NE(nullptr, ptr2);

    EXPECT_EQ(allocSize, (uintptr_t)ptr2 - (uintptr_t)ptr1);
}

TEST_F(LinearStreamTest, getSpaceTestReturnsWritablePointer) {
    uint32_t cmd = 0xbaddf00d;
    auto pCmd = linearStream.getSpace(sizeof(cmd));
    ASSERT_NE(nullptr, pCmd);
    *(uint32_t *)pCmd = cmd;
}

TEST_F(LinearStreamTest, getSpaceReturnsDifferentPointersForEachRequest) {
    auto pCmd = linearStream.getSpace(sizeof(uint32_t));
    ASSERT_NE(nullptr, pCmd);
    auto pCmd2 = linearStream.getSpace(sizeof(uint32_t));
    ASSERT_NE(pCmd2, pCmd);
}

TEST_F(LinearStreamTest, getMaxAvailableSpace) {
    ASSERT_EQ(linearStream.getMaxAvailableSpace(), linearStream.getAvailableSpace());
}

TEST_F(LinearStreamTest, getAvailableSpaceShouldBeNonZeroAfterInit) {
    EXPECT_NE(0u, linearStream.getAvailableSpace());
}

TEST_F(LinearStreamTest, getSpaceReducesAvailableSpace) {
    auto originalAvailable = linearStream.getAvailableSpace();
    linearStream.getSpace(sizeof(uint32_t));

    EXPECT_LT(linearStream.getAvailableSpace(), originalAvailable);
}

TEST_F(LinearStreamTest, testGetUsed) {
    size_t sizeToAllocate = 2 * sizeof(uint32_t);
    ASSERT_NE(nullptr, linearStream.getSpace(sizeToAllocate));

    EXPECT_EQ(sizeToAllocate, linearStream.getUsed());
}

TEST_F(LinearStreamTest, givenLinearStreamWhenGetCpuBaseIsCalledThenCpuBaseAddressIsReturned) {
    ASSERT_EQ(pCmdBuffer, linearStream.getCpuBase());
}

TEST_F(LinearStreamTest, givenNotEnoughSpaceWhenGetSpaceIsCalledThenThrowException) {
    linearStream.getSpace(linearStream.getMaxAvailableSpace());
    EXPECT_THROW(linearStream.getSpace(1), std::exception);
}

TEST_F(LinearStreamTest, testReplaceBuffer) {
    char buffer[256];
    linearStream.replaceBuffer(buffer, sizeof(buffer));
    EXPECT_EQ(buffer, linearStream.getCpuBase());
    EXPECT_EQ(sizeof(buffer), linearStream.getAvailableSpace());
    EXPECT_EQ(0u, linearStream.getUsed());
}

TEST_F(LinearStreamTest, givenNewGraphicsAllocationWhenReplaceIsCalledThenLinearStreamContainsNewGraphicsAllocation) {
    auto graphicsAllocation = linearStream.getGraphicsAllocation();
    EXPECT_NE(nullptr, graphicsAllocation);
    auto address = (void *)0x100000;
    GraphicsAllocation newGraphicsAllocation(address, 4096);
    EXPECT_NE(&newGraphicsAllocation, graphicsAllocation);
    linearStream.replaceGraphicsAllocation(&newGraphicsAllocation);
    EXPECT_EQ(&newGraphicsAllocation, linearStream.getGraphicsAllocation());
}