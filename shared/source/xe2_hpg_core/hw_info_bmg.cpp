/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe2_hpg_core/hw_info_bmg.h"

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/xe2_hpg_core/hw_cmds_bmg.h"

#include "aubstream/engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_BMG>::abbreviation = "bmg";

const PLATFORM BMG::platform = {
    IGFX_BMG,
    PCH_UNKNOWN,
    IGFX_XE2_HPG_CORE,
    IGFX_XE2_HPG_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    0,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable BMG::capabilityTable{
    .directSubmissionEngines = makeDirectSubmissionPropertiesPerEngine({
        {aub_stream::ENGINE_CCS, {.engineSupported = true, .submitOnInit = false, .useNonDefault = false, .useRootDevice = true}},
        {aub_stream::ENGINE_CCS1, {.engineSupported = true, .submitOnInit = false, .useNonDefault = true, .useRootDevice = true}},
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
    .grfSize = sizeof(BMG::GRF),
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
    .isIntegratedDevice = false,
    .supportsMediaBlock = false,
    .fusedEuEnabled = false,
    .l0DebuggerSupported = true,
    .supportsFloatAtomics = true,
    .cxlType = 0};

void BMG::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo, const ReleaseHelper &releaseHelper) {
    setupDefaultFeatureTableAndWorkaroundTable(hwInfo, releaseHelper);
    FeatureTable *featureTable = &hwInfo->featureTable;

    featureTable->flags.ftrLocalMemory = true;
    featureTable->flags.ftrFlatPhysCCS = true;
    featureTable->flags.ftrE2ECompression = true;
    featureTable->flags.ftrTile64Optimization = true;
    featureTable->flags.ftrWalkerMTP = true;
    featureTable->flags.ftrXe2PlusTiling = true;
    featureTable->flags.ftrL3TransientDataFlush = true;
    featureTable->flags.ftrPml5Support = true;

    featureTable->ftrBcsInfo = 1;
}

FeatureTable BMG::featureTable{};
WorkaroundTable BMG::workaroundTable{};

const HardwareInfo BmgHwConfig::hwInfo = {
    &BMG::platform,
    &BMG::featureTable,
    &BMG::workaroundTable,
    &BmgHwConfig::gtSystemInfo,
    BMG::capabilityTable};

GT_SYSTEM_INFO BmgHwConfig::gtSystemInfo = {};

const HardwareInfo BMG::hwInfo = BmgHwConfig::hwInfo;

void BMG::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    setupDefaultGtSysInfo(hwInfo);

    hwInfo->gtSystemInfo.NumThreadsPerEu = 8u;
    hwInfo->gtSystemInfo.ThreadCount = hwInfo->gtSystemInfo.EUCount * hwInfo->gtSystemInfo.NumThreadsPerEu;

    BMG::adjustHardwareInfo(hwInfo);
    if (setupFeatureTableAndWorkaroundTable) {
        BMG::setupFeatureAndWorkaroundTable(hwInfo, *releaseHelper);
    }

    applyDebugOverrides(*hwInfo);
}

void BmgHwConfig::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    BMG::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
}

void setupBMGHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig, const ReleaseHelper *releaseHelper) {
    BmgHwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
}

void (*BMG::setupHardwareInfo)(HardwareInfo *, bool, uint64_t, const ReleaseHelper *) = setupBMGHardwareInfoImpl;
} // namespace NEO
