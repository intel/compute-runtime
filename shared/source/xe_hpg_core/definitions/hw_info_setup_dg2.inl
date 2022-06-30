/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

const HardwareInfo DG2::hwInfo = Dg2HwConfig::hwInfo;
const uint64_t DG2::defaultHardwareInfoConfig = 0;

void DG2::adjustHardwareInfo(HardwareInfo *hwInfo) {}

void setupDG2HardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig) {
    if (hwInfoConfig == 0x0) {
        // Default config
        Dg2HwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*DG2::setupHardwareInfo)(HardwareInfo *, bool, const uint64_t) = setupDG2HardwareInfoImpl;