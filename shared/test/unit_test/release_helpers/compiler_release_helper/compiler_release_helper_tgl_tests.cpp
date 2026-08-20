/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"
#include "shared/test/unit_test/release_helpers/compiler_release_helper/compiler_release_helper_tests_base.h"

#include "gtest/gtest.h"

struct CompilerReleaseHelperTglTests : public CompilerReleaseHelperTests<12, 0> {
    std::vector<uint32_t> getRevisions() override {
        return {0};
    }
};

TEST_F(CompilerReleaseHelperTglTests, whenGettingCapabilitiesThenCorrectPropertiesAreReturned) {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        compilerReleaseHelper = CompilerReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, compilerReleaseHelper);

        EXPECT_TRUE(compilerReleaseHelper->isBindlessAddressingDisabled());
        EXPECT_FALSE(compilerReleaseHelper->isForceEmuInt32DivRemSPRequired());
        EXPECT_FALSE(compilerReleaseHelper->isMatrixMultiplyAccumulateSupported());
        EXPECT_EQ(0u, compilerReleaseHelper->getAdditionalFp16Caps());
        EXPECT_EQ(0u, compilerReleaseHelper->getAdditionalExtraCaps());
        EXPECT_TRUE(compilerReleaseHelper->getFtrXe2Compression());
        EXPECT_FALSE(compilerReleaseHelper->isAvailableSemaphore64Base());
    }
}
