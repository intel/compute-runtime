/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {
struct FeatureTableBase {
  public:
    FeatureTableBase() : packed{} {
        ftrRcsNode = 1;
    }
    union {
        struct {
            bool ftrDesktop : 1;
            bool ftrChannelSwizzlingXOREnabled : 1;

            bool ftrGtBigDie : 1;
            bool ftrGtMediumDie : 1;
            bool ftrGtSmallDie : 1;

            bool ftrGT1 : 1;
            bool ftrGT1_5 : 1;
            bool ftrGT2 : 1;
            bool ftrGT2_5 : 1;
            bool ftrGT3 : 1;
            bool ftrGT4 : 1;

            bool ftrIVBM0M1Platform : 1;
            bool ftrSGTPVSKUStrapPresent : 1;
            bool ftrGTA : 1;
            bool ftrGTC : 1;
            bool ftrGTX : 1;
            bool ftr5Slice : 1;

            bool ftrGpGpuMidBatchPreempt : 1;
            bool ftrGpGpuThreadGroupLevelPreempt : 1;
            bool ftrGpGpuMidThreadLevelPreempt : 1;

            bool ftrIoMmuPageFaulting : 1;
            bool ftrWddm2Svm : 1;
            bool ftrPooledEuEnabled : 1;

            bool ftrResourceStreamer : 1;

            bool ftrPPGTT : 1;
            bool ftrSVM : 1;
            bool ftrEDram : 1;
            bool ftrL3IACoherency : 1;
            bool ftrIA32eGfxPTEs : 1;

            bool ftr3dMidBatchPreempt : 1;
            bool ftr3dObjectLevelPreempt : 1;
            bool ftrPerCtxtPreemptionGranularityControl : 1;

            bool ftrTileY : 1;
            bool ftrDisplayYTiling : 1;
            bool ftrTranslationTable : 1;
            bool ftrUserModeTranslationTable : 1;

            bool ftrEnableGuC : 1;

            bool ftrFbc : 1;
            bool ftrFbc2AddressTranslation : 1;
            bool ftrFbcBlitterTracking : 1;
            bool ftrFbcCpuTracking : 1;

            bool ftrVcs2 : 1;
            bool ftrVEBOX : 1;
            bool ftrSingleVeboxSlice : 1;
            bool ftrULT : 1;
            bool ftrLCIA : 1;
            bool ftrGttCacheInvalidation : 1;
            bool ftrTileMappedResource : 1;
            bool ftrAstcHdr2D : 1;
            bool ftrAstcLdr2D : 1;

            bool ftrStandardMipTailFormat : 1;
            bool ftrFrameBufferLLC : 1;
            bool ftrCrystalwell : 1;
            bool ftrLLCBypass : 1;
            bool ftrDisplayEngineS3d : 1;
            bool ftrVERing : 1;
            bool ftrWddm2GpuMmu : 1;
            bool ftrWddm2_1_64kbPages : 1;
            bool ftrWddmHwQueues : 1;
            bool ftrMemTypeMocsDeferPAT : 1;

            bool ftrKmdDaf : 1;
            bool ftrSimulationMode : 1;

            bool ftrE2ECompression : 1;
            bool ftrLinearCCS : 1;
            bool ftrCCSRing : 1;
            bool ftrCCSNode : 1;
            bool ftrRcsNode : 1;
            bool ftrLocalMemory : 1;
            bool ftrLocalMemoryAllows4KB : 1;
            bool ftrFlatPhysCCS : 1;
            bool ftrMultiTileArch : 1;
            bool ftrCCSMultiInstance : 1;
            bool ftrPpgtt64KBWalkOptimization : 1;
        };
        uint64_t packed[2];
    };
};

struct WorkaroundTableBase {
    bool waDoNotUseMIReportPerfCount = false;

    bool waEnablePreemptionGranularityControlByUMD = false;
    bool waSendMIFLUSHBeforeVFE = false;
    bool waReportPerfCountUseGlobalContextID = false;
    bool waDisableLSQCROPERFforOCL = false;
    bool waMsaa8xTileYDepthPitchAlignment = false;
    bool waLosslessCompressionSurfaceStride = false;
    bool waFbcLinearSurfaceStride = false;
    bool wa4kAlignUVOffsetNV12LinearSurface = false;
    bool waEncryptedEdramOnlyPartials = false;
    bool waDisableEdramForDisplayRT = false;
    bool waForcePcBbFullCfgRestore = false;
    bool waCompressedResourceRequiresConstVA21 = false;
    bool waDisablePerCtxtPreemptionGranularityControl = false;
    bool waLLCCachingUnsupported = false;
    bool waUseVAlign16OnTileXYBpp816 = false;
    bool waModifyVFEStateAfterGPGPUPreemption = false;
    bool waCSRUncachable = false;
    bool waSamplerCacheFlushBetweenRedescribedSurfaceReads = false;
    bool waRestrictPitch128KB = false;
    bool waLimit128BMediaCompr = false;
    bool waUntypedBufferCompression = false;
    bool waAuxTable16KGranular = false;
    bool waDisableFusedThreadScheduling = false;
    bool waDefaultTile4 = false;
};
} // namespace NEO
