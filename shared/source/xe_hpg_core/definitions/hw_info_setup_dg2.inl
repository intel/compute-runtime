/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

const HardwareInfo DG2::hwInfo = Dg2HwConfig::hwInfo;

void DG2::adjustHardwareInfo(HardwareInfo *hwInfo) {}

void setupDG2HardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig, const CompilerProductHelper &compilerProductHelper) {
    DG2::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable, compilerProductHelper);
    Dg2HwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, compilerProductHelper);
}

void (*DG2::setupHardwareInfo)(HardwareInfo *, bool, uint64_t, const CompilerProductHelper &) = setupDG2HardwareInfoImpl;
