/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"
#include "shared/test/unit_test/release_helpers/compiler_release_helper/compiler_release_helper_tests_base.h"

#include "gtest/gtest.h"

struct CompilerReleaseHelperAdlPTests : public CompilerReleaseHelperTests<12, 3> {

    std::vector<uint32_t> getRevisions() override {
        return {0};
    }
};

TEST_F(CompilerReleaseHelperAdlPTests, whenGettingCapabilitiesThenCorrectPropertiesAreReturned) {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        compilerReleaseHelper = CompilerReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, compilerReleaseHelper);

        EXPECT_TRUE(compilerReleaseHelper->isBindlessAddressingDisabled());
    }
}

TEST_F(CompilerReleaseHelperAdlPTests, whenIsForceEmuInt32DivRemSPRequiredCalledThenFalseReturned) {
    whenIsForceEmuInt32DivRemSPRequiredCalledThenFalseReturned();
}
