/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/definitions/aub_services.h"
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"

#include "aubstream/engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_DG2>::abbreviation = "dg2";

const PLATFORM DG2::platform = {
    IGFX_DG2,
    PCH_UNKNOWN,
    IGFX_XE_HPG_CORE,
    IGFX_XE_HPG_CORE,
    PLATFORM_NONE,      // default init
    dg2G10DeviceIds[0], // usDeviceID
    0,                  // usRevId. 0 sets the stepping to A0
    0,                  // usDeviceID_PCH
    0,                  // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable DG2::capabilityTable{
    EngineDirectSubmissionInitVec{
        {aub_stream::ENGINE_RCS, {false, false, false, false}},
        {aub_stream::ENGINE_CCS, {true, false, false, true}},
        {aub_stream::ENGINE_CCS1, {true, false, true, true}},
        {aub_stream::ENGINE_CCS2, {true, false, true, true}},
        {aub_stream::ENGINE_CCS3, {true, false, true, true}}}, // directSubmissionEngines
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
    AubMemDump::CmdServicesMemTraceVersion::DeviceValues::Dg2, // aubDeviceId
    0,                                                         // extraQuantityThreadsPerEU
    64,                                                        // maxProgrammableSlmSize
    sizeof(DG2::GRF),                                          // grfSize
    36u,                                                       // timestampValidBits
    32u,                                                       // kernelTimestampValidBits
    false,                                                     // blitterOperationsSupported
    true,                                                      // ftrSupportsInteger64BitAtomics
    false,                                                     // ftrSupportsFP64
    true,                                                      // ftrSupportsFP64Emulation
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
    false,                                                     // isIntegratedDevice
    true,                                                      // supportsMediaBlock
    true,                                                      // p2pAccessSupported
    false,                                                     // p2pAtomicAccessSupported
    true,                                                      // fusedEuEnabled
    true,                                                      // l0DebuggerSupported
    true,                                                      // supportsFloatAtomics
    0                                                          // cxlType
};

WorkaroundTable DG2::workaroundTable = {};
FeatureTable DG2::featureTable = {};

void DG2::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo, const ReleaseHelper &releaseHelper) {
    setupDefaultFeatureTableAndWorkaroundTable(hwInfo, releaseHelper);
    FeatureTable *featureTable = &hwInfo->featureTable;

    featureTable->flags.ftrFlatPhysCCS = true;
    featureTable->flags.ftrLocalMemory = true;
    featureTable->flags.ftrE2ECompression = true;
    featureTable->flags.ftrUnified3DMediaCompressionFormats = true;
    featureTable->flags.ftrTile64Optimization = true;
    featureTable->ftrBcsInfo = 1;
};

void DG2::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    setupDefaultGtSysInfo(hwInfo, releaseHelper);

    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo, *releaseHelper);
    }
}

const HardwareInfo Dg2HwConfig::hwInfo = {
    &DG2::platform,
    &DG2::featureTable,
    &DG2::workaroundTable,
    &Dg2HwConfig::gtSystemInfo,
    DG2::capabilityTable};

GT_SYSTEM_INFO Dg2HwConfig::gtSystemInfo = {0};
void Dg2HwConfig::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    DG2::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
};

const HardwareInfo DG2::hwInfo = Dg2HwConfig::hwInfo;

void setupDG2HardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig, const ReleaseHelper *releaseHelper) {
    Dg2HwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
}

void (*DG2::setupHardwareInfo)(HardwareInfo *, bool, uint64_t, const ReleaseHelper *) = setupDG2HardwareInfoImpl;
} // namespace NEO
