/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"

using namespace NEO;

using CommandEncodeSemaphore = Test<CommandEncodeStatesFixture>;

HWTEST_F(CommandEncodeSemaphore, WhenProgrammingThenMiSemaphoreWaitIsUsed) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    MI_SEMAPHORE_WAIT miSemaphore1;

    EncodeSemaphore<FamilyType>::programMiSemaphoreWait(&miSemaphore1,
                                                        0x123400,
                                                        4,
                                                        MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                                                        false,
                                                        true,
                                                        false,
                                                        false,
                                                        false);

    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, miSemaphore1.getCompareOperation());
    EXPECT_EQ(4u, miSemaphore1.getSemaphoreDataDword());
    EXPECT_EQ(0x123400u, miSemaphore1.getSemaphoreGraphicsAddress());
    EXPECT_EQ(MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE, miSemaphore1.getWaitMode());

    MI_SEMAPHORE_WAIT miSemaphore2;
    EncodeSemaphore<FamilyType>::programMiSemaphoreWait(&miSemaphore2,
                                                        0x123400,
                                                        4,
                                                        MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                                                        false,
                                                        false,
                                                        false,
                                                        false,
                                                        false);
    EXPECT_EQ(MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_SIGNAL_MODE, miSemaphore2.getWaitMode());
}

HWTEST_F(CommandEncodeSemaphore, whenAddingMiSemaphoreCommandThenExpectCompareFieldsAreSetCorrectly) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename FamilyType::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;
    using WAIT_MODE = typename FamilyType::MI_SEMAPHORE_WAIT::WAIT_MODE;

    auto buffer = std::make_unique<uint8_t[]>(128);
    LinearStream stream(buffer.get(), 128);
    COMPARE_OPERATION compareMode = COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD;

    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(stream,
                                                           0xFF00FF000u,
                                                           5u,
                                                           compareMode, false, false, false, false, nullptr);

    EXPECT_EQ(NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait(), stream.getUsed());

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(stream);
    MI_SEMAPHORE_WAIT *miSemaphore = hwParse.getCommand<MI_SEMAPHORE_WAIT>();
    ASSERT_NE(nullptr, miSemaphore);

    EXPECT_EQ(compareMode, miSemaphore->getCompareOperation());
    EXPECT_EQ(5u, miSemaphore->getSemaphoreDataDword());
    EXPECT_EQ(0xFF00FF000u, miSemaphore->getSemaphoreGraphicsAddress());
    EXPECT_EQ(WAIT_MODE::WAIT_MODE_POLLING_MODE, miSemaphore->getWaitMode());
}

HWTEST2_F(CommandEncodeSemaphore, givenIndirectModeSetWhenProgrammingSemaphoreThenSetIndirectBit, IsAtLeastXeCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename FamilyType::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    auto buffer = std::make_unique<uint8_t[]>(128);
    LinearStream stream(buffer.get(), 128);
    COMPARE_OPERATION compareMode = COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD;

    void *outSemWait = nullptr;
    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(stream,
                                                           0xFF00FF000u,
                                                           5u,
                                                           compareMode, false, false, true, false, &outSemWait);

    EXPECT_EQ(NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait(), stream.getUsed());

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(stream);
    MI_SEMAPHORE_WAIT *miSemaphore = hwParse.getCommand<MI_SEMAPHORE_WAIT>();
    ASSERT_NE(nullptr, miSemaphore);

    auto outSemWaitCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(outSemWait);
    ASSERT_NE(nullptr, outSemWaitCmd);
    EXPECT_EQ(miSemaphore, outSemWaitCmd);

    EXPECT_TRUE(miSemaphore->getIndirectSemaphoreDataDword());
}

HWTEST_F(CommandEncodeSemaphore, whenGettingMiSemaphoreCommandSizeThenExpectSingleMiSemaphoreCommandSize) {
    size_t expectedSize = NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();
    size_t actualSize = EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();
    EXPECT_EQ(expectedSize, actualSize);
}
