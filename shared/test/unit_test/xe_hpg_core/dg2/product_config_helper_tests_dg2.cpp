/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/product_config_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "platforms.h"

using namespace NEO;
using ProductConfigHelperDg2Tests = ::testing::Test;
using ProductHelperTests = Test<DeviceFixture>;

DG2TEST_F(ProductConfigHelperDg2Tests, givenVariousVariantsOfXeHpgAcronymsWhenGetReleaseThenCorrectValueIsReturned) {
    std::vector<std::string> acronymsVariants = {"xe_hpg_core", "xe_hpg", "xehpg", "XeHpg"};
    for (auto &acronym : acronymsVariants) {
        ProductConfigHelper::adjustDeviceName(acronym);
        auto ret = ProductConfigHelper::getReleaseForAcronym(acronym);
        EXPECT_EQ(ret, AOT::XE_HPG_RELEASE);
    }
}

DG2TEST_F(ProductConfigHelperDg2Tests, givenXeHpgReleaseWhenSearchForDeviceAcronymThenObjectIsFound) {
    auto productConfigHelper = std::make_unique<ProductConfigHelper>();
    auto aotInfos = productConfigHelper->getDeviceAotInfo();
    EXPECT_TRUE(std::any_of(aotInfos.begin(), aotInfos.end(), ProductConfigHelper::findDeviceAcronymForRelease(AOT::XE_HPG_RELEASE)));
}
DG2TEST_F(ProductHelperTests, givenNoDpasInstructionInKernelHelperWhenCheckingIfEuFusionShouldBeDisabledThenFalseReturned) {
    auto &gfxCoreHelper = getHelper<ProductHelper>();
    const uint32_t lws[3] = {1, 1, 1};
    const uint32_t groupCount[3] = {5, 3, 1};
    bool dpasInstruction = false;
    EXPECT_FALSE(gfxCoreHelper.isFusedEuDisabledForDpas(dpasInstruction, lws, groupCount));
}
DG2TEST_F(ProductHelperTests, givenDpasInstructionLwsAndGroupCountIsNullPtrInKernelHelperWhenCheckingIfEuFusionShouldBeDisabledThenTrueReturned) {
    auto &gfxCoreHelper = getHelper<ProductHelper>();
    bool dpasInstruction = true;
    EXPECT_TRUE(gfxCoreHelper.isFusedEuDisabledForDpas(dpasInstruction, nullptr, nullptr));
}
DG2TEST_F(ProductHelperTests, givenDpasInstructionLwsIsNullPtrInKernelHelperWhenCheckingIfEuFusionShouldBeDisabledThenTrueReturned) {
    auto &gfxCoreHelper = getHelper<ProductHelper>();
    bool dpasInstruction = true;
    const uint32_t groupCount[3] = {5, 3, 1};
    EXPECT_TRUE(gfxCoreHelper.isFusedEuDisabledForDpas(dpasInstruction, nullptr, groupCount));
}
DG2TEST_F(ProductHelperTests, givenDpasInstructionGroupCountIsNullPtrInKernelHelperWhenCheckingIfEuFusionShouldBeDisabledThenTrueReturned) {
    auto &gfxCoreHelper = getHelper<ProductHelper>();
    bool dpasInstruction = true;
    const uint32_t lws[3] = {1, 1, 1};
    EXPECT_TRUE(gfxCoreHelper.isFusedEuDisabledForDpas(dpasInstruction, lws, nullptr));
}
DG2TEST_F(ProductHelperTests, givenDpasInstructionLwsAndLwsIsOddWhenCheckingIfEuFusionShouldBeDisabledThenTrueReturned) {
    auto &gfxCoreHelper = getHelper<ProductHelper>();
    const uint32_t lws[3] = {7, 3, 1};
    const uint32_t groupCount[3] = {2, 1, 1};
    bool dpasInstruction = true;
    EXPECT_TRUE(gfxCoreHelper.isFusedEuDisabledForDpas(dpasInstruction, lws, groupCount));
}
DG2TEST_F(ProductHelperTests, givenDpasInstructionLwsAndLwsIsNoOddWhenCheckingIfEuFusionShouldBeDisabledThenFalseReturned) {
    auto &gfxCoreHelper = getHelper<ProductHelper>();
    const uint32_t lws[3] = {8, 3, 1};
    const uint32_t groupCount[3] = {2, 1, 1};
    bool dpasInstruction = true;
    EXPECT_FALSE(gfxCoreHelper.isFusedEuDisabledForDpas(dpasInstruction, lws, groupCount));
}
DG2TEST_F(ProductHelperTests, givenDpasInstructionLwsAndLwsIsOneAndXGroupCountIsOddWhenCheckingIfEuFusionShouldBeDisabledThenFalseReturned) {
    auto &gfxCoreHelper = getHelper<ProductHelper>();
    const uint32_t lws[3] = {1, 1, 1};
    const uint32_t groupCount[3] = {5, 1, 1};
    bool dpasInstruction = true;
    EXPECT_TRUE(gfxCoreHelper.isFusedEuDisabledForDpas(dpasInstruction, lws, groupCount));
}
DG2TEST_F(ProductHelperTests, givenDpasInstructionLwsAndLwsIsOneAndXGroupCountIsNoOddWhenCheckingIfEuFusionShouldBeDisabledThenFalseReturned) {
    auto &gfxCoreHelper = getHelper<ProductHelper>();
    const uint32_t lws[3] = {1, 1, 1};
    const uint32_t groupCount[3] = {4, 1, 1};
    bool dpasInstruction = true;
    EXPECT_FALSE(gfxCoreHelper.isFusedEuDisabledForDpas(dpasInstruction, lws, groupCount));
}
DG2TEST_F(ProductHelperTests, givenDg2ProductHelperWhenCallingIsCalculationForDisablingEuFusionWithDpasNeededThenTrueReturned) {
    auto &gfxCoreHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(gfxCoreHelper.isCalculationForDisablingEuFusionWithDpasNeeded());
}