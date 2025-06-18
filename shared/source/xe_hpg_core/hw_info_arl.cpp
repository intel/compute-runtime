/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/definitions/aub_services.h"
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/release_helper/release_helper.h"
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
    AubMemDump::CmdServicesMemTraceVersion::DeviceValues::Arl, // aubDeviceId
    0,                                                         // extraQuantityThreadsPerEU
    64,                                                        // maxProgrammableSlmSize
    sizeof(ARL::GRF),                                          // grfSize
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

WorkaroundTable ARL::workaroundTable = {};
FeatureTable ARL::featureTable = {};

void ARL::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo, const ReleaseHelper &releaseHelper) {
    setupDefaultFeatureTableAndWorkaroundTable(hwInfo, releaseHelper);
    FeatureTable *featureTable = &hwInfo->featureTable;
    WorkaroundTable *workaroundTable = &hwInfo->workaroundTable;

    featureTable->flags.ftrTile64Optimization = true;
    featureTable->ftrBcsInfo = 1;

    workaroundTable->flags.waUntypedBufferCompression = true;
};

void ARL::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    setupDefaultGtSysInfo(hwInfo, releaseHelper);

    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo, *releaseHelper);
    }
}

const HardwareInfo ArlHwConfig::hwInfo = {
    &ARL::platform,
    &ARL::featureTable,
    &ARL::workaroundTable,
    &ArlHwConfig::gtSystemInfo,
    ARL::capabilityTable};

GT_SYSTEM_INFO ArlHwConfig::gtSystemInfo = {0};
void ArlHwConfig::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    ARL::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);

    if (setupFeatureTableAndWorkaroundTable) {
        ARL::setupFeatureAndWorkaroundTable(hwInfo, *releaseHelper);
    }
};

const HardwareInfo ARL::hwInfo = ArlHwConfig::hwInfo;

void setupARLHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig, const ReleaseHelper *releaseHelper) {
    ArlHwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
}

void (*ARL::setupHardwareInfo)(HardwareInfo *, bool, uint64_t, const ReleaseHelper *) = setupARLHardwareInfoImpl;
} // namespace NEO
