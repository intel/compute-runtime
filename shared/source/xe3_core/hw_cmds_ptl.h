/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"
#include "shared/source/xe3_core/hw_cmds_base.h"

namespace NEO {

struct PTL : public Xe3CoreFamily {
    static const PLATFORM platform;
    static const HardwareInfo hwInfo;
    static FeatureTable featureTable;
    static WorkaroundTable workaroundTable;
    static const RuntimeCapabilityTable capabilityTable;
    static void (*setupHardwareInfo)(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);
    static void setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo);
    static void setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);
    static void setupHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);
    static void adjustHardwareInfo(HardwareInfo *hwInfo);

    static constexpr bool isDcFlushAllowed = true;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};

} // namespace NEO
