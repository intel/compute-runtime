/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3p_core/hw_info_nvlp.h"

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/xe3p_core/hw_cmds_nvlp.h"

#include "aubstream/engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_NVL>::abbreviation = "nvlp";

const PLATFORM NVLP::platform = {
    IGFX_NVL,
    PCH_UNKNOWN,
    IGFX_XE3P_CORE,
    IGFX_XE3P_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    4,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable NVLP::capabilityTable{
    .directSubmissionEngines = makeDirectSubmissionPropertiesPerEngine({
        {aub_stream::ENGINE_CCS, {.engineSupported = true, .submitOnInit = false, .useNonDefault = false, .useRootDevice = true}},
    }),
    .kmdNotifyProperties = {0, 0, 0, 0, false, false, false, false},
    .gpuAddressSpace = MemoryConstants::max48BitAddress,
    .sharedSystemMemCapabilities = 0,
    .requiredPreemptionSurfaceSize = MemoryConstants::pageSize,
    .deviceName = "",
    .preferredPlatformName = nullptr,
    .defaultPreemptionMode = PreemptionMode::MidThread,
    .defaultEngineType = aub_stream::ENGINE_CCS,
    .maxRenderFrequency = 0,
    .extraQuantityThreadsPerEU = 0,
    .maxProgrammableSlmSize = 192,
    .grfSize = sizeof(NVLP::GRF),
    .timestampValidBits = 64,
    .kernelTimestampValidBits = 64,
    .blitterOperationsSupported = false,
    .ftrSupportsFP64 = true,
    .ftrSupportsFP64Emulation = false,
    .ftrSupports64BitMath = true,
    .ftrSupportsCoherency = false,
    .ftrRenderCompressedBuffers = false,
    .ftrRenderCompressedImages = false,
    .instrumentationEnabled = true,
    .supportCacheFlushAfterWalker = false,
    .supportsImages = true,
    .supportsOnDemandPageFaults = true,
    .supportsIndependentForwardProgress = true,
    .isIntegratedDevice = true,
    .supportsMediaBlock = false,
    .fusedEuEnabled = false,
    .l0DebuggerSupported = true,
};

void NVLP::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo, const ReleaseHelper &releaseHelper) {
    setupDefaultFeatureTableAndWorkaroundTable(hwInfo, releaseHelper);
    FeatureTable *featureTable = &hwInfo->featureTable;

    featureTable->flags.ftrFlatPhysCCS = true;
    featureTable->flags.ftrE2ECompression = true;
    featureTable->flags.ftrTile64Optimization = true;
    featureTable->flags.ftrWalkerMTP = true;
    featureTable->flags.ftrSelectiveWmtp = true;
    featureTable->flags.ftrXe2PlusTiling = true;
    featureTable->flags.ftrL3TransientDataFlush = true;
    featureTable->flags.ftrPml5Support = true;

    featureTable->ftrBcsInfo = 1;
}

void NVLP::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    setupDefaultGtSysInfo(hwInfo);

    hwInfo->gtSystemInfo.NumThreadsPerEu = 10u;
    hwInfo->gtSystemInfo.ThreadCount = hwInfo->gtSystemInfo.EUCount * hwInfo->gtSystemInfo.NumThreadsPerEu;

    hwInfo->featureTable.flags.ftrHeaplessMode = true;

    adjustHardwareInfo(hwInfo);
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo, *releaseHelper);
    }

    applyDebugOverrides(*hwInfo);
}

FeatureTable NVLP::featureTable{};
WorkaroundTable NVLP::workaroundTable{};

const HardwareInfo NvlHwConfig::hwInfo = {
    &NVLP::platform,
    &NVLP::featureTable,
    &NVLP::workaroundTable,
    &NvlHwConfig::gtSystemInfo,
    NVLP::capabilityTable};

GT_SYSTEM_INFO NvlHwConfig::gtSystemInfo = {0};
void NvlHwConfig::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    NVLP::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
}

const HardwareInfo NVLP::hwInfo = NvlHwConfig::hwInfo;

void setupNVLHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig, const ReleaseHelper *releaseHelper) {
    NvlHwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
}

void (*NVLP::setupHardwareInfo)(HardwareInfo *, bool, uint64_t, const ReleaseHelper *) = setupNVLHardwareInfoImpl;
} // namespace NEO
