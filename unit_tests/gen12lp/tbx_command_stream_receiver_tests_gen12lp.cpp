/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub_mem_dump/page_table_entry_bits.h"
#include "runtime/command_stream/tbx_command_stream_receiver_hw.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"

using namespace NEO;

using Gen12LPTbxCommandStreamReceiverTests = Test<DeviceFixture>;

GEN12LPTEST_F(Gen12LPTbxCommandStreamReceiverTests, givenNullPtrGraphicsAlloctionWhenGetPPGTTAdditionalBitsIsCalledThenAppropriateValueIsReturned) {
    auto tbxCsr = std::make_unique<TbxCommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex());
    GraphicsAllocation *allocation = nullptr;
    auto bits = tbxCsr->getPPGTTAdditionalBits(allocation);
    constexpr uint64_t expectedBits = BIT(PageTableEntry::presentBit) | BIT(PageTableEntry::writableBit);

    EXPECT_EQ(expectedBits, bits);
}

GEN12LPTEST_F(Gen12LPTbxCommandStreamReceiverTests, givenGraphicsAlloctionWWhenGetPPGTTAdditionalBitsIsCalledThenAppropriateValueIsReturned) {
    auto tbxCsr = std::make_unique<TbxCommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex());
    MockGraphicsAllocation allocation(nullptr, 0);
    auto bits = tbxCsr->getPPGTTAdditionalBits(&allocation);
    constexpr uint64_t expectedBits = BIT(PageTableEntry::presentBit) | BIT(PageTableEntry::writableBit);

    EXPECT_EQ(expectedBits, bits);
}

GEN12LPTEST_F(Gen12LPTbxCommandStreamReceiverTests, whenAskedForPollForCompletionParametersThenReturnCorrectValues) {
    class MyMockTbxHw : public TbxCommandStreamReceiverHw<FamilyType> {
      public:
        MyMockTbxHw(ExecutionEnvironment &executionEnvironment)
            : TbxCommandStreamReceiverHw<FamilyType>(executionEnvironment, 0) {}
        using TbxCommandStreamReceiverHw<FamilyType>::getpollNotEqualValueForPollForCompletion;
        using TbxCommandStreamReceiverHw<FamilyType>::getMaskAndValueForPollForCompletion;
    };

    MyMockTbxHw myMockTbxHw(*pDevice->executionEnvironment);
    EXPECT_EQ(0x80u, myMockTbxHw.getMaskAndValueForPollForCompletion());
    EXPECT_TRUE(myMockTbxHw.getpollNotEqualValueForPollForCompletion());
}
