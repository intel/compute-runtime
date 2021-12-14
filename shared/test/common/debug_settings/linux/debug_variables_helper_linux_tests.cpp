/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_variables_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/test_macros/test.h"

#include <unordered_map>

namespace NEO {

TEST(DebugVariablesHelperTests, whenIsDebugKeysReadEnableIsCalledThenProperValueIsReturned) {
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{"NEOReadDebugKeys", "1"}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

        EXPECT_TRUE(isDebugKeysReadEnabled());
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{"NEOReadDebugKeys", "0"}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

        EXPECT_FALSE(isDebugKeysReadEnabled());
    }
}

} // namespace NEO
