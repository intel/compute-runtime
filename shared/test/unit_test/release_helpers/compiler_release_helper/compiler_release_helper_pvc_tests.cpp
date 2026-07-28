/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/release_helpers/compiler_release_helper/compiler_release_helper_tests_base.h"

#include "gtest/gtest.h"

struct CompilerReleaseHelperPvcTests : public CompilerReleaseHelperTests<12, 60> {

    std::vector<uint32_t> getRevisions() override {
        return {0, 1, 3, 5, 6, 7};
    }
};

TEST_F(CompilerReleaseHelperPvcTests, whenIsForceEmuInt32DivRemSPRequiredCalledThenFalseReturned) {
    whenIsForceEmuInt32DivRemSPRequiredCalledThenFalseReturned();
}
