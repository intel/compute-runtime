/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

template <typename T>
class Dg1HwInfoTests : public ::testing::Test {};
typedef ::testing::Types<Dg1HwConfig> dg1TestTypes;
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