/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/gmm_helper/gmm_lib.h"

#include "sku_info.h"

namespace NEO {
struct SkuInfoBaseReference {
    static void fillReferenceFtrForTransfer(_SKU_FEATURE_TABLE &refFtrTable) {
        memset(&refFtrTable, 0, sizeof(refFtrTable));
        refFtrTable.FtrStandardMipTailFormat = 1;
        refFtrTable.FtrULT = 1;
        refFtrTable.FtrEDram = 1;
        refFtrTable.FtrFrameBufferLLC = 1;
        refFtrTable.FtrCrystalwell = 1;
        refFtrTable.FtrDisplayEngineS3d = 1;
        refFtrTable.FtrTileY = 1;
        refFtrTable.FtrDisplayYTiling = 1;
        refFtrTable.FtrFbc = 1;
        refFtrTable.FtrVERing = 1;
        refFtrTable.FtrVcs2 = 1;
        refFtrTable.FtrLCIA = 1;
        refFtrTable.FtrIA32eGfxPTEs = 1;
        refFtrTable.FtrWddm2GpuMmu = 1;
        refFtrTable.FtrWddm2_1_64kbPages = 1;

        refFtrTable.FtrTranslationTable = 1;
        refFtrTable.FtrUserModeTranslationTable = 1;
        refFtrTable.FtrLLCBypass = 1;
        refFtrTable.FtrWddm2Svm = 1;

        refFtrTable.FtrE2ECompression = 1;
        refFtrTable.FtrLinearCCS = 1;
        refFtrTable.FtrCCSRing = 1;
        refFtrTable.FtrCCSNode = 1;
        refFtrTable.FtrMemTypeMocsDeferPAT = 1;
    }

    static void fillReferenceWaForTransfer(_WA_TABLE &refWaTable) {
        memset(&refWaTable, 0, sizeof(refWaTable));
        refWaTable.WaFbcLinearSurfaceStride = 1;
        refWaTable.WaDisableEdramForDisplayRT = 1;
        refWaTable.WaEncryptedEdramOnlyPartials = 1;
        refWaTable.WaLosslessCompressionSurfaceStride = 1;
        refWaTable.WaRestrictPitch128KB = 1;
        refWaTable.WaLimit128BMediaCompr = 1;
        refWaTable.WaUntypedBufferCompression = 1;
        refWaTable.WaAuxTable16KGranular = 1;
    }

