/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/definitions/aub_services.h"
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_mtl.h"

#include "aubstream/engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_METEORLAKE>::abbreviation = "mtl";

const PLATFORM MTL::platform = {
    IGFX_METEORLAKE,
    PCH_UNKNOWN,
    IGFX_XE_HPG_CORE,
    IGFX_XE_HPG_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    0,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable MTL::capabilityTable{
    EngineDirectSubmissionInitVec{
        {aub_stream::ENGINE_CCS, {true, false, false, true}}}, // directSubmissionEngines
    {0, 0, 0, 0, false, false, false, false},                  // kmdNotifyProperties
    MemoryConstants::max48BitAddress,                          // gpuAddressSpace
    0,                                                         // sharedSystemMemCapabilities
    MemoryConstants::pageSize,                                 // requiredPreemptionSurfaceSize
    "",                                                        // deviceName
    nullptr,                                                   // preferredPlatformName
    PreemptionMode::ThreadGroup,                               // defaultPreemptionMode
    aub_stream::ENGINE_CCS,                                    // defaultEngineType
    0,                                                         // maxRenderFrequency
    30,                                                        // clVersionSupport
    AubMemDump::CmdServicesMemTraceVersion::DeviceValues::Mtl, // aubDeviceId
    0,                                                         // extraQuantityThreadsPerEU
    64,                                                        // maxProgrammableSlmSize
    sizeof(MTL::GRF),                                          // grfSize
    36u,                                                       // timestampValidBits
    32u,                                                       // kernelTimestampValidBits
    false,                                                     // blitterOperationsSupported
    true,                                                      // ftrSupportsInteger64BitAtomics
    true,                                                      // ftrSupportsFP64
    false,                                                     // ftrSupportsFP64Emulation
    true,                                                      // ftrSupports64BitMath
    false,                                                     // ftrSupportsCoherency
    false,                                                     // ftrRenderCompressedBuffers
    false,                                                     // ftrRenderCompressedImages
    true,                                                      // instrumentationEnabled
    true,                                                      // supportCacheFlushAfterWalker
    true,                                                      // supportsImages
    true,                                                      // supportsOcl21Features
    false,                                                     // supportsOnDemandPageFaults
    false,                                                     // supportsIndependentForwardProgress
    true,                                                      // isIntegratedDevice
    true,                                                      // supportsMediaBlock
    false,                                                     // p2pAccessSupported
    false,                                                     // p2pAtomicAccessSupported
    true,                                                      // fusedEuEnabled
    true,                                                      // l0DebuggerSupported
    true,                                                      // supportsFloatAtomics
    0                                                          // cxlType
};

WorkaroundTable MTL::workaroundTable = {};
FeatureTable MTL::featureTable = {};

void MTL::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo, const ReleaseHelper &releaseHelper) {
    setupDefaultFeatureTableAndWorkaroundTable(hwInfo, releaseHelper);
    FeatureTable *featureTable = &hwInfo->featureTable;
    WorkaroundTable *workaroundTable = &hwInfo->workaroundTable;

    featureTable->flags.ftrTile64Optimization = true;
    featureTable->ftrBcsInfo = 1;

    workaroundTable->flags.waUntypedBufferCompression = true;
};

void MTL::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    setupDefaultGtSysInfo(hwInfo, releaseHelper);

    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo, *releaseHelper);
    }
}

const HardwareInfo MtlHwConfig::hwInfo = {
    &MTL::platform,
    &MTL::featureTable,
    &MTL::workaroundTable,
    &MtlHwConfig::gtSystemInfo,
    MTL::capabilityTable};

GT_SYSTEM_INFO MtlHwConfig::gtSystemInfo = {0};
void MtlHwConfig::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    MTL::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);

    if (setupFeatureTableAndWorkaroundTable) {
        MTL::setupFeatureAndWorkaroundTable(hwInfo, *releaseHelper);
    }
};

const HardwareInfo MTL::hwInfo = MtlHwConfig::hwInfo;

void setupMTLHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig, const ReleaseHelper *releaseHelper) {
    MtlHwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
}

void (*MTL::setupHardwareInfo)(HardwareInfo *, bool, uint64_t, const ReleaseHelper *) = setupMTLHardwareInfoImpl;
} // namespace NEO
