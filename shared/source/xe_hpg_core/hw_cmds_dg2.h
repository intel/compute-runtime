/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"
#include "shared/source/xe_hpg_core/dg2/device_ids_configs_dg2.h"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

#include <algorithm>

namespace NEO {

constexpr uint16_t revIdA0 = 0;
constexpr uint16_t revIdA1 = 1;
constexpr uint16_t revIdB0 = 4;
constexpr uint16_t revIdB1 = 5;
constexpr uint16_t revIdC0 = 8;

struct DG2 : public XeHpgCoreFamily {
    static const PLATFORM platform;
    static const HardwareInfo hwInfo;
    static FeatureTable featureTable;
    static WorkaroundTable workaroundTable;
    static const RuntimeCapabilityTable capabilityTable;
    static void (*setupHardwareInfo)(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig, const ReleaseHelper *releaseHelper);
    static void setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo, const ReleaseHelper &releaseHelper);
    static void setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper);

    static bool isG10(const HardwareInfo &hwInfo) {
        auto it = std::find(dg2G10DeviceIds.begin(), dg2G10DeviceIds.end(), hwInfo.platform.usDeviceID);
        return it != dg2G10DeviceIds.end();
    }

    static bool isG11(const HardwareInfo &hwInfo) {
        auto it = std::find(dg2G11DeviceIds.begin(), dg2G11DeviceIds.end(), hwInfo.platform.usDeviceID);
        return it != dg2G11DeviceIds.end();
    }

    static bool isG12(const HardwareInfo &hwInfo) {
        auto it = std::find(dg2G12DeviceIds.begin(), dg2G12DeviceIds.end(), hwInfo.platform.usDeviceID);
        return it != dg2G12DeviceIds.end();
    }
};

class Dg2HwConfig : public DG2 {
  public:
    static void setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};

} // namespace NEO
