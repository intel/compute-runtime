/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"
namespace NEO {

struct MTL : public XeHpgCoreFamily {
    static const PLATFORM platform;
    static const HardwareInfo hwInfo;
    static const uint64_t defaultHardwareInfoConfig;
    static FeatureTable featureTable;
    static WorkaroundTable workaroundTable;
    // Initial non-zero values for unit tests
    static const uint32_t threadsPerEu = 8;
    static const uint32_t maxEuPerSubslice = 16;
    static const uint32_t maxSlicesSupported = 8;
    static const uint32_t maxSubslicesSupported = 32;
    static const uint32_t maxDualSubslicesSupported = 32;
    static const RuntimeCapabilityTable capabilityTable;
    static void (*setupHardwareInfo)(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig);
    static void setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo);
    static void setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);
    static void setupHardwareInfoMultiTileBase(HardwareInfo *hwInfo);

    static bool isLpg(const HardwareInfo &hwInfo) {
        return ((hwInfo.ipVersion.architecture == 12) &&
                (hwInfo.ipVersion.release <= 71));
    }
};

class MtlHwConfig : public MTL {
  public:
    static void setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};

#include "hw_cmds_mtl.inl"
} // namespace NEO
