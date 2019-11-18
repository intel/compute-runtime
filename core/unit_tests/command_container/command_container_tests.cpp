/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/command_container/cmdcontainer.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"

using namespace NEO;

class CommandContainerTest : public DeviceFixture,
                             public ::testing::Test {

  public:
    void SetUp() override {
        ::testing::Test::SetUp();
        DeviceFixture::SetUp();
    }
    void TearDown() override {
        DeviceFixture::TearDown();
        ::testing::Test::TearDown();
    }
};

TEST_F(CommandContainerTest, givenCommandContainerWhenInitializeThenEverythingIsInitialized) {
    CommandContainer cmdContainer;
    auto status = cmdContainer.initialize(pDevice);
    EXPECT_TRUE(status);

    EXPECT_EQ(pDevice, cmdContainer.getDevice());
    EXPECT_NE(cmdContainer.getHeapHelper(), nullptr);
    EXPECT_NE(cmdContainer.getCmdBufferAllocation(), nullptr);
    EXPECT_NE(cmdContainer.getCommandStream(), nullptr);

    for (uint32_t i = 0; i < HeapType::NUM_TYPES; i++) {
        auto indirectHeap = cmdContainer.getIndirectHeap(static_cast<HeapType>(i));
        auto heapAllocation = cmdContainer.getIndirectHeapAllocation(static_cast<HeapType>(i));
        EXPECT_EQ(indirectHeap->getGraphicsAllocation(), heapAllocation);
    }
    EXPECT_EQ(cmdContainer.getInstructionHeapBaseAddress(), pDevice->getMemoryManager()->getInternalHeapBaseAddress(0));
}

TEST_F(CommandContainerTest, givenCommandContainerWhenInitializeWithoutDeviceThenReturnedFalse) {
    CommandContainer cmdContainer;
    auto status = cmdContainer.initialize(nullptr);
    EXPECT_FALSE(status);
}

TEST_F(CommandContainerTest, givenCommandContainerWhenSettingIndirectHeapAllocationThenAllocationIsSet) {
    CommandContainer cmdContainer;
    MockGraphicsAllocation mockAllocation;
    auto heapType = HeapType::DYNAMIC_STATE;
    cmdContainer.setIndirectHeapAllocation(heapType, &mockAllocation);
    EXPECT_EQ(cmdContainer.getIndirectHeapAllocation(heapType), &mockAllocation);
}

TEST_F(CommandContainerTest, givenCommandContainerWhenSettingCommandBufferAllocationThenAllocationIsSet) {
    CommandContainer cmdContainer;
    MockGraphicsAllocation mockAllocation;
    cmdContainer.setCmdBufferAllocation(&mockAllocation);
    EXPECT_EQ(cmdContainer.getCmdBufferAllocation(), &mockAllocation);
}

TEST_F(CommandContainerTest, givenHeapAllocationsWhenDestroyCommandContainerThenHeapAllocationsAreReused) {
    std::unique_ptr<CommandContainer> cmdContainer(new CommandContainer);
    cmdContainer->initialize(pDevice);
    auto heapAllocationsAddress = cmdContainer->getIndirectHeapAllocation(HeapType::DYNAMIC_STATE)->getUnderlyingBuffer();
    cmdContainer.reset(new CommandContainer);
    cmdContainer->initialize(pDevice);
    bool status = false;
    for (uint32_t i = 0; i < HeapType::NUM_TYPES && !status; i++) {
        status = cmdContainer->getIndirectHeapAllocation(static_cast<HeapType>(i))->getUnderlyingBuffer() == heapAllocationsAddress;
    }
    EXPECT_TRUE(status);
}

TEST_F(CommandContainerTest, givenCommandContainerWhenResetTheanStreamsAreNotUsed) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice);
    LinearStream stream;
    uint32_t usedSize = 1;
    cmdContainer.getCommandStream()->getSpace(usedSize);
    EXPECT_EQ(usedSize, cmdContainer.getCommandStream()->getUsed());
    cmdContainer.reset();
    EXPECT_NE(usedSize, cmdContainer.getCommandStream()->getUsed());
    EXPECT_EQ(0u, cmdContainer.getCommandStream()->getUsed());
}

TEST_F(CommandContainerTest, givenCommandContainerWhenWantToAddNullPtrToResidencyContainerThenNothingIsAdded) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice);
    auto size = cmdContainer.getResidencyContainer().size();
    cmdContainer.addToResidencyContainer(nullptr);
    EXPECT_EQ(cmdContainer.getResidencyContainer().size(), size);
}

TEST_F(CommandContainerTest, givenCommandContainerWhenWantToAddAleradyAddedAllocationThenNothingIsAdded) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice);
    MockGraphicsAllocation mockAllocation;

    auto sizeBefore = cmdContainer.getResidencyContainer().size();

    cmdContainer.addToResidencyContainer(&mockAllocation);
    auto sizeAfterFirstAdd = cmdContainer.getResidencyContainer().size();

    EXPECT_NE(sizeBefore, sizeAfterFirstAdd);

    cmdContainer.addToResidencyContainer(&mockAllocation);
    auto sizeAfterSecondAdd = cmdContainer.getResidencyContainer().size();

    EXPECT_EQ(sizeAfterFirstAdd, sizeAfterSecondAdd);
}