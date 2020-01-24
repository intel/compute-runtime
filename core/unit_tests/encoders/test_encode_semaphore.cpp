/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/ptr_math.h"
#include "core/unit_tests/fixtures/command_container_fixture.h"
#include "unit_tests/gen_common/gen_cmd_parse.h"

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
