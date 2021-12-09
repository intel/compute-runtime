/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"

#include "opencl/test/unit_test/helpers/gtest_helpers.h"
#include "opencl/test/unit_test/os_interface/hw_info_config_tests.h"
#include "test.h"
using namespace NEO;

template <typename T>
class Dg1HwInfoTests : public ::testing::Test {};
typedef ::testing::Types<DG1_CONFIG> dg1TestTypes;
TYPED_TEST_CASE(Dg1HwInfoTests, dg1TestTypes);

TYPED_TEST(Dg1HwInfoTests, WhenSetupHardwareInfoWithSetupFeatureTableFlagTrueOrFalseIsCalledThenFeatureTableHasCorrectValueOfLocalMemoryFeature) {
    HardwareInfo hwInfo = *defaultHwInfo;
    FeatureTable &featureTable = hwInfo.featureTable;

    EXPECT_FALSE(featureTable.flags.ftrLocalMemory);
    TypeParam::setupHardwareInfo(&hwInfo, false);
    EXPECT_FALSE(featureTable.flags.ftrLocalMemory);
    TypeParam::setupHardwareInfo(&hwInfo, true);
    EXPECT_TRUE(featureTable.flags.ftrLocalMemory);
}

HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTest, whenOverrideGfxPartitionLayoutForWslThenReturnFalse, IGFX_DG1);

using HwInfoConfigTestDG1 = HwInfoConfigTest;

DG1TEST_F(HwInfoConfigTestDG1, whenOverrideGfxPartitionLayoutForWslThenReturnTrue) {
    auto hwInfoConfig = HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig->overrideGfxPartitionLayoutForWsl());
}