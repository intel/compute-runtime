/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/test_macros/test.h"

extern bool sysmanUltsEnable;

using namespace NEO;

using envVariableMap = std::unordered_map<std::string, std::string>;

namespace L0 {
namespace ult {

class SysmanEnabledFixture : public ::testing::Test {
  public:
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        mockableEnvValues = std::make_unique<envVariableMap>();
        (*mockableEnvValues)["ZES_ENABLE_SYSMAN"] = "1";
        mockableEnvValuesBackup = std::make_unique<VariableBackup<envVariableMap *>>(&IoFunctions::mockableEnvValues, mockableEnvValues.get());
    }

  protected:
    std::unique_ptr<VariableBackup<envVariableMap *>> mockableEnvValuesBackup;
    std::unique_ptr<envVariableMap> mockableEnvValues;
};

} // namespace ult
} // namespace L0
