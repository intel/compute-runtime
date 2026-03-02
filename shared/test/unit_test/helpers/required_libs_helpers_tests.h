/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/unit_test/mocks/mock_required_libs_helpers.h"

#include "gtest/gtest.h"

namespace NEO {

struct RequiredLibsHelpersTest : public ::testing::Test {
    void SetUp() override {
        MockRequiredLibsHelpers::getDefaultBinarySearchPathsCalled = 0;
        MockRequiredLibsHelpers::getOptionalBinarySearchPathsCalled = 0;
        NEO::IoFunctions::mockGetenvCalled = 0U;
    }

    void TearDown() override {
        MockRequiredLibsHelpers::optionalSearchPaths.clear();
        MockRequiredLibsHelpers::optionalSearchPaths.shrink_to_fit();
        MockRequiredLibsHelpers::defaultSearchPaths.clear();
        MockRequiredLibsHelpers::defaultSearchPaths.shrink_to_fit();
        NEO::IoFunctions::mockGetenvCalled = 0U;
        NEO::IoFunctions::mockableEnvValues = nullptr;
        MockRequiredLibsHelpers::getDefaultBinarySearchPathsCalled = 0;
        MockRequiredLibsHelpers::getOptionalBinarySearchPathsCalled = 0;
    }

    DebugManagerStateRestore restorer;
};

} // namespace NEO
