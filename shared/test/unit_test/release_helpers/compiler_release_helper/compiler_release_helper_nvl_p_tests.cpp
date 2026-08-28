/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"
#include "shared/test/unit_test/release_helpers/compiler_release_helper/compiler_release_helper_tests_base.h"

#include "gtest/gtest.h"

struct CompilerReleaseHelperNvlPTests : public CompilerReleaseHelperTests<35, 10> {
    std::vector<uint32_t> getRevisions() override {
        return {0, 4};
    }
};

TEST_F(CompilerReleaseHelperNvlPTests, whenGettingCapabilitiesThenCorrectPropertiesAreReturned) {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        compilerReleaseHelper = CompilerReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, compilerReleaseHelper);

        EXPECT_FALSE(compilerReleaseHelper->isForceEmuInt32DivRemSPRequired());
        EXPECT_TRUE(compilerReleaseHelper->isMatrixMultiplyAccumulateSupported());
        EXPECT_EQ(FpAtomicExtFlags::addAtomicCaps, compilerReleaseHelper->getAdditionalFp16Caps());
        EXPECT_EQ(FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps | FpAtomicExtFlags::addAtomicCaps, compilerReleaseHelper->getAdditionalExtraCaps());
        EXPECT_TRUE(compilerReleaseHelper->getFtrXe2Compression());
    }
}
