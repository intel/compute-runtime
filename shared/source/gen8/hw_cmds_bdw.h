/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gen8/hw_cmds_base.h"
#include "shared/source/helpers/hw_info.h"

#include <string>

namespace NEO {

struct BDW : public Gen8Family {
    static const PLATFORM platform;
    static const HardwareInfo hwInfo;
    static FeatureTable featureTable;
    static WorkaroundTable workaroundTable;
    static const uint32_t maxEuPerSubslice = 8;
    static const uint32_t maxSlicesSupported = 2;
    static const uint32_t maxSubslicesSupported = 6;
    static const RuntimeCapabilityTable capabilityTable;
    static void (*setupHardwareInfo)(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig, const ReleaseHelper *releaseHelper);
    static void setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo);
    static void setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper);
};
class BdwHw1x2x6 : public BDW {
  public:
    static void setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};
class BdwHw1x3x6 : public BDW {
  public:
    static void setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};
class BdwHw1x3x8 : public BDW {
  public:
    static void setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};
class BdwHw2x3x8 : public BDW {
  public:
    static void setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};
} // namespace NEO
