/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

const HardwareInfo ADLP::hwInfo = AdlpHwConfig::hwInfo;

void setupADLPHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig) {
    AdlpHwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
}

void (*ADLP::setupHardwareInfo)(HardwareInfo *, bool, const uint64_t) = setupADLPHardwareInfoImpl;
