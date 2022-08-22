/*
 * Copyright (C) 2021-2022 Intel Corporation
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
extern const wchar_t *currentLibraryPath;
}

template <PRODUCT_FAMILY productFamily>
class AILMock : public AILConfigurationHw<productFamily> {
  public:
    using AILConfiguration::processName;
};

HWTEST2_F(AILTests, givenValidApplicationPathWhenAILinitProcessExecutableNameThenProperProcessNameIsReturned, IsAtLeastGen12lp) {
    VariableBackup<const wchar_t *> applicationPathBackup(&SysCalls::currentLibraryPath);
    applicationPathBackup = L"C\\Users\\Administrator\\application.exe";

    VariableBackup<AILConfiguration *> ailConfigurationBackup(&ailConfigurationTable[productFamily]);

    AILMock<productFamily> ailTemp;
    ailConfigurationTable[productFamily] = &ailTemp;

    EXPECT_EQ(ailTemp.initProcessExecutableName(), true);

    EXPECT_EQ("application", ailTemp.processName);
}

HWTEST2_F(AILTests, givenValidApplicationPathWithoutLongNameWhenAILinitProcessExecutableNameThenProperProcessNameIsReturned, IsAtLeastGen12lp) {
    VariableBackup<const wchar_t *> applicationPathBackup(&SysCalls::currentLibraryPath);
    applicationPathBackup = L"C\\Users\\Administrator\\application";

    VariableBackup<AILConfiguration *> ailConfigurationBackup(&ailConfigurationTable[productFamily]);

    AILMock<productFamily> ailTemp;
    ailConfigurationTable[productFamily] = &ailTemp;

    EXPECT_EQ(ailTemp.initProcessExecutableName(), true);

    EXPECT_EQ("application", ailTemp.processName);
}

HWTEST2_F(AILTests, givenApplicationPathWithNonLatinCharactersWhenAILinitProcessExecutableNameThenProperProcessNameIsReturned, IsAtLeastGen12lp) {
    VariableBackup<const wchar_t *> applicationPathBackup(&SysCalls::currentLibraryPath);
    applicationPathBackup = L"C\\\u4E20\u4E24\\application";

    VariableBackup<AILConfiguration *> ailConfigurationBackup(&ailConfigurationTable[productFamily]);

    AILMock<productFamily> ailTemp;
    ailConfigurationTable[productFamily] = &ailTemp;

    EXPECT_EQ(ailTemp.initProcessExecutableName(), true);

    EXPECT_EQ("application", ailTemp.processName);
}

} // namespace NEO
