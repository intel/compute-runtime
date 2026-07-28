/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/release_helpers/compiler_release_helper/compiler_release_helper_tests_base.h"

#include "gtest/gtest.h"

struct CompilerReleaseHelperDg2G11Tests : public CompilerReleaseHelperTests<12, 56> {

    std::vector<uint32_t> getRevisions() override {
        return {0, 4, 5};
    }
};

TEST_F(CompilerReleaseHelperDg2G11Tests, whenIsForceEmuInt32DivRemSPRequiredCalledThenFalseReturned) {
    whenIsForceEmuInt32DivRemSPRequiredCalledThenFalseReturned();
}
