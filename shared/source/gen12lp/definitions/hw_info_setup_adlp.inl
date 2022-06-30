/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

const uint64_t ADLP::defaultHardwareInfoConfig = 0x0;
const HardwareInfo ADLP::hwInfo = AdlpHwConfig::hwInfo;

void setupADLPHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig) {
    AdlpHwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
}

void (*ADLP::setupHardwareInfo)(HardwareInfo *, bool, const uint64_t) = setupADLPHardwareInfoImpl;
