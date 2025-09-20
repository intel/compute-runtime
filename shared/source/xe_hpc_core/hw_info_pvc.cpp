/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/unified_memory/usm_memory_support.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/source/xe_hpc_core/pvc/device_ids_configs_pvc.h"

#include "aubstream/engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_PVC>::abbreviation = "pvc";

const PLATFORM PVC::platform = {
    IGFX_PVC,
    PCH_UNKNOWN,
    IGFX_XE_HPC_CORE,
    IGFX_XE_HPC_CORE,
    PLATFORM_NONE,     // default init
    pvcXtDeviceIds[0], // usDeviceID
    47,                // usRevId
    0,                 // usDeviceID_PCH
    0,                 // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable PVC::capabilityTable{
    .directSubmissionEngines = makeDirectSubmissionPropertiesPerEngine({
        {aub_stream::ENGINE_CCS, {.engineSupported = true, .submitOnInit = false, .useNonDefault = false, .useRootDevice = true}},
        {aub_stream::ENGINE_CCS1, {.engineSupported = true, .submitOnInit = false, .useNonDefault = true, .useRootDevice = true}},
        {aub_stream::ENGINE_CCS2, {.engineSupported = true, .submitOnInit = false, .useNonDefault = true, .useRootDevice = true}},
        {aub_stream::ENGINE_CCS3, {.engineSupported = true, .submitOnInit = false, .useNonDefault = true, .useRootDevice = true}},
        {aub_stream::ENGINE_BCS, {.engineSupported = true, .submitOnInit = false, .useNonDefault = true, .useRootDevice = true}},
        {aub_stream::ENGINE_BCS1, {.engineSupported = true, .submitOnInit = false, .useNonDefault = true, .useRootDevice = true}},
        {aub_stream::ENGINE_BCS2, {.engineSupported = true, .submitOnInit = false, .useNonDefault = true, .useRootDevice = true}},
        {aub_stream::ENGINE_BCS3, {.engineSupported = true, .submitOnInit = false, .useNonDefault = true, .useRootDevice = true}},
        {aub_stream::ENGINE_BCS4, {.engineSupported = true, .submitOnInit = false, .useNonDefault = true, .useRootDevice = true}},
        {aub_stream::ENGINE_BCS5, {.engineSupported = true, .submitOnInit = false, .useNonDefault = true, .useRootDevice = true}},
        {aub_stream::ENGINE_BCS6, {.engineSupported = true, .submitOnInit = false, .useNonDefault = true, .useRootDevice = true}},
        {aub_stream::ENGINE_BCS7, {.engineSupported = true, .submitOnInit = false, .useNonDefault = true, .useRootDevice = true}},
        {aub_stream::ENGINE_BCS8, {.engineSupported = true, .submitOnInit = false, .useNonDefault = true, .useRootDevice = true}},
    }),
    .kmdNotifyProperties = {0, 0, 0, 0, false, false, false, false},
    .gpuAddressSpace = maxNBitValue(57),
    .sharedSystemMemCapabilities = 0,
    .requiredPreemptionSurfaceSize = MemoryConstants::pageSize,
    .deviceName = "",
    .preferredPlatformName = nullptr,
    .defaultPreemptionMode = PreemptionMode::ThreadGroup,
    .defaultEngineType = aub_stream::ENGINE_CCS,
    .maxRenderFrequency = 0,
    .clVersionSupport = 30,
    .extraQuantityThreadsPerEU = 0,
    .maxProgrammableSlmSize = 128,
    .grfSize = sizeof(PVC::GRF),
    .timestampValidBits = 36u,
    .kernelTimestampValidBits = 32u,
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
    .supportsImages = false,
    .supportsOcl21Features = true,
    .supportsOnDemandPageFaults = true,
    .supportsIndependentForwardProgress = true,
    .isIntegratedDevice = false,
    .supportsMediaBlock = false,
    .fusedEuEnabled = false,
    .l0DebuggerSupported = true,
    .supportsFloatAtomics = true,
    .cxlType = 0};

void PVC::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo, const ReleaseHelper &releaseHelper) {
    setupDefaultFeatureTableAndWorkaroundTable(hwInfo, releaseHelper);
    FeatureTable *featureTable = &hwInfo->featureTable;

    featureTable->flags.ftrLocalMemory = true;
    featureTable->flags.ftrFlatPhysCCS = true;
    featureTable->flags.ftrMultiTileArch = true;

    featureTable->ftrBcsInfo = maxNBitValue(9);
}

void PVC::adjustHardwareInfo(HardwareInfo *hwInfo) {
    hwInfo->capabilityTable.sharedSystemMemCapabilities = (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess);
}

void PVC::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    setupDefaultGtSysInfo(hwInfo);
    setupHardwareInfoMultiTileBase(hwInfo, true);

    hwInfo->gtSystemInfo.NumThreadsPerEu = 8u;
    hwInfo->gtSystemInfo.ThreadCount = hwInfo->gtSystemInfo.EUCount * hwInfo->gtSystemInfo.NumThreadsPerEu;

    PVC::adjustHardwareInfo(hwInfo);

    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo, *releaseHelper);
    }

    applyDebugOverrides(*hwInfo);
}

void PVC::setupHardwareInfoMultiTileBase(HardwareInfo *hwInfo, bool setupMultiTile) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->MultiTileArchInfo.IsValid = setupMultiTile;
    gtSysInfo->MultiTileArchInfo.TileCount = 1;
    if (debugManager.flags.CreateMultipleSubDevices.get() > 0) {
        gtSysInfo->MultiTileArchInfo.TileCount = debugManager.flags.CreateMultipleSubDevices.get();
    }
    gtSysInfo->MultiTileArchInfo.TileMask = static_cast<uint8_t>(maxNBitValue(gtSysInfo->MultiTileArchInfo.TileCount));
}

FeatureTable PVC::featureTable{};
WorkaroundTable PVC::workaroundTable{};

const HardwareInfo PvcHwConfig::hwInfo = {
    &PVC::platform,
    &PVC::featureTable,
    &PVC::workaroundTable,
    &PvcHwConfig::gtSystemInfo,
    PVC::capabilityTable};

GT_SYSTEM_INFO PvcHwConfig::gtSystemInfo = {0};
void PvcHwConfig::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    PVC::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
};

const HardwareInfo PVC::hwInfo = PvcHwConfig::hwInfo;

void setupPVCHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig, const ReleaseHelper *releaseHelper) {
    PvcHwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
}

void (*PVC::setupHardwareInfo)(HardwareInfo *, bool, uint64_t, const ReleaseHelper *) = setupPVCHardwareInfoImpl;
} // namespace NEO
