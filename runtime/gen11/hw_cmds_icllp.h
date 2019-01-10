/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/gen11/hw_cmds_base.h"

namespace NEO {

struct ICLLP : public ICLFamily {
    static const PLATFORM platform;
    static const HardwareInfo hwInfo;
    static const uint32_t threadsPerEu = 7;
    static const uint32_t maxEuPerSubslice = 8;
    static const uint32_t maxSlicesSupported = 1;
    static const uint32_t maxSubslicesSupported = 8;
    static const RuntimeCapabilityTable capabilityTable;
    static void (*setupHardwareInfo)(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable, const std::string &hwInfoConfig);
};

class ICLLP_1x8x8 : public ICLLP {
  public:
    static void setupHardwareInfo(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};

class ICLLP_1x4x8 : public ICLLP {
  public:
    static void setupHardwareInfo(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};
class ICLLP_1x6x8 : public ICLLP {
  public:
    static void setupHardwareInfo(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};

class ICLLP_1x1x8 : public ICLLP {
  public:
    static void setupHardwareInfo(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};

} // namespace NEO
