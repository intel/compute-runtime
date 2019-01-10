/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/tbx_command_stream_receiver_hw.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"

using namespace NEO;

using Gen11TbxCommandStreamReceiverTests = Test<DeviceFixture>;

GEN11TEST_F(Gen11TbxCommandStreamReceiverTests, whenAskedForPollForCompletionParametersThenReturnCorrectValues) {
    class MyMockTbxHw : public TbxCommandStreamReceiverHw<FamilyType> {
      public:
        MyMockTbxHw(ExecutionEnvironment &executionEnvironment)
            : TbxCommandStreamReceiverHw<FamilyType>(executionEnvironment) {}
        using TbxCommandStreamReceiverHw<FamilyType>::getpollNotEqualValueForPollForCompletion;
        using TbxCommandStreamReceiverHw<FamilyType>::getMaskAndValueForPollForCompletion;
    };

    MyMockTbxHw myMockTbxHw(*pDevice->executionEnvironment);
    EXPECT_EQ(0x80u, myMockTbxHw.getMaskAndValueForPollForCompletion());
    EXPECT_TRUE(myMockTbxHw.getpollNotEqualValueForPollForCompletion());
}
