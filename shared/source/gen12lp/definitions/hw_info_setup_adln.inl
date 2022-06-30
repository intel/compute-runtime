/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

const uint64_t ADLN::defaultHardwareInfoConfig = 0x0;
const HardwareInfo ADLN::hwInfo = AdlnHwConfig::hwInfo;

void setupADLNHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig) {
    AdlnHwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
}

void (*ADLN::setupHardwareInfo)(HardwareInfo *, bool, const uint64_t) = setupADLNHardwareInfoImpl;
