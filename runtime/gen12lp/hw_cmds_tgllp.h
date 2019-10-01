/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/gen12lp/hw_cmds_base.h"
namespace NEO {

struct TGLLP : public TGLLPFamily {
    static const PLATFORM platform;
    static const HardwareInfo hwInfo;
    static FeatureTable featureTable;
    static WorkaroundTable workaroundTable;
    static const uint32_t threadsPerEu = 7;
    static const uint32_t maxEuPerSubslice = 16;
    static const uint32_t maxSlicesSupported = 1;
    static const uint32_t maxSubslicesSupported = 6;
    static const uint32_t maxDualSubslicesSupported = 12;
    static const RuntimeCapabilityTable capabilityTable;
    static void (*setupHardwareInfo)(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const std::string &hwInfoConfig);
    static void setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo);
};

class TGLLP_1x6x16 : public TGLLP {
  public:
    static void setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};

class TGLLP_1x2x16 : public TGLLP {
  public:
    static void setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};
} // namespace NEO
