/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/release_helpers/caps/caps_setup.h"
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

void ARL::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
    setupDefaultFeatureTableAndWorkaroundTable(hwInfo);
    FeatureTable *featureTable = &hwInfo->featureTable;
    WorkaroundTable *workaroundTable = &hwInfo->workaroundTable;

    featureTable->flags.ftrTile64Optimization = true;
    featureTable->ftrBcsInfo = 1;

    workaroundTable->flags.waUntypedBufferCompression = true;
};

void ARL::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    setupDefaultGtSysInfo(hwInfo);

    hwInfo->gtSystemInfo.NumThreadsPerEu = 8u;
    hwInfo->gtSystemInfo.ThreadCount = hwInfo->gtSystemInfo.EUCount * hwInfo->gtSystemInfo.NumThreadsPerEu;

    setupCaps(*hwInfo);
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }

    applyDebugOverrides(*hwInfo);
}

const HardwareInfo ARL::hwInfo = {
    &ARL::platform,
    &ARL::featureTable,
    &ARL::workaroundTable,
    &ARL::gtSystemInfo,
    ARL::capabilityTable};

GT_SYSTEM_INFO ARL::gtSystemInfo = {0};
void ARL::setupHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    ARL::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable);

    if (setupFeatureTableAndWorkaroundTable) {
        ARL::setupFeatureAndWorkaroundTable(hwInfo);
    }
};

void (*ARL::setupHardwareInfo)(HardwareInfo *, bool) = ARL::setupHardwareInfoImpl;
} // namespace NEO
