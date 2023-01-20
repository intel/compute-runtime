/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

const HardwareInfo XE_HP_SDV::hwInfo = XehpSdvHwConfig::hwInfo;
const uint64_t XE_HP_SDV::defaultHardwareInfoConfig = 0;

void setupXEHPHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig) {
    XE_HP_SDV::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable);
    XehpSdvHwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
}

void (*XE_HP_SDV::setupHardwareInfo)(HardwareInfo *, bool, const uint64_t) = setupXEHPHardwareInfoImpl;