    static void fillReferenceFtrToReceive(FeatureTable &refFtrTable) {
        refFtrTable = {};
        refFtrTable.ftrDesktop = true;
        refFtrTable.ftrChannelSwizzlingXOREnabled = true;

        refFtrTable.ftrGtBigDie = true;
        refFtrTable.ftrGtMediumDie = true;
        refFtrTable.ftrGtSmallDie = true;

        refFtrTable.ftrGT1 = true;
        refFtrTable.ftrGT1_5 = true;
        refFtrTable.ftrGT2 = true;
        refFtrTable.ftrGT2_5 = true;
        refFtrTable.ftrGT3 = true;
        refFtrTable.ftrGT4 = true;

        refFtrTable.ftrIVBM0M1Platform = true;
        refFtrTable.ftrSGTPVSKUStrapPresent = true;
        refFtrTable.ftrGTA = true;
        refFtrTable.ftrGTC = true;
        refFtrTable.ftrGTX = true;
        refFtrTable.ftr5Slice = true;

        refFtrTable.ftrGpGpuMidBatchPreempt = true;
        refFtrTable.ftrGpGpuThreadGroupLevelPreempt = true;
        refFtrTable.ftrGpGpuMidThreadLevelPreempt = true;

        refFtrTable.ftrIoMmuPageFaulting = true;
        refFtrTable.ftrWddm2Svm = true;
        refFtrTable.ftrPooledEuEnabled = true;

        refFtrTable.ftrResourceStreamer = true;

        refFtrTable.ftrPPGTT = true;
        refFtrTable.ftrSVM = true;
        refFtrTable.ftrEDram = true;
        refFtrTable.ftrL3IACoherency = true;
        refFtrTable.ftrIA32eGfxPTEs = true;

        refFtrTable.ftr3dMidBatchPreempt = true;
        refFtrTable.ftr3dObjectLevelPreempt = true;
        refFtrTable.ftrPerCtxtPreemptionGranularityControl = true;

        refFtrTable.ftrTileY = true;
        refFtrTable.ftrDisplayYTiling = true;
        refFtrTable.ftrTranslationTable = true;
        refFtrTable.ftrUserModeTranslationTable = true;

        refFtrTable.ftrEnableGuC = true;

        refFtrTable.ftrFbc = true;
        refFtrTable.ftrFbc2AddressTranslation = true;
        refFtrTable.ftrFbcBlitterTracking = true;
        refFtrTable.ftrFbcCpuTracking = true;

        refFtrTable.ftrVcs2 = true;
        refFtrTable.ftrVEBOX = true;
        refFtrTable.ftrSingleVeboxSlice = true;
        refFtrTable.ftrULT = true;
        refFtrTable.ftrLCIA = true;
        refFtrTable.ftrGttCacheInvalidation = true;
        refFtrTable.ftrTileMappedResource = true;
        refFtrTable.ftrAstcHdr2D = true;
        refFtrTable.ftrAstcLdr2D = true;

        refFtrTable.ftrStandardMipTailFormat = true;
        refFtrTable.ftrFrameBufferLLC = true;
        refFtrTable.ftrCrystalwell = true;
        refFtrTable.ftrLLCBypass = true;
        refFtrTable.ftrDisplayEngineS3d = true;
        refFtrTable.ftrVERing = true;
        refFtrTable.ftrWddm2GpuMmu = true;
        refFtrTable.ftrWddm2_1_64kbPages = true;

        refFtrTable.ftrKmdDaf = true;
        refFtrTable.ftrSimulationMode = true;

        refFtrTable.ftrE2ECompression = 1;
        refFtrTable.ftrLinearCCS = 1;
        refFtrTable.ftrCCSRing = 1;
        refFtrTable.ftrCCSNode = 1;
        refFtrTable.ftrMemTypeMocsDeferPAT = 1;
    }

    static void fillReferenceWaToReceive(WorkaroundTable &refWaTable) {
        refWaTable = {};
        refWaTable.waDoNotUseMIReportPerfCount = true;

        refWaTable.waEnablePreemptionGranularityControlByUMD = true;
        refWaTable.waSendMIFLUSHBeforeVFE = true;
        refWaTable.waReportPerfCountUseGlobalContextID = true;
        refWaTable.waDisableLSQCROPERFforOCL = true;
        refWaTable.waMsaa8xTileYDepthPitchAlignment = true;
        refWaTable.waLosslessCompressionSurfaceStride = true;
        refWaTable.waFbcLinearSurfaceStride = true;
        refWaTable.wa4kAlignUVOffsetNV12LinearSurface = true;
        refWaTable.waEncryptedEdramOnlyPartials = true;
        refWaTable.waDisableEdramForDisplayRT = true;
        refWaTable.waForcePcBbFullCfgRestore = true;
        refWaTable.waCompressedResourceRequiresConstVA21 = true;
        refWaTable.waDisablePerCtxtPreemptionGranularityControl = true;
        refWaTable.waLLCCachingUnsupported = true;
        refWaTable.waUseVAlign16OnTileXYBpp816 = true;
        refWaTable.waModifyVFEStateAfterGPGPUPreemption = true;
        refWaTable.waCSRUncachable = true;
        refWaTable.waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;
        refWaTable.waRestrictPitch128KB = true;
        refWaTable.waLimit128BMediaCompr = 1;
        refWaTable.waUntypedBufferCompression = 1;
        refWaTable.waAuxTable16KGranular = 1;
    }
}; // namespace SkuInfoBaseReference
} // namespace NEO
