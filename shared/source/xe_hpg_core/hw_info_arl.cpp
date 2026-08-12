/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_arl.h"

#include "aubstream/engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_ARROWLAKE>::abbreviation = "arl";

const PLATFORM ARL::platform = {
    IGFX_ARROWLAKE,
    PCH_UNKNOWN,
    IGFX_XE_HPG_CORE,
    IGFX_XE_HPG_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    0,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable ARL::capabilityTable{
    .directSubmissionEngines = makeDirectSubmissionPropertiesPerEngine({
        {aub_stream::ENGINE_CCS, {.engineSupported = true, .submitOnInit = false, .useNonDefault = false, .useRootDevice = true}},
    }),
    .kmdNotifyProperties = {0, 0, 0, 0, false, false, false, false},
    .gpuAddressSpace = MemoryConstants::max48BitAddress,
    .sharedSystemMemCapabilities = 0,
    .requiredPreemptionSurfaceSize = MemoryConstants::pageSize,
    .deviceName = "",
    .preferredPlatformName = nullptr,
    .defaultPreemptionMode = PreemptionMode::ThreadGroup,
    .defaultEngineType = aub_stream::ENGINE_CCS,
    .maxRenderFrequency = 0,
    .extraQuantityThreadsPerEU = 0,
    .maxProgrammableSlmSize = 64,
    .grfSize = sizeof(ARL::GRF),
    .timestampValidBits = 36u,
    .kernelTimestampValidBits = 32u,
    .blitterOperationsSupported = false,
    .ftrSupportsFP64 = true,
    .ftrSupportsFP64Emulation = false,
    .ftrSupports64BitMath = true,
    .ftrSupportsCoherency = false,
    .ftrRenderCompressedBuffers = false,
    .ftrRenderCompressedImages = false,
    .instrumentationEnabled = true,
    .supportCacheFlushAfterWalker = true,
    .supportsImages = true,
    .supportsOnDemandPageFaults = false,
    .supportsIndependentForwardProgress = false,
    .isIntegratedDevice = true,
    .supportsMediaBlock = true,
    .fusedEuEnabled = true,
    .l0DebuggerSupported = true,
};

WorkaroundTable ARL::workaroundTable = {};
FeatureTable ARL::featureTable = {};

void ARL::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo, const CompilerReleaseHelper &compilerReleaseHelper) {
    setupDefaultFeatureTableAndWorkaroundTable(hwInfo, compilerReleaseHelper);
    FeatureTable *featureTable = &hwInfo->featureTable;
    WorkaroundTable *workaroundTable = &hwInfo->workaroundTable;

    featureTable->flags.ftrTile64Optimization = true;
    featureTable->ftrBcsInfo = 1;

    workaroundTable->flags.waUntypedBufferCompression = true;
};

void ARL::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const CompilerReleaseHelper *compilerReleaseHelper) {
    setupDefaultGtSysInfo(hwInfo);

    hwInfo->gtSystemInfo.NumThreadsPerEu = 8u;
    hwInfo->gtSystemInfo.ThreadCount = hwInfo->gtSystemInfo.EUCount * hwInfo->gtSystemInfo.NumThreadsPerEu;

    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo, *compilerReleaseHelper);
    }

    applyDebugOverrides(*hwInfo);
}

const HardwareInfo ArlHwConfig::hwInfo = {
    &ARL::platform,
    &ARL::featureTable,
    &ARL::workaroundTable,
    &ArlHwConfig::gtSystemInfo,
    ARL::capabilityTable};

GT_SYSTEM_INFO ArlHwConfig::gtSystemInfo = {0};
void ArlHwConfig::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const CompilerReleaseHelper *compilerReleaseHelper) {
    ARL::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable, compilerReleaseHelper);

    if (setupFeatureTableAndWorkaroundTable) {
        ARL::setupFeatureAndWorkaroundTable(hwInfo, *compilerReleaseHelper);
    }
};

const HardwareInfo ARL::hwInfo = ArlHwConfig::hwInfo;

void setupARLHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig, const CompilerReleaseHelper *compilerReleaseHelper) {
    ArlHwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, compilerReleaseHelper);
}

void (*ARL::setupHardwareInfo)(HardwareInfo *, bool, uint64_t, const CompilerReleaseHelper *) = setupARLHardwareInfoImpl;
} // namespace NEO
