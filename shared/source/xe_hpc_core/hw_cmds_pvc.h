/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/xe_hpc_core/hw_cmds_base.h"

#include "device_ids_configs_pvc.h"

namespace NEO {

struct PVC : public XE_HPC_COREFamily {
    static const PLATFORM platform;
    static const HardwareInfo hwInfo;
    static const uint64_t defaultHardwareInfoConfig;
    static FeatureTable featureTable;
    static WorkaroundTable workaroundTable;
    // Initial non-zero values for unit tests
    static const uint32_t threadsPerEu = 8;
    static const uint32_t maxEuPerSubslice = 8;
    static const uint32_t maxSlicesSupported = 8;
    static const uint32_t maxSubslicesSupported = 64;
    static const uint32_t maxDualSubslicesSupported = 64;
    static const RuntimeCapabilityTable capabilityTable;
    static void (*setupHardwareInfo)(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig);
    static void setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo);
    static void setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, bool setupMultiTile);
    static void adjustHardwareInfo(HardwareInfo *hwInfo);
    static bool isXlA0(const HardwareInfo &hwInfo);
    static bool isAtMostXtA0(const HardwareInfo &hwInfo);

    static bool isXl(const HardwareInfo &hwInfo) {
        auto it = std::find(PVC_XL_IDS.begin(), PVC_XL_IDS.end(), hwInfo.platform.usDeviceID);
        if (it != PVC_XL_IDS.end()) {
            return true;
        }
        return false;
    }

    static bool isXt(const HardwareInfo &hwInfo) {
        auto it = std::find(PVC_XT_IDS.begin(), PVC_XT_IDS.end(), hwInfo.platform.usDeviceID);
        if (it != PVC_XT_IDS.end()) {
            return true;
        }
        return false;
    }

    static constexpr uint32_t pvcSteppingBits = 0b111;
};

class PVC_CONFIG : public PVC {
  public:
    static void setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);
    static void setupHardwareInfoMultiTile(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, bool setupMultiTile);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};

#include "hw_cmds_pvc.inl"
} // namespace NEO
