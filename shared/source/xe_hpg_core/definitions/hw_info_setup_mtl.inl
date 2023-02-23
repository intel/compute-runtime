/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {
const HardwareInfo MTL::hwInfo = MtlHwConfig::hwInfo;
const uint64_t MTL::defaultHardwareInfoConfig = 0;

void setupMTLHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig) {
    MTL::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable);
    MtlHwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
}

void (*MTL::setupHardwareInfo)(HardwareInfo *, bool, const uint64_t) = setupMTLHardwareInfoImpl;
} // namespace NEO
