/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"
#include "shared/test/unit_test/release_helpers/compiler_release_helper/compiler_release_helper_tests_base.h"

#include "gtest/gtest.h"

struct CompilerReleaseHelperRklTests : public CompilerReleaseHelperTests<12, 1> {

    std::vector<uint32_t> getRevisions() override {
        return {0};
    }
};

TEST_F(CompilerReleaseHelperRklTests, whenGettingCapabilitiesThenCorrectPropertiesAreReturned) {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        compilerReleaseHelper = CompilerReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, compilerReleaseHelper);

        EXPECT_TRUE(compilerReleaseHelper->isBindlessAddressingDisabled());
    }
}

TEST_F(CompilerReleaseHelperRklTests, whenIsForceEmuInt32DivRemSPRequiredCalledThenTrueReturned) {
    whenIsForceEmuInt32DivRemSPRequiredCalledThenTrueReturned();
}
