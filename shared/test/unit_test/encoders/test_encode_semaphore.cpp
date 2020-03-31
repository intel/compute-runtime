/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/unit_test/cmd_parse/hw_parse.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"

using namespace NEO;

using CommandEncodeSemaphore = Test<CommandEncodeStatesFixture>;

HWTEST_F(CommandEncodeSemaphore, programMiSemaphoreWait) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    MI_SEMAPHORE_WAIT miSemaphore;

    EncodeSempahore<FamilyType>::programMiSemaphoreWait(&miSemaphore,
                                                        0x123400,
                                                        4,
                                                        MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);

    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, miSemaphore.getCompareOperation());
    EXPECT_EQ(4u, miSemaphore.getSemaphoreDataDword());
    EXPECT_EQ(0x123400u, miSemaphore.getSemaphoreGraphicsAddress());
    EXPECT_EQ(MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE, miSemaphore.getWaitMode());
}

HWTEST_F(CommandEncodeSemaphore, whenAddingMiSemaphoreCommandThenExpectCompareFieldsAreSetCorrectly) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename FamilyType::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;
    using WAIT_MODE = typename FamilyType::MI_SEMAPHORE_WAIT::WAIT_MODE;

    std::unique_ptr<uint8_t> buffer(new uint8_t[128]);
    LinearStream stream(buffer.get(), 128);
    COMPARE_OPERATION compareMode = COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD;

    EncodeSempahore<FamilyType>::addMiSemaphoreWaitCommand(stream,
                                                           0xFF00FF000u,
                                                           5u,
                                                           compareMode);

    EXPECT_EQ(sizeof(MI_SEMAPHORE_WAIT), stream.getUsed());

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(stream);
    MI_SEMAPHORE_WAIT *miSemaphore = hwParse.getCommand<MI_SEMAPHORE_WAIT>();
    ASSERT_NE(nullptr, miSemaphore);

    EXPECT_EQ(compareMode, miSemaphore->getCompareOperation());
    EXPECT_EQ(5u, miSemaphore->getSemaphoreDataDword());
    EXPECT_EQ(0xFF00FF000u, miSemaphore->getSemaphoreGraphicsAddress());
    EXPECT_EQ(WAIT_MODE::WAIT_MODE_POLLING_MODE, miSemaphore->getWaitMode());
}

HWTEST_F(CommandEncodeSemaphore, whenGettingMiSemaphoreCommandSizeThenExpectSingleMiSemaphoreCommandSize) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    size_t expectedSize = sizeof(MI_SEMAPHORE_WAIT);
    size_t actualSize = EncodeSempahore<FamilyType>::getSizeMiSemaphoreWait();
    EXPECT_EQ(expectedSize, actualSize);
}
