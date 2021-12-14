/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/page_table_entry_bits.h"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

using Gen12LPTbxCommandStreamReceiverTests = Test<ClDeviceFixture>;

GEN12LPTEST_F(Gen12LPTbxCommandStreamReceiverTests, givenNullPtrGraphicsAlloctionWhenGetPPGTTAdditionalBitsIsCalledThenAppropriateValueIsReturned) {
    auto tbxCsr = std::make_unique<TbxCommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    GraphicsAllocation *allocation = nullptr;
    auto bits = tbxCsr->getPPGTTAdditionalBits(allocation);
    constexpr uint64_t expectedBits = BIT(PageTableEntry::presentBit) | BIT(PageTableEntry::writableBit);

    EXPECT_EQ(expectedBits, bits);
}

GEN12LPTEST_F(Gen12LPTbxCommandStreamReceiverTests, givenGraphicsAlloctionWithLocalMemoryPoolWhenGetPPGTTAdditionalBitsIsCalledThenAppropriateValueIsReturned) {
    auto tbxCsr = std::make_unique<TbxCommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockGraphicsAllocation allocation(nullptr, 0);
    allocation.overrideMemoryPool(MemoryPool::LocalMemory);
    auto bits = tbxCsr->getPPGTTAdditionalBits(&allocation);
    constexpr uint64_t expectedBits = BIT(PageTableEntry::presentBit) | BIT(PageTableEntry::writableBit) | BIT(PageTableEntry::localMemoryBit);

    EXPECT_EQ(expectedBits, bits);
}

GEN12LPTEST_F(Gen12LPTbxCommandStreamReceiverTests, whenAskedForPollForCompletionParametersThenReturnCorrectValues) {
    class MyMockTbxHw : public TbxCommandStreamReceiverHw<FamilyType> {
      public:
        MyMockTbxHw(ExecutionEnvironment &executionEnvironment)
            : TbxCommandStreamReceiverHw<FamilyType>(executionEnvironment, 0, 1) {}
        using TbxCommandStreamReceiverHw<FamilyType>::getpollNotEqualValueForPollForCompletion;
        using TbxCommandStreamReceiverHw<FamilyType>::getMaskAndValueForPollForCompletion;
    };

    MyMockTbxHw myMockTbxHw(*pDevice->executionEnvironment);
    EXPECT_EQ(0x80u, myMockTbxHw.getMaskAndValueForPollForCompletion());
    EXPECT_TRUE(myMockTbxHw.getpollNotEqualValueForPollForCompletion());
}
