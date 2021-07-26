/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

const HardwareInfo XE_HP_SDV::hwInfo = XE_HP_SDV_CONFIG::hwInfo;
const uint64_t XE_HP_SDV::defaultHardwareInfoConfig = 0;

void setupXEHPHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig) {
    if (hwInfoConfig == 0x0) {
        // Default config
        XE_HP_SDV_CONFIG::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*XE_HP_SDV::setupHardwareInfo)(HardwareInfo *, bool, const uint64_t) = setupXEHPHardwareInfoImpl;
