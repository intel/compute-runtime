/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
#pragma once

#include "sku_info.h"
#include "runtime/gmm_helper/gmm_lib.h"

namespace OCLRT {
struct SkuInfoBaseReference {
    static void fillReferenceFtrForTransfer(_SKU_FEATURE_TABLE &refFtrTable) {
        memset(&refFtrTable, 0, sizeof(refFtrTable));
        refFtrTable.FtrStandardMipTailFormat = 1;
        refFtrTable.FtrULT = 1;
        refFtrTable.FtrEDram = 1;
        refFtrTable.FtrFrameBufferLLC = 1;
        refFtrTable.FtrCrystalwell = 1;
        refFtrTable.FtrDisplayEngineS3d = 1;
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
    }

    static void fillReferenceWaForTransfer(_WA_TABLE &refWaTable) {
        memset(&refWaTable, 0, sizeof(refWaTable));
        refWaTable.WaFbcLinearSurfaceStride = 1;
        refWaTable.WaDisableEdramForDisplayRT = 1;
        refWaTable.WaEncryptedEdramOnlyPartials = 1;
        refWaTable.WaLosslessCompressionSurfaceStride = 1;
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
    }
}; // namespace SkuInfoBaseReference
} // namespace OCLRT
