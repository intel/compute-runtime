/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"
#include "shared/source/xe_hpc_core/pvc/device_ids_configs_pvc.h"

#include <algorithm>

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
    static void setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);
    static void setupHardwareInfoMultiTileBase(HardwareInfo *hwInfo, bool setupMultiTile);
    static void adjustHardwareInfo(HardwareInfo *hwInfo);

    static constexpr uint8_t pvcBaseDieRevMask = 0b111000; // [3:5]
    static constexpr uint8_t pvcBaseDieA0Masked = 0;       // [3:5] == 0

    static bool isXl(const HardwareInfo &hwInfo) {
        auto it = std::find(pvcXlDeviceIds.begin(), pvcXlDeviceIds.end(), hwInfo.platform.usDeviceID);
        return it != pvcXlDeviceIds.end();
    }

    static bool isXt(const HardwareInfo &hwInfo) {
        auto it = std::find(pvcXtDeviceIds.begin(), pvcXtDeviceIds.end(), hwInfo.platform.usDeviceID);
        return it != pvcXtDeviceIds.end();
    }

    static bool isXlA0(const HardwareInfo &hwInfo) {
        auto revId = hwInfo.platform.usRevId & pvcSteppingBits;
        return (revId < 0x3);
    }

    static bool isAtMostXtA0(const HardwareInfo &hwInfo) {
        auto revId = hwInfo.platform.usRevId & pvcSteppingBits;
        return (revId <= 0x3);
    }
    static constexpr uint32_t pvcSteppingBits = 0b111;
};

class PvcHwConfig : public PVC {
  public:
    static void setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};

#include "hw_cmds_pvc.inl"
} // namespace NEO
