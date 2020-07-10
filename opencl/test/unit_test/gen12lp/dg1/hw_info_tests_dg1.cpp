/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/helpers/gtest_helpers.h"
#include "test.h"
using namespace NEO;

template <typename T>
class Dg1HwInfoTests : public ::testing::Test {};
typedef ::testing::Types<DG1_CONFIG> dg1TestTypes;
TYPED_TEST_CASE(Dg1HwInfoTests, dg1TestTypes);

TYPED_TEST(Dg1HwInfoTests, WhenSetupHardwareInfoWithSetupFeatureTableFlagTrueOrFalseIsCalledThenFeatureTableHasCorrectValueOfLocalMemoryFeature) {
    HardwareInfo hwInfo;
    FeatureTable &featureTable = hwInfo.featureTable;

    EXPECT_FALSE(featureTable.ftrLocalMemory);
    TypeParam::setupHardwareInfo(&hwInfo, false);
    EXPECT_FALSE(featureTable.ftrLocalMemory);
    TypeParam::setupHardwareInfo(&hwInfo, true);
    EXPECT_TRUE(featureTable.ftrLocalMemory);
}
