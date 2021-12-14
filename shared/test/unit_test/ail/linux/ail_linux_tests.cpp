/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/test.h"

namespace NEO {
using AILTests = ::testing::Test;
namespace SysCalls {
extern bool isInvalidAILTest;
}

HWTEST2_F(AILTests, givenValidPathWhenAILinitProcessExecutableNameThenSuccessIsReturned, IsDG1) {
    AILConfigurationHw<productFamily> ail;

    EXPECT_EQ(ail.initProcessExecutableName(), true);
}

HWTEST2_F(AILTests, givenInvalidPathWhenAILinitProcessExecutableNameThenFailIsReturned, IsDG1) {
    VariableBackup<bool> isAILTestBackup(&SysCalls::isInvalidAILTest);
    SysCalls::isInvalidAILTest = true;

    AILConfigurationHw<productFamily> ail;

    EXPECT_EQ(ail.initProcessExecutableName(), false);
}
} // namespace NEO