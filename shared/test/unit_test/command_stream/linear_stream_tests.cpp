/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/fixtures/linear_stream_fixture.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

using namespace NEO;

TEST(LinearStreamCtorTest, WhenConstructingLinearStreamThenInitialValuesValuesAreSet) {
    LinearStream linearStream;
    EXPECT_EQ(nullptr, linearStream.getCpuBase());
    EXPECT_EQ(0u, linearStream.getMaxAvailableSpace());
}

TEST(LinearStreamCtorTest, whenProvidedAllArgumentsThenExpectSameValuesSet) {
    GraphicsAllocation *gfxAllocation = reinterpret_cast<GraphicsAllocation *>(0x1234);
    void *buffer = reinterpret_cast<void *>(0x2000);
    size_t bufferSize = 0x1000u;
    LinearStream linearStream(gfxAllocation, buffer, bufferSize);
    EXPECT_EQ(buffer, linearStream.getCpuBase());
    EXPECT_EQ(bufferSize, linearStream.getMaxAvailableSpace());
    EXPECT_EQ(gfxAllocation, linearStream.getGraphicsAllocation());
}

TEST_F(LinearStreamTest, GivenSizeZeroWhenGettingSpaceUsedThenNonNullPointerIsReturned) {
    EXPECT_NE(nullptr, linearStream.getSpace(0));
}

TEST_F(LinearStreamTest, GivenSizeUint32WhenGettingSpaceUsedThenNonNullPointerIsReturned) {
    EXPECT_NE(nullptr, linearStream.getSpace(sizeof(uint32_t)));
}

TEST_F(LinearStreamTest, GivenNullBufferWhenGettingSpaceThenAssert) {
    linearStream.replaceBuffer(nullptr, 100);
    EXPECT_THROW(linearStream.getSpace(1), std::exception);
}

TEST_F(LinearStreamTest, GivenBadBufferPtrWhenGettingSpaceThenAssert) {
    int64_t ptr = -1;
    linearStream.replaceBuffer(reinterpret_cast<void *>(ptr), 100);
    EXPECT_THROW(linearStream.getSpace(1), std::exception);
}

TEST_F(LinearStreamTest, WhenAllocatingMultipleTimesThenPointersIncrementedCorrectly) {
    size_t allocSize = 1;
    auto ptr1 = linearStream.getSpace(allocSize);
    ASSERT_NE(nullptr, ptr1);

    auto ptr2 = linearStream.getSpace(2);
    ASSERT_NE(nullptr, ptr2);

    EXPECT_EQ(allocSize, (uintptr_t)ptr2 - (uintptr_t)ptr1);
}

TEST_F(LinearStreamTest, WhenGettingSpaceThenPointerIsWriteable) {
    uint32_t cmd = 0xbaddf00d;
    auto pCmd = linearStream.getSpace(sizeof(cmd));
    ASSERT_NE(nullptr, pCmd);
    *(uint32_t *)pCmd = cmd;
}

TEST_F(LinearStreamTest, WhenRequestingMutltipleAllocationsThenDifferentPointersAreReturnedForEachRequest) {
    auto pCmd = linearStream.getSpace(sizeof(uint32_t));
    ASSERT_NE(nullptr, pCmd);
    auto pCmd2 = linearStream.getSpace(sizeof(uint32_t));
    ASSERT_NE(pCmd2, pCmd);
}

TEST_F(LinearStreamTest, GivenNoAllocationsWhenGettingSpaceThenAvailableSpaceIsEqualMaximumSpace) {
    ASSERT_EQ(linearStream.getMaxAvailableSpace(), linearStream.getAvailableSpace());
}

TEST_F(LinearStreamTest, GivenNoAllocationsWhenGettingSpaceThenAvailableSpaceIsGreaterThanZero) {
    EXPECT_NE(0u, linearStream.getAvailableSpace());
}

TEST_F(LinearStreamTest, GivenAllocationWhenGettingSpaceThenAvailableSpaceIsReduced) {
    auto originalAvailable = linearStream.getAvailableSpace();
    linearStream.getSpace(sizeof(uint32_t));

    EXPECT_LT(linearStream.getAvailableSpace(), originalAvailable);
}

