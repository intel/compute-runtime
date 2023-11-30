/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/definitions/aub_services.h"
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
    EngineDirectSubmissionInitVec{
        {aub_stream::ENGINE_CCS, {true, false, false, true}},
        {aub_stream::ENGINE_CCS1, {true, false, true, true}},
        {aub_stream::ENGINE_CCS2, {true, false, true, true}},
        {aub_stream::ENGINE_CCS3, {true, false, true, true}},
        {aub_stream::ENGINE_BCS, {true, false, true, true}},
        {aub_stream::ENGINE_BCS1, {true, false, true, true}},
        {aub_stream::ENGINE_BCS2, {true, false, true, true}},
        {aub_stream::ENGINE_BCS3, {true, false, true, true}},
        {aub_stream::ENGINE_BCS4, {true, false, true, true}},
        {aub_stream::ENGINE_BCS5, {true, false, true, true}},
        {aub_stream::ENGINE_BCS6, {true, false, true, true}},
        {aub_stream::ENGINE_BCS7, {true, false, true, true}},
        {aub_stream::ENGINE_BCS8, {true, false, true, true}}}, // directSubmissionEngines
    {0, 0, 0, 0, false, false, false, false},                  // kmdNotifyProperties
    maxNBitValue(57),                                          // gpuAddressSpace
    0,                                                         // sharedSystemMemCapabilities
    83.333,                                                    // defaultProfilingTimerResolution
    MemoryConstants::pageSize,                                 // requiredPreemptionSurfaceSize
    "",                                                        // deviceName
    nullptr,                                                   // preferredPlatformName
    PreemptionMode::ThreadGroup,                               // defaultPreemptionMode
    aub_stream::ENGINE_CCS,                                    // defaultEngineType
    0,                                                         // maxRenderFrequency
    30,                                                        // clVersionSupport
    CmdServicesMemTraceVersion::DeviceValues::Pvc,             // aubDeviceId
    0,                                                         // extraQuantityThreadsPerEU
    128,                                                       // slmSize
    sizeof(PVC::GRF),                                          // grfSize
    36u,                                                       // timestampValidBits
    32u,                                                       // kernelTimestampValidBits
    false,                                                     // blitterOperationsSupported
    true,                                                      // ftrSupportsInteger64BitAtomics
    true,                                                      // ftrSupportsFP64
    false,                                                     // ftrSupportsFP64Emulation
    true,                                                      // ftrSupports64BitMath
    true,                                                      // ftrSvm
    false,                                                     // ftrSupportsCoherency
    false,                                                     // ftrSupportsVmeAvcTextureSampler
    false,                                                     // ftrSupportsVmeAvcPreemption
    false,                                                     // ftrRenderCompressedBuffers
    false,                                                     // ftrRenderCompressedImages
    true,                                                      // ftr64KBpages
    true,                                                      // instrumentationEnabled
    false,                                                     // supportsVme
    false,                                                     // supportCacheFlushAfterWalker
    false,                                                     // supportsImages
    false,                                                     // supportsDeviceEnqueue
    false,                                                     // supportsPipes
    true,                                                      // supportsOcl21Features
    true,                                                      // supportsOnDemandPageFaults
    true,                                                      // supportsIndependentForwardProgress
    false,                                                     // hostPtrTrackingEnabled
    true,                                                      // levelZeroSupported
    false,                                                     // isIntegratedDevice
    false,                                                     // supportsMediaBlock
    true,                                                      // p2pAccessSupported
    true,                                                      // p2pAtomicAccessSupported
    false,                                                     // fusedEuEnabled
    true,                                                      // l0DebuggerSupported;
    true                                                       // supportsFloatAtomics
};

