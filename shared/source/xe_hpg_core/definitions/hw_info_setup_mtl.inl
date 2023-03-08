/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {
const HardwareInfo MTL::hwInfo = MtlHwConfig::hwInfo;

void setupMTLHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig, const CompilerProductHelper &compilerProductHelper) {
    MTL::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable, compilerProductHelper);
    MtlHwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, compilerProductHelper);
}

void (*MTL::setupHardwareInfo)(HardwareInfo *, bool, uint64_t, const CompilerProductHelper &) = setupMTLHardwareInfoImpl;
} // namespace NEO
