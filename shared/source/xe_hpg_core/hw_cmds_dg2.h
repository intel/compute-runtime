/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/xe_hpg_core/hw_cmds_base.h"
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
};

class DG2_CONFIG : public DG2 {
  public:
    static void setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);
    static void setupHardwareInfoMultiTile(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, bool setupMultiTile);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};

#include "hw_cmds_dg2.inl"

} // namespace NEO