void PVC::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
    FeatureTable *featureTable = &hwInfo->featureTable;
    WorkaroundTable *workaroundTable = &hwInfo->workaroundTable;

    featureTable->flags.ftrL3IACoherency = true;
    featureTable->flags.ftrLocalMemory = true;
    featureTable->flags.ftrLinearCCS = true;
    featureTable->flags.ftrFlatPhysCCS = true;
    featureTable->flags.ftrE2ECompression = false;
    featureTable->flags.ftrCCSNode = true;
    featureTable->flags.ftrCCSRing = true;
    featureTable->flags.ftrMultiTileArch = true;

    featureTable->flags.ftrPPGTT = true;
    featureTable->flags.ftrSVM = true;
    featureTable->flags.ftrL3IACoherency = true;
    featureTable->flags.ftrIA32eGfxPTEs = true;
    featureTable->flags.ftrStandardMipTailFormat = true;
    featureTable->flags.ftrTranslationTable = true;
    featureTable->flags.ftrUserModeTranslationTable = true;
    featureTable->flags.ftrTileMappedResource = true;
    featureTable->flags.ftrFbc = true;
    featureTable->flags.ftrAstcHdr2D = true;
    featureTable->flags.ftrAstcLdr2D = true;

    featureTable->flags.ftrGpGpuMidBatchPreempt = true;
    featureTable->flags.ftrGpGpuThreadGroupLevelPreempt = true;

    featureTable->flags.ftrTileY = false;
    featureTable->ftrBcsInfo = maxNBitValue(9);
    workaroundTable->flags.wa4kAlignUVOffsetNV12LinearSurface = true;
}

void PVC::adjustHardwareInfo(HardwareInfo *hwInfo) {
    hwInfo->capabilityTable.sharedSystemMemCapabilities = (UNIFIED_SHARED_MEMORY_ACCESS | UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS | UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS);
}

void PVC::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * releaseHelper->getNumThreadsPerEu();
    gtSysInfo->MaxFillRate = 128;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = PVC::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = PVC::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = PVC::maxSubslicesSupported;
    gtSysInfo->MaxDualSubSlicesSupported = PVC::maxDualSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;

    PVC::adjustHardwareInfo(hwInfo);

    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
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

FeatureTable PVC::featureTable;
WorkaroundTable PVC::workaroundTable;

const HardwareInfo PvcHwConfig::hwInfo = {
    &PVC::platform,
    &PVC::featureTable,
    &PVC::workaroundTable,
    &PvcHwConfig::gtSystemInfo,
    PVC::capabilityTable};

GT_SYSTEM_INFO PvcHwConfig::gtSystemInfo = {0};
void PvcHwConfig::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    PVC::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;

    // non-zero values for unit tests
    if (gtSysInfo->SliceCount == 0) {
        setupHardwareInfoMultiTileBase(hwInfo, true);
        gtSysInfo->SliceCount = 2;
        gtSysInfo->SubSliceCount = 8;
        gtSysInfo->DualSubSliceCount = gtSysInfo->SubSliceCount;
        gtSysInfo->EUCount = 40;
        gtSysInfo->MaxEuPerSubSlice = gtSysInfo->EUCount / gtSysInfo->SubSliceCount;
        gtSysInfo->MaxSlicesSupported = gtSysInfo->SliceCount;
        gtSysInfo->MaxSubSlicesSupported = gtSysInfo->SubSliceCount;

        gtSysInfo->L3CacheSizeInKb = 1;
        gtSysInfo->L3BankCount = 1;

        gtSysInfo->CCSInfo.IsValid = true;
        gtSysInfo->CCSInfo.NumberOfCCSEnabled = 1;
        gtSysInfo->CCSInfo.Instances.CCSEnableMask = 0b1;

        hwInfo->featureTable.ftrBcsInfo = 1;
        gtSysInfo->IsDynamicallyPopulated = true;
        for (uint32_t slice = 0; slice < gtSysInfo->SliceCount; slice++) {
            gtSysInfo->SliceInfo[slice].Enabled = true;
        }
    }

    if (setupFeatureTableAndWorkaroundTable) {
        PVC::setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo PVC::hwInfo = PvcHwConfig::hwInfo;

void setupPVCHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig, const ReleaseHelper *releaseHelper) {
    PvcHwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
}

void (*PVC::setupHardwareInfo)(HardwareInfo *, bool, uint64_t, const ReleaseHelper *) = setupPVCHardwareInfoImpl;
} // namespace NEO
