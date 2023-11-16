/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gen12lp/hw_cmds_base.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {

struct TGLLP : public Gen12LpFamily {
    static const PLATFORM platform;
    static const HardwareInfo hwInfo;
    static FeatureTable featureTable;
    static WorkaroundTable workaroundTable;
    static const uint32_t maxEuPerSubslice = 16;
    static const uint32_t maxSlicesSupported = 1;
    static const uint32_t maxSubslicesSupported = 6;
    static const uint32_t maxDualSubslicesSupported = 6;
    static const RuntimeCapabilityTable capabilityTable;
    static void (*setupHardwareInfo)(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig, const ReleaseHelper *releaseHelper);
    static void setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo);
    static void setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper);
};

class TgllpHw1x6x16 : public TGLLP {
  public:
    static void setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};

class TgllpHw1x2x16 : public TGLLP {
  public:
    static void setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};
} // namespace NEO
