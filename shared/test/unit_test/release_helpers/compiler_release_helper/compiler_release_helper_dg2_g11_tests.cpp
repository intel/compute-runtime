/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"
#include "shared/test/unit_test/release_helpers/compiler_release_helper/compiler_release_helper_tests_base.h"

#include "gtest/gtest.h"

struct CompilerReleaseHelperDg2G11Tests : public CompilerReleaseHelperTests<12, 56> {
    std::vector<uint32_t> getRevisions() override {
        return {0, 4, 5};
    }
};

TEST_F(CompilerReleaseHelperDg2G11Tests, whenGettingCapabilitiesThenCorrectPropertiesAreReturned) {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        compilerReleaseHelper = CompilerReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, compilerReleaseHelper);

        EXPECT_FALSE(compilerReleaseHelper->isBindlessAddressingDisabled());
        EXPECT_FALSE(compilerReleaseHelper->isForceEmuInt32DivRemSPRequired());
        EXPECT_TRUE(compilerReleaseHelper->isMatrixMultiplyAccumulateSupported());
        EXPECT_TRUE(compilerReleaseHelper->isSplitMatrixMultiplyAccumulateSupported());
        EXPECT_TRUE(compilerReleaseHelper->isBFloat16ConversionSupported());

        EXPECT_EQ(0u, compilerReleaseHelper->getAdditionalFp16Caps());
        EXPECT_EQ(0u, compilerReleaseHelper->getAdditionalExtraCaps());
    }
}