TEST_F(LinearStreamTest, GivenOneAllocationsWhenGettingSpaceThenSpaceUsedIsEqualToAllocationSize) {
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

TEST_F(LinearStreamTest, WhenReplacingBufferThenAvailableSizeIsEqualToBufferSizeAndAllSpaceIsAvailable) {
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
    MockGraphicsAllocation newGraphicsAllocation(address, 4096);
    EXPECT_NE(&newGraphicsAllocation, graphicsAllocation);
    linearStream.replaceGraphicsAllocation(&newGraphicsAllocation);
    EXPECT_EQ(&newGraphicsAllocation, linearStream.getGraphicsAllocation());
}

class MyLinearStreamMock : public LinearStream {
  public:
    using LinearStream::sizeUsed;
};

TEST_F(LinearStreamTest, givenLinearStreamWithoutCmdContainerWhenOneByteLeftInStreamThenGetSpaceDontThrowAbort) {
    reinterpret_cast<MyLinearStreamMock *>(&linearStream)->sizeUsed = linearStream.getMaxAvailableSpace() - 1;
    EXPECT_NO_THROW(linearStream.getSpace(1));
}
using CommandContainerLinearStreamTest = Test<DeviceFixture>;
TEST_F(CommandContainerLinearStreamTest, givenLinearStreamWithCmdContainerWhenOneByteLeftInStreamThenGetSpaceThrowAbort) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr);
    auto stream = reinterpret_cast<MyLinearStreamMock *>(cmdContainer.getCommandStream());
    stream->sizeUsed = stream->getMaxAvailableSpace() - 1;
    EXPECT_THROW(stream->getSpace(1), std::exception);
}

TEST_F(CommandContainerLinearStreamTest, givenLinearStreamWithCmdContainerWhenThereIsNoSpaceForCommandAndBBEndThenNewCmdBufferAllocated) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr);
    auto &hwInfo = pDevice->getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    auto stream = reinterpret_cast<MyLinearStreamMock *>(cmdContainer.getCommandStream());
    size_t dummyCommandSize = 2;
    stream->sizeUsed = stream->getMaxAvailableSpace() - hwHelper.getBatchBufferEndSize() - (dummyCommandSize - 1);
    EXPECT_EQ(cmdContainer.getCmdBufferAllocations().size(), 1u);
    stream->getSpace(dummyCommandSize);
    EXPECT_EQ(cmdContainer.getCmdBufferAllocations().size(), 2u);
}

TEST_F(CommandContainerLinearStreamTest, givenLinearStreamWithCmdContainerWhenThereIsNoSpaceForCommandAndBBEndThenLinearStreamHasNewAllocation) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr);
    auto &hwInfo = pDevice->getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    auto stream = reinterpret_cast<MyLinearStreamMock *>(cmdContainer.getCommandStream());
    size_t dummyCommandSize = 2;
    stream->sizeUsed = stream->getMaxAvailableSpace() - hwHelper.getBatchBufferEndSize() - (dummyCommandSize - 1);
    auto oldBuffer = stream->getCpuBase();
    stream->getSpace(dummyCommandSize);
    auto newBuffer = stream->getCpuBase();
    EXPECT_NE(newBuffer, oldBuffer);
}

TEST_F(CommandContainerLinearStreamTest, givenLinearStreamWithCmdContainerWhenThereIsNoSpaceForCommandAndBBEndThenGetSpaceReturnPtrFromNewAllocation) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr);
    auto &hwInfo = pDevice->getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    auto stream = reinterpret_cast<MyLinearStreamMock *>(cmdContainer.getCommandStream());
    size_t dummyCommandSize = 2;
    stream->sizeUsed = stream->getMaxAvailableSpace() - hwHelper.getBatchBufferEndSize() - (dummyCommandSize - 1);
    auto ptr = stream->getSpace(dummyCommandSize);
    auto buffer = stream->getCpuBase();
    EXPECT_EQ(buffer, ptr);
}

TEST_F(CommandContainerLinearStreamTest, givenLinearStreamWithCmdContainerWhenThereIsSpaceForCommandAndBBEndThenNewCmdBufferIsNotAllocated) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr);
    auto &hwInfo = pDevice->getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    auto stream = reinterpret_cast<MyLinearStreamMock *>(cmdContainer.getCommandStream());
    size_t dummyCommandSize = 2;
    stream->sizeUsed = stream->getMaxAvailableSpace() - hwHelper.getBatchBufferEndSize() - (dummyCommandSize);
    EXPECT_EQ(cmdContainer.getCmdBufferAllocations().size(), 1u);
    stream->getSpace(dummyCommandSize);
    EXPECT_EQ(cmdContainer.getCmdBufferAllocations().size(), 1u);
}

TEST_F(CommandContainerLinearStreamTest, givenLinearStreamWithCmdContainerWhenThereIsNoSpaceForCommandAndBBEndThenBBEndAddedAtEndOfStream) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr);
    auto &hwInfo = pDevice->getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    auto stream = reinterpret_cast<MyLinearStreamMock *>(cmdContainer.getCommandStream());
    size_t dummyCommandSize = 2;
    stream->sizeUsed = stream->getMaxAvailableSpace() - hwHelper.getBatchBufferEndSize() - (dummyCommandSize - 1);
    auto ptr = stream->getSpace(0u);
    stream->getSpace(dummyCommandSize);
    EXPECT_EQ(memcmp(ptr, hwHelper.getBatchBufferEndReference(), hwHelper.getBatchBufferEndSize()), 0);
}