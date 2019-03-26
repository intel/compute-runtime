/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
struct FeatureTableBase {
    bool ftrDesktop = false;
    bool ftrChannelSwizzlingXOREnabled = false;

    bool ftrGtBigDie = false;
    bool ftrGtMediumDie = false;
    bool ftrGtSmallDie = false;

    bool ftrGT1 = false;
    bool ftrGT1_5 = false;
    bool ftrGT2 = false;
    bool ftrGT2_5 = false;
    bool ftrGT3 = false;
    bool ftrGT4 = false;

    bool ftrIVBM0M1Platform = false;
    bool ftrSGTPVSKUStrapPresent = false;
    bool ftrGTA = false;
    bool ftrGTC = false;
    bool ftrGTX = false;
    bool ftr5Slice = false;

    bool ftrGpGpuMidBatchPreempt = false;
    bool ftrGpGpuThreadGroupLevelPreempt = false;
    bool ftrGpGpuMidThreadLevelPreempt = false;

    bool ftrIoMmuPageFaulting = false;
    bool ftrWddm2Svm = false;
    bool ftrPooledEuEnabled = false;

    bool ftrResourceStreamer = false;

    bool ftrPPGTT = false;
    bool ftrSVM = false;
    bool ftrEDram = false;
    bool ftrL3IACoherency = false;
    bool ftrIA32eGfxPTEs = false;

    bool ftr3dMidBatchPreempt = false;
    bool ftr3dObjectLevelPreempt = false;
    bool ftrPerCtxtPreemptionGranularityControl = false;

    bool ftrTileY = false;
    bool ftrDisplayYTiling = false;
    bool ftrTranslationTable = false;
    bool ftrUserModeTranslationTable = false;

    bool ftrEnableGuC = false;

    bool ftrFbc = false;
    bool ftrFbc2AddressTranslation = false;
    bool ftrFbcBlitterTracking = false;
    bool ftrFbcCpuTracking = false;

    bool ftrVcs2 = false;
    bool ftrVEBOX = false;
    bool ftrSingleVeboxSlice = false;
    bool ftrULT = false;
    bool ftrLCIA = false;
    bool ftrGttCacheInvalidation = false;
    bool ftrTileMappedResource = false;
    bool ftrAstcHdr2D = false;
    bool ftrAstcLdr2D = false;

    bool ftrStandardMipTailFormat = false;
    bool ftrFrameBufferLLC = false;
    bool ftrCrystalwell = false;
    bool ftrLLCBypass = false;
    bool ftrDisplayEngineS3d = false;
    bool ftrVERing = false;
    bool ftrWddm2GpuMmu = false;
    bool ftrWddm2_1_64kbPages = false;
    bool ftrWddmHwQueues = false;

    bool ftrKmdDaf = false;
    bool ftrSimulationMode = false;
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
};
} // namespace NEO
