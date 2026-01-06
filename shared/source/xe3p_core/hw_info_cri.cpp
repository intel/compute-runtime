/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3p_core/hw_info_cri.h"

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/unified_memory/usm_memory_support.h"
#include "shared/source/xe3p_core/hw_cmds_cri.h"

#include "aubstream/engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_CRI>::abbreviation = "cri";

const PLATFORM CRI::platform = {
    IGFX_CRI,
    PCH_UNKNOWN,
    IGFX_XE3P_CORE,
    IGFX_XE3P_CORE,
    PLATFORM_NONE, // default init
    0x674C,        // usDeviceID
    0,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable CRI::capabilityTable{
    .directSubmissionEngines = makeDirectSubmissionPropertiesPerEngine({
        {aub_stream::ENGINE_CCS, {.engineSupported = true, .submitOnInit = false, .useNonDefault = false, .useRootDevice = true}},
        {aub_stream::ENGINE_BCS1, {.engineSupported = true, .submitOnInit = false, .useNonDefault = true, .useRootDevice = true}},
        {aub_stream::ENGINE_BCS2, {.engineSupported = true, .submitOnInit = false, .useNonDefault = true, .useRootDevice = true}},
        {aub_stream::ENGINE_BCS3, {.engineSupported = true, .submitOnInit = false, .useNonDefault = true, .useRootDevice = true}},
        {aub_stream::ENGINE_BCS4, {.engineSupported = true, .submitOnInit = false, .useNonDefault = true, .useRootDevice = true}},
    }),
    .kmdNotifyProperties = {0, 0, 0, 0, false, false, false, false},
    .gpuAddressSpace = maxNBitValue(57),
    .sharedSystemMemCapabilities = 0,
    .requiredPreemptionSurfaceSize = MemoryConstants::pageSize,
    .deviceName = "",
    .preferredPlatformName = nullptr,
    .defaultPreemptionMode = PreemptionMode::MidThread,
    .defaultEngineType = aub_stream::ENGINE_CCS,
    .maxRenderFrequency = 0,
    .extraQuantityThreadsPerEU = 0,
    .maxProgrammableSlmSize = 384,
    .grfSize = sizeof(CRI::GRF),
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
    .supportsImages = false,
    .supportsOnDemandPageFaults = true,
    .supportsIndependentForwardProgress = true,
    .isIntegratedDevice = false,
    .supportsMediaBlock = false,
    .fusedEuEnabled = false,
    .l0DebuggerSupported = true,
};

void CRI::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo, const ReleaseHelper &releaseHelper) {
    setupDefaultFeatureTableAndWorkaroundTable(hwInfo, releaseHelper);
    FeatureTable *featureTable = &hwInfo->featureTable;

    featureTable->flags.ftrLocalMemory = true;
    featureTable->flags.ftrFlatPhysCCS = false;
    featureTable->flags.ftrMultiTileArch = false;
    featureTable->flags.ftrWalkerMTP = true;
    featureTable->flags.ftrXe2PlusTiling = true;
    featureTable->flags.ftrL3TransientDataFlush = true;
    featureTable->flags.ftrPml5Support = true;

    featureTable->ftrBcsInfo = maxNBitValue(9);
}

void CRI::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    setupDefaultGtSysInfo(hwInfo);

    hwInfo->gtSystemInfo.NumThreadsPerEu = 8u;
    hwInfo->gtSystemInfo.ThreadCount = hwInfo->gtSystemInfo.EUCount * hwInfo->gtSystemInfo.NumThreadsPerEu;

    hwInfo->featureTable.flags.ftrHeaplessMode = true;

    adjustHardwareInfo(hwInfo);
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo, *releaseHelper);
    }

    applyDebugOverrides(*hwInfo);
}

FeatureTable CRI::featureTable{};
WorkaroundTable CRI::workaroundTable{};

const HardwareInfo CriHwConfig::hwInfo = {
    &CRI::platform,
    &CRI::featureTable,
    &CRI::workaroundTable,
    &CriHwConfig::gtSystemInfo,
    CRI::capabilityTable};

GT_SYSTEM_INFO CriHwConfig::gtSystemInfo = {0};
void CriHwConfig::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
}

const HardwareInfo CRI::hwInfo = CriHwConfig::hwInfo;

void setupCRIHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig, const ReleaseHelper *releaseHelper) {
    CriHwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
}

void (*CRI::setupHardwareInfo)(HardwareInfo *, bool, uint64_t, const ReleaseHelper *) = setupCRIHardwareInfoImpl;
} // namespace NEO
