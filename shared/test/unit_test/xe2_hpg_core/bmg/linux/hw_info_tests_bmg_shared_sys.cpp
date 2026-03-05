/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/unified_memory/usm_memory_support.h"
#include "shared/source/xe2_hpg_core/hw_cmds_bmg.h"
#include "shared/source/xe2_hpg_core/hw_info_bmg.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using BmgHwInfoTest = ::testing::Test;

BMGTEST_F(BmgHwInfoTest, givenBmgHwConfigWhenSetupHardwareInfoThenSharedSystemMemCapabilitiesIsCorrect) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto compilerProductHelper = CompilerProductHelper::create(hwInfo.platform.eProductFamily);
    auto releaseHelper = ReleaseHelper::create(hwInfo.ipVersion);
    auto &capabilityTable = hwInfo.capabilityTable;
    BmgHwConfig::setupHardwareInfo(&hwInfo, false, releaseHelper.get());
    uint64_t expectedSharedSystemMemCapabilities = (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess);
    EXPECT_EQ(expectedSharedSystemMemCapabilities, capabilityTable.sharedSystemMemCapabilities);
}