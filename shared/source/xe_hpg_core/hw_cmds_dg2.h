/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/xe_hpg_core/hw_cmds_base.h"

#include "device_ids_configs_dg2.h"

#include <algorithm>

namespace NEO {

struct DG2 : public XE_HPG_COREFamily {
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
    static void adjustHardwareInfo(HardwareInfo *hwInfo);
    static void setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);

    static bool isG10(const HardwareInfo &hwInfo) {
        auto it = std::find(DG2_G10_IDS.begin(), DG2_G10_IDS.end(), hwInfo.platform.usDeviceID);
        return it != DG2_G10_IDS.end();
    }

    static bool isG11(const HardwareInfo &hwInfo) {
        auto it = std::find(DG2_G11_IDS.begin(), DG2_G11_IDS.end(), hwInfo.platform.usDeviceID);
        return it != DG2_G11_IDS.end();
    }
};

class DG2_CONFIG : public DG2 {
  public:
    static void setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};

#include "hw_cmds_dg2.inl"

} // namespace NEO
