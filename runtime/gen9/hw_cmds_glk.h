/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/gen9/hw_cmds_base.h"

namespace NEO {

struct GLK : public SKLFamily {
    static const PLATFORM platform;
    static const HardwareInfo hwInfo;

    static const uint32_t threadsPerEu = 6;
    static const uint32_t maxEuPerSubslice = 6;
    static const uint32_t maxSlicesSupported = 1;
    static const uint32_t maxSubslicesSupported = 3;

    static const RuntimeCapabilityTable capabilityTable;
    static void (*setupHardwareInfo)(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable, const std::string &hwInfoConfig);
};

class GLK_1x3x6 : public GLK {
  public:
    static void setupHardwareInfo(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};

class GLK_1x2x6 : public GLK {
  public:
    static void setupHardwareInfo(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};
} // namespace NEO
