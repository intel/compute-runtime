/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/dispatchers/dispatcher.h"
#include "shared/test/unit_test/cmd_parse/hw_parse.h"
#include "shared/test/unit_test/direct_submission/dispatchers/dispatcher_fixture.h"

#include "test.h"

using namespace NEO;

using DispatcherTest = Test<DispatcherFixture>;

HWTEST_F(DispatcherTest, givenBaseDispatcherWhenAskingForStartCmdSizeThenReturnBbStartCmdSize) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    EXPECT_EQ(sizeof(MI_BATCH_BUFFER_START), Dispatcher<FamilyType>::getSizeStartCommandBuffer());
}

HWTEST_F(DispatcherTest, givenBaseDispatcherWhenAddingStartCmdThenExpectBbStart) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    uint64_t gpuVa = 0xFF0000;
    Dispatcher<FamilyType>::dispatchStartCommandBuffer(cmdBuffer, gpuVa);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(cmdBuffer);
    MI_BATCH_BUFFER_START *start = hwParse.getCommand<MI_BATCH_BUFFER_START>();
    ASSERT_NE(nullptr, start);
    EXPECT_EQ(gpuVa, start->getBatchBufferStartAddressGraphicsaddress472());
}

HWTEST_F(DispatcherTest, givenBaseDispatcherWhenAskingForStopCmdSizeThenReturnBbStopCmdSize) {
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    EXPECT_EQ(sizeof(MI_BATCH_BUFFER_END), Dispatcher<FamilyType>::getSizeStopCommandBuffer());
}

HWTEST_F(DispatcherTest, givenBaseDispatcherWhenAddingStopCmdThenExpectBbStop) {
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    Dispatcher<FamilyType>::dispatchStopCommandBuffer(cmdBuffer);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(cmdBuffer);
    MI_BATCH_BUFFER_END *stop = hwParse.getCommand<MI_BATCH_BUFFER_END>();
    ASSERT_NE(nullptr, stop);
}

HWTEST_F(DispatcherTest, givenBaseDispatcherWhenAskingForStoreCmdSizeThenReturnStoreDataImmCmdSize) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    EXPECT_EQ(sizeof(MI_STORE_DATA_IMM), Dispatcher<FamilyType>::getSizeStoreDwordCommand());
}

HWTEST_F(DispatcherTest, givenBaseDispatcherWhenAddingStoreCmdThenExpectStoreDataImm) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    uint64_t gpuVa = 0xFF0000;
    uint32_t val = 123;
    Dispatcher<FamilyType>::dispatchStoreDwordCommand(cmdBuffer, gpuVa, val);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(cmdBuffer);
    MI_STORE_DATA_IMM *sdi = hwParse.getCommand<MI_STORE_DATA_IMM>();
    ASSERT_NE(nullptr, sdi);
    EXPECT_EQ(gpuVa, sdi->getAddress());
    EXPECT_EQ(val, sdi->getDataDword0());
}
