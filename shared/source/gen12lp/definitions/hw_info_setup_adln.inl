/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

const HardwareInfo ADLN::hwInfo = AdlnHwConfig::hwInfo;

void setupADLNHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig, const CompilerProductHelper &compilerProductHelper) {
    AdlnHwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, compilerProductHelper);
}

void (*ADLN::setupHardwareInfo)(HardwareInfo *, bool, uint64_t, const CompilerProductHelper &) = setupADLNHardwareInfoImpl;
