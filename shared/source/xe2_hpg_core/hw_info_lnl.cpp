/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe2_hpg_core/hw_info_lnl.h"

#include "shared/source/aub_mem_dump/definitions/aub_services.h"
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/xe2_hpg_core/hw_cmds_lnl.h"

#include "aubstream/engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_LUNARLAKE>::abbreviation = "lnl";

const PLATFORM LNL::platform = {
    IGFX_LUNARLAKE,
    PCH_UNKNOWN,
    IGFX_XE2_HPG_CORE,
    IGFX_XE2_HPG_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    0,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable LNL::capabilityTable{
    EngineDirectSubmissionInitVec{
        {aub_stream::ENGINE_CCS, {true, false, false, true}}}, // directSubmissionEngines
    {0, 0, 0, 0, false, false, false, false},                  // kmdNotifyProperties
    MemoryConstants::max48BitAddress,                          // gpuAddressSpace
    0,                                                         // sharedSystemMemCapabilities
    MemoryConstants::pageSize,                                 // requiredPreemptionSurfaceSize
    "",                                                        // deviceName
    nullptr,                                                   // preferredPlatformName
    PreemptionMode::MidThread,                                 // defaultPreemptionMode
    aub_stream::ENGINE_CCS,                                    // defaultEngineType
    0,                                                         // maxRenderFrequency
    30,                                                        // clVersionSupport
    AubMemDump::CmdServicesMemTraceVersion::DeviceValues::Lnl, // aubDeviceId
    0,                                                         // extraQuantityThreadsPerEU
    128,                                                       // maxProgrammableSlmSize
    sizeof(LNL::GRF),                                          // grfSize
    64,                                                        // timestampValidBits
    64,                                                        // kernelTimestampValidBits
    false,                                                     // blitterOperationsSupported
    true,                                                      // ftrSupportsInteger64BitAtomics
    true,                                                      // ftrSupportsFP64
    false,                                                     // ftrSupportsFP64Emulation
    true,                                                      // ftrSupports64BitMath
    false,                                                     // ftrSupportsCoherency
    false,                                                     // ftrRenderCompressedBuffers
    false,                                                     // ftrRenderCompressedImages
    true,                                                      // instrumentationEnabled
    false,                                                     // supportCacheFlushAfterWalker
    true,                                                      // supportsImages
    true,                                                      // supportsOcl21Features
    true,                                                      // supportsOnDemandPageFaults
    true,                                                      // supportsIndependentForwardProgress
    true,                                                      // isIntegratedDevice
    false,                                                     // supportsMediaBlock
    false,                                                     // p2pAccessSupported
    false,                                                     // p2pAtomicAccessSupported
    false,                                                     // fusedEuEnabled
    true,                                                      // l0DebuggerSupported;
    true,                                                      // supportsFloatAtomics
    0                                                          // cxlType
};

void LNL::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo, const ReleaseHelper &releaseHelper) {
    setupDefaultFeatureTableAndWorkaroundTable(hwInfo, releaseHelper);
    FeatureTable *featureTable = &hwInfo->featureTable;

    featureTable->flags.ftrFlatPhysCCS = true;
    featureTable->flags.ftrE2ECompression = true;
    featureTable->flags.ftrTile64Optimization = true;
    featureTable->flags.ftrWalkerMTP = true;
    featureTable->flags.ftrXe2PlusTiling = true;
    featureTable->flags.ftrPml5Support = true;

    featureTable->ftrBcsInfo = 1;
    hwInfo->workaroundTable.flags.wa_14018976079 = true;
    hwInfo->workaroundTable.flags.wa_14018984349 = true;
}

FeatureTable LNL::featureTable{};
WorkaroundTable LNL::workaroundTable{};

const HardwareInfo LnlHwConfig::hwInfo = {
    &LNL::platform,
    &LNL::featureTable,
    &LNL::workaroundTable,
    &LnlHwConfig::gtSystemInfo,
    LNL::capabilityTable};

GT_SYSTEM_INFO LnlHwConfig::gtSystemInfo = {};

const HardwareInfo LNL::hwInfo = LnlHwConfig::hwInfo;

void LNL::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    setupDefaultGtSysInfo(hwInfo, releaseHelper);

    LNL::adjustHardwareInfo(hwInfo);
    if (setupFeatureTableAndWorkaroundTable) {
        LNL::setupFeatureAndWorkaroundTable(hwInfo, *releaseHelper);
    }
}
void LnlHwConfig::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    LNL::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
}

void setupLNLHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig, const ReleaseHelper *releaseHelper) {
    LnlHwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
}

void (*LNL::setupHardwareInfo)(HardwareInfo *, bool, uint64_t, const ReleaseHelper *) = setupLNLHardwareInfoImpl;
} // namespace NEO
