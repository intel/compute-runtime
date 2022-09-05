/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/unified_memory/usm_memory_support.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using PvcConfigHwInfoTests = ::testing::Test;

PVCTEST_F(PvcConfigHwInfoTests, givenPvcDeviceIdsAndRevisionsWhenCheckingConfigsThenReturnCorrectValues) {
    HardwareInfo hwInfo = *defaultHwInfo;

    for (auto &deviceId : pvcXlDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        EXPECT_TRUE(PVC::isXl(hwInfo));
        EXPECT_FALSE(PVC::isXt(hwInfo));

        hwInfo.platform.usRevId = 0x0;
        EXPECT_TRUE(PVC::isXlA0(hwInfo));
        EXPECT_TRUE(PVC::isAtMostXtA0(hwInfo));

        hwInfo.platform.usRevId = 0x1;
        EXPECT_TRUE(PVC::isXlA0(hwInfo));
        EXPECT_TRUE(PVC::isAtMostXtA0(hwInfo));

        hwInfo.platform.usRevId = 0x6;
        EXPECT_FALSE(PVC::isXlA0(hwInfo));
        EXPECT_FALSE(PVC::isAtMostXtA0(hwInfo));
    }

    for (auto &deviceId : pvcXtDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        EXPECT_FALSE(PVC::isXl(hwInfo));
        EXPECT_TRUE(PVC::isXt(hwInfo));

        hwInfo.platform.usRevId = 0x0;
        EXPECT_TRUE(PVC::isAtMostXtA0(hwInfo));

        hwInfo.platform.usRevId = 0x1;
        EXPECT_TRUE(PVC::isAtMostXtA0(hwInfo));

        hwInfo.platform.usRevId = 0x3;
        EXPECT_TRUE(PVC::isAtMostXtA0(hwInfo));

        hwInfo.platform.usRevId = 0x5;
        EXPECT_FALSE(PVC::isAtMostXtA0(hwInfo));
    }
}
PVCTEST_F(PvcConfigHwInfoTests, givenPvcConfigWhenSetupHardwareInfoBaseThenGtSystemInfoIsCorrect) {
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    PVC::setupHardwareInfoBase(&hwInfo, false);

    EXPECT_EQ(128u, gtSystemInfo.MaxFillRate);
    EXPECT_EQ(336u, gtSystemInfo.TotalVsThreads);
    EXPECT_EQ(336u, gtSystemInfo.TotalHsThreads);
    EXPECT_EQ(336u, gtSystemInfo.TotalDsThreads);
    EXPECT_EQ(336u, gtSystemInfo.TotalGsThreads);
    EXPECT_EQ(64u, gtSystemInfo.TotalPsThreadsWindowerRange);
    EXPECT_EQ(8u, gtSystemInfo.CsrSizeInMb);
    EXPECT_FALSE(gtSystemInfo.IsL3HashModeEnabled);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
}

PVCTEST_F(PvcConfigHwInfoTests, givenPvcConfigWhenSetupMultiTileInfoBaseThenGtSystemInfoIsCorrect) {
    DebugManagerStateRestore restorer;
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;

    PVC::setupHardwareInfoMultiTileBase(&hwInfo, false);
    EXPECT_FALSE(gtSystemInfo.MultiTileArchInfo.IsValid);
    EXPECT_EQ(1u, gtSystemInfo.MultiTileArchInfo.TileCount);
    EXPECT_EQ(static_cast<uint8_t>(maxNBitValue(1u)), gtSystemInfo.MultiTileArchInfo.TileMask);

    DebugManager.flags.CreateMultipleSubDevices.set(2);
    PVC::setupHardwareInfoMultiTileBase(&hwInfo, true);
    EXPECT_TRUE(gtSystemInfo.MultiTileArchInfo.IsValid);
    EXPECT_EQ(2u, gtSystemInfo.MultiTileArchInfo.TileCount);
    EXPECT_EQ(static_cast<uint8_t>(maxNBitValue(2u)), gtSystemInfo.MultiTileArchInfo.TileMask);
}

PVCTEST_F(PvcConfigHwInfoTests, givenPvcHwConfigWhenSetupHardwareInfoThenSharedSystemMemCapabilitiesIsCorrect) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto &capabilityTable = hwInfo.capabilityTable;
    PvcHwConfig::setupHardwareInfo(&hwInfo, false);
    uint64_t expectedSharedSystemMemCapabilities = (UNIFIED_SHARED_MEMORY_ACCESS | UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS | UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS);
    EXPECT_EQ(expectedSharedSystemMemCapabilities, capabilityTable.sharedSystemMemCapabilities);
}
