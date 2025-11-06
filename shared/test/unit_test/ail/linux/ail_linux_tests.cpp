/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {
using AILTests = ::testing::Test;
namespace SysCalls {
extern bool isInvalidAILTest;
}

HWTEST2_F(AILTests, givenValidPathWhenAILinitProcessExecutableNameThenSuccessIsReturned, MatchAny) {
    AILConfigurationHw<productFamily> ail;

    EXPECT_EQ(ail.initProcessExecutableName(), true);
}

HWTEST2_F(AILTests, givenInvalidPathWhenAILinitProcessExecutableNameThenFailIsReturned, MatchAny) {
    VariableBackup<bool> isAILTestBackup(&SysCalls::isInvalidAILTest);
    SysCalls::isInvalidAILTest = true;

    AILConfigurationHw<productFamily> ail;

    EXPECT_EQ(ail.initProcessExecutableName(), false);
}
} // namespace NEO