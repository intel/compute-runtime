/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

const HardwareInfo XEHP::hwInfo = XEHP_CONFIG::hwInfo;
const uint64_t XEHP::defaultHardwareInfoConfig = 0;

void setupXEHPHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig) {
    if (hwInfoConfig == 0x0) {
        // Default config
        XEHP_CONFIG::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*XEHP::setupHardwareInfo)(HardwareInfo *, bool, const uint64_t) = setupXEHPHardwareInfoImpl;
