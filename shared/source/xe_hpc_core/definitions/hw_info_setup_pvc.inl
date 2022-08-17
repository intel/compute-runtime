/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

const HardwareInfo PVC::hwInfo = PvcHwConfig::hwInfo;
const uint64_t PVC::defaultHardwareInfoConfig = 0;

void setupPVCHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig) {
    PVC::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable);
    PvcHwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
}

void (*PVC::setupHardwareInfo)(HardwareInfo *, bool, const uint64_t) = setupPVCHardwareInfoImpl;