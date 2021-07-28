/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

const uint64_t ADLP::defaultHardwareInfoConfig = 0x0;
const HardwareInfo ADLP::hwInfo = ADLP_CONFIG::hwInfo;

void setupADLPHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig) {
    ADLP_CONFIG::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
}

void (*ADLP::setupHardwareInfo)(HardwareInfo *, bool, const uint64_t) = setupADLPHardwareInfoImpl;
