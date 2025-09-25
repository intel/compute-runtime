/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3_core/hw_info_ptl.h"

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/xe3_core/hw_cmds_ptl.h"

#include "aubstream/engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_PTL>::abbreviation = "ptl";

const PLATFORM PTL::platform = {
    IGFX_PTL,
    PCH_UNKNOWN,
    IGFX_XE3_CORE,
    IGFX_XE3_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    4,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable PTL::capabilityTable{
    .directSubmissionEngines = makeDirectSubmissionPropertiesPerEngine({
        {aub_stream::ENGINE_CCS, {.engineSupported = true, .submitOnInit = false, .useNonDefault = false, .useRootDevice = true}},
        {aub_stream::ENGINE_BCS, {.engineSupported = true, .submitOnInit = false, .useNonDefault = true, .useRootDevice = true}},
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
    .clVersionSupport = 30,
    .extraQuantityThreadsPerEU = 0,
    .maxProgrammableSlmSize = 128,
    .grfSize = sizeof(PTL::GRF),
    .timestampValidBits = 64,
    .kernelTimestampValidBits = 64,
    .blitterOperationsSupported = false,
    .ftrSupportsInteger64BitAtomics = true,
    .ftrSupportsFP64 = true,
    .ftrSupportsFP64Emulation = false,
    .ftrSupports64BitMath = true,
    .ftrSupportsCoherency = false,
    .ftrRenderCompressedBuffers = false,
    .ftrRenderCompressedImages = false,
    .instrumentationEnabled = true,
    .supportCacheFlushAfterWalker = false,
    .supportsImages = true,
    .supportsOcl21Features = true,
    .supportsOnDemandPageFaults = true,
    .supportsIndependentForwardProgress = true,
    .isIntegratedDevice = true,
    .supportsMediaBlock = false,
    .fusedEuEnabled = false,
    .l0DebuggerSupported = true,
    .supportsFloatAtomics = true,
    .cxlType = 0};

void PTL::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo, const ReleaseHelper &releaseHelper) {
    setupDefaultFeatureTableAndWorkaroundTable(hwInfo, releaseHelper);
    FeatureTable *featureTable = &hwInfo->featureTable;

    featureTable->flags.ftrE2ECompression = true;
    featureTable->flags.ftrFlatPhysCCS = true;
    featureTable->flags.ftrWalkerMTP = true;
    featureTable->flags.ftrTile64Optimization = true;
    featureTable->flags.ftrXe2PlusTiling = true;
    featureTable->flags.ftrPml5Support = true;

    featureTable->ftrBcsInfo = 1;
    hwInfo->workaroundTable.flags.wa_14018976079 = true;
    hwInfo->workaroundTable.flags.wa_14018984349 = true;
}

void PTL::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    setupDefaultGtSysInfo(hwInfo);

    hwInfo->gtSystemInfo.NumThreadsPerEu = 10u;
    hwInfo->gtSystemInfo.ThreadCount = hwInfo->gtSystemInfo.EUCount * hwInfo->gtSystemInfo.NumThreadsPerEu;

    adjustHardwareInfo(hwInfo);
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo, *releaseHelper);
    }

    applyDebugOverrides(*hwInfo);
}

FeatureTable PTL::featureTable{};
WorkaroundTable PTL::workaroundTable{};

const HardwareInfo PtlHwConfig::hwInfo = {
    &PTL::platform,
    &PTL::featureTable,
    &PTL::workaroundTable,
    &PtlHwConfig::gtSystemInfo,
    PTL::capabilityTable};

GT_SYSTEM_INFO PtlHwConfig::gtSystemInfo = {0};
void PtlHwConfig::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    PTL::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
}

const HardwareInfo PTL::hwInfo = PtlHwConfig::hwInfo;

void setupPTLHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig, const ReleaseHelper *releaseHelper) {
    PtlHwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
}

void (*PTL::setupHardwareInfo)(HardwareInfo *, bool, uint64_t, const ReleaseHelper *) = setupPTLHardwareInfoImpl;
} // namespace NEO
