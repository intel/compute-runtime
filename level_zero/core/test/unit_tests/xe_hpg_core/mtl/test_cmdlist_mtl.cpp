/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.inl"

namespace L0 {
namespace ult {

using CmdListPipelineSelectStateTestMtl = Test<CmdListPipelineSelectStateFixture>;

HWTEST2_F(CmdListPipelineSelectStateTestMtl,
          givenAppendSystolicKernelToCommandListWhenExecutingCommandListThenPipelineSelectStateIsTrackedCorrectly, IsMTL) {
    testBody<FamilyType>();
}

HWTEST2_F(CmdListPipelineSelectStateTestMtl,
          givenCmdQueueAndImmediateCmdListUseSameCsrWhenAppendingSystolicKernelOnBothRegularFirstThenPipelineSelectStateIsNotChanged, IsMTL) {
    testBodyShareStateRegularImmediate<FamilyType>();
}

HWTEST2_F(CmdListPipelineSelectStateTestMtl,
          givenCmdQueueAndImmediateCmdListUseSameCsrWhenAppendingSystolicKernelOnBothImmediateFirstThenPipelineSelectStateIsNotChanged, IsMTL) {
    testBodyShareStateImmediateRegular<FamilyType>();
}

using CmdListLargeGrfTestMtl = Test<CmdListLargeGrfFixture>;

HWTEST2_F(CmdListLargeGrfTestMtl,
          givenAppendLargeGrfKernelToCommandListWhenExecutingCommandListThenStateComputeModeStateIsTrackedCorrectly, IsMTL) {
    testBody<FamilyType>();
}

} // namespace ult
} // namespace L0
