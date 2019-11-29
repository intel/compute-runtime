/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/gen8/hw_cmds_base.h"

#include <string>

namespace NEO {

struct BDW : public BDWFamily {
    static const PLATFORM platform;
    static const HardwareInfo hwInfo;
    static const std::string defaultHardwareInfoConfig;
    static FeatureTable featureTable;
    static WorkaroundTable workaroundTable;
    static const uint32_t threadsPerEu = 7;
    static const uint32_t maxEuPerSubslice = 8;
    static const uint32_t maxSlicesSupported = 2;
    static const uint32_t maxSubslicesSupported = 6;
    static const RuntimeCapabilityTable capabilityTable;
    static void (*setupHardwareInfo)(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const std::string &hwInfoConfig);
    static void setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo);
};
class BDW_1x2x6 : public BDW {
  public:
    static void setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};
class BDW_1x3x6 : public BDW {
  public:
    static void setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};
class BDW_1x3x8 : public BDW {
  public:
    static void setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};
class BDW_2x3x8 : public BDW {
  public:
    static void setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};
} // namespace NEO
