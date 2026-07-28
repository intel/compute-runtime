/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/release_helpers/compiler_release_helper/compiler_release_helper_tests_base.h"

#include "gtest/gtest.h"

struct CompilerReleaseHelperNvlUTests : public CompilerReleaseHelperTests<30, 5> {

    std::vector<uint32_t> getRevisions() override {
        return {0, 1, 4};
    }
};

TEST_F(CompilerReleaseHelperNvlUTests, whenIsForceEmuInt32DivRemSPRequiredCalledThenFalseReturned) {
    whenIsForceEmuInt32DivRemSPRequiredCalledThenFalseReturned();
}
