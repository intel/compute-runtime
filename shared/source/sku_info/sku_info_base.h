/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <array>
#include <bitset>
#include <cstdint>

namespace NEO {
constexpr size_t bcsInfoMaskSize = 9u;
using BcsInfoMask = std::bitset<bcsInfoMaskSize>;

struct FeatureTableBase {
  public:
    FeatureTableBase() {
        flags.ftrRcsNode = 1;
    }

    struct Flags {
        // DW0
        uint32_t ftrDesktop : 1;
        uint32_t ftrChannelSwizzlingXOREnabled : 1;
        uint32_t ftrIVBM0M1Platform : 1;
        uint32_t ftrSGTPVSKUStrapPresent : 1;
        uint32_t ftr5Slice : 1;
        uint32_t ftrGpGpuMidBatchPreempt : 1;
        uint32_t ftrGpGpuThreadGroupLevelPreempt : 1;
        uint32_t ftrGpGpuMidThreadLevelPreempt : 1;
        uint32_t ftrIoMmuPageFaulting : 1;
        uint32_t ftrWddm2Svm : 1;
        uint32_t ftrPooledEuEnabled : 1;
        uint32_t ftrResourceStreamer : 1;
        uint32_t ftrPPGTT : 1;
        uint32_t ftrSVM : 1;
        uint32_t ftrEDram : 1;
        uint32_t ftrL3IACoherency : 1;
        uint32_t ftrIA32eGfxPTEs : 1;
        uint32_t ftr3dMidBatchPreempt : 1;
        uint32_t ftr3dObjectLevelPreempt : 1;
        uint32_t ftrPerCtxtPreemptionGranularityControl : 1;
        uint32_t ftrTileY : 1;
        uint32_t ftrDisplayYTiling : 1;
        uint32_t ftrTranslationTable : 1;
        uint32_t ftrUserModeTranslationTable : 1;
        uint32_t ftrEnableGuC : 1;
        uint32_t ftrFbc : 1;
        uint32_t ftrFbc2AddressTranslation : 1;
        uint32_t ftrFbcBlitterTracking : 1;
        uint32_t ftrFbcCpuTracking : 1;
        uint32_t ftrULT : 1;
        uint32_t ftrLCIA : 1;
        uint32_t ftrGttCacheInvalidation : 1;
        // DW1
        uint32_t ftrTileMappedResource : 1;
        uint32_t ftrAstcHdr2D : 1;
        uint32_t ftrAstcLdr2D : 1;
        uint32_t ftrStandardMipTailFormat : 1;
        uint32_t ftrFrameBufferLLC : 1;
        uint32_t ftrCrystalwell : 1;
        uint32_t ftrLLCBypass : 1;
        uint32_t ftrDisplayEngineS3d : 1;
        uint32_t ftrWddm2GpuMmu : 1;
        uint32_t ftrWddm2_1_64kbPages : 1;
        uint32_t ftrWddmHwQueues : 1;
        uint32_t ftrMemTypeMocsDeferPAT : 1;
        uint32_t ftrKmdDaf : 1;
        uint32_t ftrSimulationMode : 1;
        uint32_t ftrE2ECompression : 1;
        uint32_t ftrLinearCCS : 1;
        uint32_t ftrCCSRing : 1;
        uint32_t ftrCCSNode : 1;
        uint32_t ftrRcsNode : 1;
        uint32_t ftrLocalMemory : 1;
        uint32_t ftrLocalMemoryAllows4KB : 1;
        uint32_t ftrFlatPhysCCS : 1;
        uint32_t ftrMultiTileArch : 1;
        uint32_t ftrCCSMultiInstance : 1;
        uint32_t ftrPpgtt64KBWalkOptimization : 1;
        uint32_t ftrUnified3DMediaCompressionFormats : 1;
        uint32_t ftr57bGPUAddressing : 1;
        uint32_t reserved : 5;
    };

    BcsInfoMask ftrBcsInfo = 1;

    union {
        Flags flags;
        std::array<uint32_t, 2> packed = {};
    };
};

static_assert(sizeof(FeatureTableBase::flags) == sizeof(FeatureTableBase::packed));

struct WorkaroundTableBase {

    struct Flags {
        // DW0
        uint32_t waDoNotUseMIReportPerfCount : 1;
        uint32_t waEnablePreemptionGranularityControlByUMD : 1;
        uint32_t waSendMIFLUSHBeforeVFE : 1;
        uint32_t waReportPerfCountUseGlobalContextID : 1;
        uint32_t waDisableLSQCROPERFforOCL : 1;
        uint32_t waMsaa8xTileYDepthPitchAlignment : 1;
        uint32_t waLosslessCompressionSurfaceStride : 1;
        uint32_t waFbcLinearSurfaceStride : 1;
        uint32_t wa4kAlignUVOffsetNV12LinearSurface : 1;
        uint32_t waEncryptedEdramOnlyPartials : 1;
        uint32_t waDisableEdramForDisplayRT : 1;
        uint32_t waForcePcBbFullCfgRestore : 1;
        uint32_t waCompressedResourceRequiresConstVA21 : 1;
        uint32_t waDisablePerCtxtPreemptionGranularityControl : 1;
        uint32_t waLLCCachingUnsupported : 1;
        uint32_t waUseVAlign16OnTileXYBpp816 : 1;
        uint32_t waModifyVFEStateAfterGPGPUPreemption : 1;
        uint32_t waCSRUncachable : 1;
        uint32_t waSamplerCacheFlushBetweenRedescribedSurfaceReads : 1;
        uint32_t waRestrictPitch128KB : 1;
        uint32_t waLimit128BMediaCompr : 1;
        uint32_t waUntypedBufferCompression : 1;
        uint32_t waAuxTable16KGranular : 1;
        uint32_t waDisableFusedThreadScheduling : 1;
        uint32_t waAuxTable64KGranular : 1;
        uint32_t reserved : 7;
    };

    union {
        Flags flags;
        std::array<uint32_t, 1> packed = {};
    };
};

static_assert(sizeof(WorkaroundTableBase::flags) == sizeof(WorkaroundTableBase::packed));
} // namespace NEO
