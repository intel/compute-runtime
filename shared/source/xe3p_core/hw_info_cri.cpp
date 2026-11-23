/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3p_core/hw_info_cri.h"

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/cache_policy_from_xe3p_and_later.inl"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/enable_product.inl"
#include "shared/source/release_helpers/caps/caps_setup.h"
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
        {aub_stream::ENGINE_CCS, {.engineSupported = true, .submitOnInit = false, .useNonDefault = false}},
        {aub_stream::ENGINE_BCS1, {.engineSupported = true, .submitOnInit = false, .useNonDefault = true}},
        {aub_stream::ENGINE_BCS2, {.engineSupported = true, .submitOnInit = false, .useNonDefault = true}},
        {aub_stream::ENGINE_BCS3, {.engineSupported = true, .submitOnInit = false, .useNonDefault = true}},
        {aub_stream::ENGINE_BCS4, {.engineSupported = true, .submitOnInit = false, .useNonDefault = true}},
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
    .supportCacheFlushAfterWalker = false,
    .supportsImages = false,
    .supportsOnDemandPageFaults = true,
    .supportsIndependentForwardProgress = true,
    .isIntegratedDevice = false,
    .supportsMediaBlock = false,
    .fusedEuEnabled = false,
    .l0DebuggerSupported = true,
};

void CRI::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
    setupDefaultFeatureTableAndWorkaroundTable(hwInfo);
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

void CRI::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    setupDefaultGtSysInfo(hwInfo);

    hwInfo->gtSystemInfo.NumThreadsPerEu = 8u;
    hwInfo->gtSystemInfo.ThreadCount = hwInfo->gtSystemInfo.EUCount * hwInfo->gtSystemInfo.NumThreadsPerEu;

    hwInfo->featureTable.flags.ftrHeaplessMode = true;

    adjustHardwareInfo(hwInfo);
    setupCaps(*hwInfo);
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }

    applyDebugOverrides(*hwInfo);
}

FeatureTable CRI::featureTable{};
WorkaroundTable CRI::workaroundTable{};

const HardwareInfo CRI::hwInfo = {
    &CRI::platform,
    &CRI::featureTable,
    &CRI::workaroundTable,
    &CRI::gtSystemInfo,
    CRI::capabilityTable};

GT_SYSTEM_INFO CRI::gtSystemInfo = {0};
void CRI::setupHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable);
}

void (*CRI::setupHardwareInfo)(HardwareInfo *, bool) = CRI::setupHardwareInfoImpl;

template <>
uint32_t L1CachePolicyHelper<IGFX_CRI>::getDefaultL1CachePolicy(bool isDebuggerActive) {
    using GfxFamily = HwMapper<IGFX_CRI>::GfxFamily;
    if (isDebuggerActive) {
        return GfxFamily::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WBP;
    }
    return GfxFamily::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WS;
}

template struct L1CachePolicyHelper<IGFX_CRI>;
template EnableGfxProductHw<IGFX_CRI>::EnableGfxProductHw();
} // namespace NEO
