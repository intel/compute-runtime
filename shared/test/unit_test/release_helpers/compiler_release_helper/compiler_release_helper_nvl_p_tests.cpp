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
#include "neo_aot_platforms.h"

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

        EXPECT_TRUE(compilerReleaseHelper->isBindlessAddressingDisabled());
        EXPECT_FALSE(compilerReleaseHelper->isForceEmuInt32DivRemSPRequired());
        EXPECT_TRUE(compilerReleaseHelper->isMatrixMultiplyAccumulateSupported());
        EXPECT_FALSE(compilerReleaseHelper->isSplitMatrixMultiplyAccumulateSupported());
        EXPECT_TRUE(compilerReleaseHelper->isBFloat16ConversionSupported());
        EXPECT_EQ(FpAtomicExtFlags::addAtomicCaps, compilerReleaseHelper->getAdditionalFp16Caps());
        EXPECT_EQ(FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps | FpAtomicExtFlags::addAtomicCaps, compilerReleaseHelper->getAdditionalExtraCaps());
        EXPECT_TRUE(compilerReleaseHelper->getFtrXe2Compression());
    }
}

TEST_F(CompilerReleaseHelperNvlPTests, whenIsAvailableSemaphore64BaseCalledThenCorrectValueReturned) {
    for (const auto &baseIpVersion : {static_cast<uint32_t>(AOT::NVL_P_A0)}) {
        ipVersion.value = baseIpVersion;
        for (auto &revision : getRevisions()) {
            ipVersion.revision = revision;
            compilerReleaseHelper = CompilerReleaseHelper::create(ipVersion);
            ASSERT_NE(nullptr, compilerReleaseHelper);
            if (revision != 0) {
                EXPECT_TRUE(compilerReleaseHelper->isAvailableSemaphore64Base());
            } else {
                EXPECT_FALSE(compilerReleaseHelper->isAvailableSemaphore64Base());
            }
        }
    }
}
