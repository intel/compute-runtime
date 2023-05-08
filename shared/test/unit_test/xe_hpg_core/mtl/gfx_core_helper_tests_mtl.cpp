/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using GfxCoreHelperTestMtl = GfxCoreHelperTest;

MTLTEST_F(GfxCoreHelperTestMtl, givenAllocationThenCheckResourceCompatibilityReturnsTrue) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto allocation = std::make_unique<GraphicsAllocation>(0, AllocationType::BUFFER, nullptr, 0u, 0, MemoryPool::MemoryNull, 3u, 0llu);
    EXPECT_TRUE(gfxCoreHelper.checkResourceCompatibility(*allocation));
}

MTLTEST_F(GfxCoreHelperTestMtl, givenisCompressionEnabledAndWaAuxTable64KGranularWhenCheckIs1MbAlignmentSupportedThenReturnCorrectValue) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    auto hardwareInfo = *defaultHwInfo;
    auto isCompressionEnabled = true;
    hardwareInfo.workaroundTable.flags.waAuxTable64KGranular = true;
    EXPECT_FALSE(gfxCoreHelper.is1MbAlignmentSupported(hardwareInfo, isCompressionEnabled));

    isCompressionEnabled = false;
    hardwareInfo.workaroundTable.flags.waAuxTable64KGranular = true;
    EXPECT_FALSE(gfxCoreHelper.is1MbAlignmentSupported(hardwareInfo, isCompressionEnabled));

    isCompressionEnabled = false;
    hardwareInfo.workaroundTable.flags.waAuxTable64KGranular = false;
    EXPECT_FALSE(gfxCoreHelper.is1MbAlignmentSupported(hardwareInfo, isCompressionEnabled));

    isCompressionEnabled = true;
    hardwareInfo.workaroundTable.flags.waAuxTable64KGranular = false;
    EXPECT_TRUE(gfxCoreHelper.is1MbAlignmentSupported(hardwareInfo, isCompressionEnabled));
}

MTLTEST_F(GfxCoreHelperTestMtl, givenRevisionEnumAndPlatformFamilyTypeThenProperValueForIsWorkaroundRequiredIsReturned) {
    uint32_t steppings[] = {
        REVISION_A0,
        REVISION_B,
        CommonConstants::invalidStepping,
    };

    auto hardwareInfo = *defaultHwInfo;
    const auto &productHelper = getHelper<ProductHelper>();

    for (auto stepping : steppings) {
        hardwareInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(stepping, hardwareInfo);

        if (stepping == REVISION_A0) {
            EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo, productHelper));
        } else {
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo, productHelper));
        }

        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_B, REVISION_A0, hardwareInfo, productHelper));

        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_A1, hardwareInfo, productHelper));
        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo, productHelper));
        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_D, hardwareInfo, productHelper));
        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_D, REVISION_A0, hardwareInfo, productHelper));
    }
}

MTLTEST_F(GfxCoreHelperTestMtl, givenMtlWhenSetForceNonCoherentThenNothingChanged) {
    using FORCE_NON_COHERENT = typename FamilyType::STATE_COMPUTE_MODE::FORCE_NON_COHERENT;

    auto &productHelper = getHelper<ProductHelper>();

    auto stateComputeMode = FamilyType::cmdInitStateComputeMode;
    auto properties = StateComputeModeProperties{};

    properties.isCoherencyRequired.set(true);
    productHelper.setForceNonCoherent(&stateComputeMode, properties);
    EXPECT_EQ(FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_DISABLED, stateComputeMode.getForceNonCoherent());
    EXPECT_EQ(0u, stateComputeMode.getMaskBits());
}

MTLTEST_F(GfxCoreHelperTestMtl, GivenVariousValuesWhenComputeSlmSizeIsCalledThenCorrectValueIsReturned) {
    auto hardwareInfo = *defaultHwInfo;
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    for (auto &testInput : computeSlmValuesXeHPAndLaterTestsInput) {
        EXPECT_EQ(testInput.expected, gfxCoreHelper.computeSlmValues(hardwareInfo, testInput.slmSize));
    }
}
