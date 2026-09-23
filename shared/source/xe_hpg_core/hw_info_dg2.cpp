/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/release_helpers/caps/caps_setup.h"
#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"
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
    .directSubmissionEngines = makeDirectSubmissionPropertiesPerEngine({
        {aub_stream::ENGINE_RCS, {.engineSupported = false, .submitOnInit = false, .useNonDefault = false, .useRootDevice = false}},
        {aub_stream::ENGINE_CCS, {.engineSupported = true, .submitOnInit = false, .useNonDefault = false, .useRootDevice = true}},
        {aub_stream::ENGINE_CCS1, {.engineSupported = true, .submitOnInit = false, .useNonDefault = true, .useRootDevice = true}},
        {aub_stream::ENGINE_CCS2, {.engineSupported = true, .submitOnInit = false, .useNonDefault = true, .useRootDevice = true}},
        {aub_stream::ENGINE_CCS3, {.engineSupported = true, .submitOnInit = false, .useNonDefault = true, .useRootDevice = true}},

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
    .grfSize = sizeof(DG2::GRF),
    .timestampValidBits = 36u,
    .kernelTimestampValidBits = 32u,
    .blitterOperationsSupported = false,
    .ftrSupportsFP64 = false,
    .ftrSupportsFP64Emulation = true,
    .ftrSupports64BitMath = true,
    .ftrSupportsCoherency = false,
    .ftrRenderCompressedBuffers = false,
    .ftrRenderCompressedImages = false,
    .instrumentationEnabled = true,
    .supportCacheFlushAfterWalker = true,
    .supportsImages = true,
    .supportsOnDemandPageFaults = false,
    .supportsIndependentForwardProgress = false,
    .isIntegratedDevice = false,
    .supportsMediaBlock = true,
    .fusedEuEnabled = true,
    .l0DebuggerSupported = true,
};

WorkaroundTable DG2::workaroundTable = {};
FeatureTable DG2::featureTable = {};

void DG2::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
    setupDefaultFeatureTableAndWorkaroundTable(hwInfo);
    FeatureTable *featureTable = &hwInfo->featureTable;

    featureTable->flags.ftrFlatPhysCCS = true;
    featureTable->flags.ftrLocalMemory = true;
    featureTable->flags.ftrE2ECompression = true;
    featureTable->flags.ftrUnified3DMediaCompressionFormats = true;
    featureTable->flags.ftrTile64Optimization = true;
    featureTable->ftrBcsInfo = 1;
};

void DG2::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    setupDefaultGtSysInfo(hwInfo);

    hwInfo->gtSystemInfo.NumThreadsPerEu = 8u;
    hwInfo->gtSystemInfo.ThreadCount = hwInfo->gtSystemInfo.EUCount * hwInfo->gtSystemInfo.NumThreadsPerEu;

    setupCaps(*hwInfo);
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }

    applyDebugOverrides(*hwInfo);
}

const HardwareInfo DG2::hwInfo = {
    &DG2::platform,
    &DG2::featureTable,
    &DG2::workaroundTable,
    &DG2::gtSystemInfo,
    DG2::capabilityTable};

GT_SYSTEM_INFO DG2::gtSystemInfo = {0};
void DG2::setupHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    DG2::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable);
};

void (*DG2::setupHardwareInfo)(HardwareInfo *, bool) = DG2::setupHardwareInfoImpl;
} // namespace NEO
