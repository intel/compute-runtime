/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"
#include "shared/test/unit_test/release_helpers/compiler_release_helper/compiler_release_helper_tests_base.h"

#include "gtest/gtest.h"

struct CompilerReleaseHelperPvcTests : public CompilerReleaseHelperTests<12, 60> {
    std::vector<uint32_t> getRevisions() override {
        return {0, 1, 3, 5, 6, 7};
    }
};

TEST_F(CompilerReleaseHelperPvcTests, whenGettingCapabilitiesThenCorrectPropertiesAreReturned) {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        compilerReleaseHelper = CompilerReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, compilerReleaseHelper);

        EXPECT_TRUE(compilerReleaseHelper->isBindlessAddressingDisabled());
        EXPECT_FALSE(compilerReleaseHelper->isForceEmuInt32DivRemSPRequired());
        EXPECT_TRUE(compilerReleaseHelper->isMatrixMultiplyAccumulateSupported());
        EXPECT_FALSE(compilerReleaseHelper->isSplitMatrixMultiplyAccumulateSupported());
        EXPECT_TRUE(compilerReleaseHelper->isBFloat16ConversionSupported());
        EXPECT_EQ(0u, compilerReleaseHelper->getAdditionalFp16Caps());
        EXPECT_EQ(0u, compilerReleaseHelper->getAdditionalExtraCaps());
        EXPECT_FALSE(compilerReleaseHelper->getFtrXe2Compression());
        EXPECT_FALSE(compilerReleaseHelper->isAvailableSemaphore64Base());
    }
}
