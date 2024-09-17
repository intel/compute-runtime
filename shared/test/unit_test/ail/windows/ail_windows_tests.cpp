/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_ail_configuration.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {

using AILTests = ::testing::Test;
namespace SysCalls {
extern const wchar_t *currentLibraryPath;
}

HWTEST2_F(AILTests, givenValidApplicationPathWhenAILinitProcessExecutableNameThenProperProcessNameIsReturned, MatchAny) {
    VariableBackup<const wchar_t *> applicationPathBackup(&SysCalls::currentLibraryPath);
    applicationPathBackup = L"C\\Users\\Administrator\\application.exe";

    AILWhitebox<productFamily> ail;

    EXPECT_EQ(ail.initProcessExecutableName(), true);

    EXPECT_EQ("application", ail.processName);
}

HWTEST2_F(AILTests, givenValidApplicationPathWithoutLongNameWhenAILinitProcessExecutableNameThenProperProcessNameIsReturned, MatchAny) {
    VariableBackup<const wchar_t *> applicationPathBackup(&SysCalls::currentLibraryPath);
    applicationPathBackup = L"C\\Users\\Administrator\\application";

    AILWhitebox<productFamily> ail;

    EXPECT_EQ(ail.initProcessExecutableName(), true);

    EXPECT_EQ("application", ail.processName);
}

HWTEST2_F(AILTests, givenApplicationPathWithNonLatinCharactersWhenAILinitProcessExecutableNameThenProperProcessNameIsReturned, MatchAny) {
    VariableBackup<const wchar_t *> applicationPathBackup(&SysCalls::currentLibraryPath);
    applicationPathBackup = L"C\\\u4E20\u4E24\\application";

    AILWhitebox<productFamily> ail;

    EXPECT_EQ(ail.initProcessExecutableName(), true);

    EXPECT_EQ("application", ail.processName);
}

} // namespace NEO
