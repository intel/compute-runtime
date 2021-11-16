/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

const HardwareInfo PVC::hwInfo = PVC_CONFIG::hwInfo;
const uint64_t PVC::defaultHardwareInfoConfig = 0;

void setupPVCHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig) {
    if (hwInfoConfig == 0x0) {
        // Default config
        PVC_CONFIG::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*PVC::setupHardwareInfo)(HardwareInfo *, bool, const uint64_t) = setupPVCHardwareInfoImpl;