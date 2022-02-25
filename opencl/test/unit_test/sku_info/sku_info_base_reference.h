/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/gmm_helper/gmm_lib.h"

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
        refFtrTable.FtrLCIA = 1;
        refFtrTable.FtrIA32eGfxPTEs = 1;
        refFtrTable.FtrWddm2GpuMmu = 1;
        refFtrTable.FtrWddm2_1_64kbPages = 1;

        refFtrTable.FtrTranslationTable = 1;
        refFtrTable.FtrUserModeTranslationTable = 1;
        refFtrTable.FtrWddm2Svm = 1;
        refFtrTable.FtrLLCBypass = 1;

        refFtrTable.FtrE2ECompression = 1;
        refFtrTable.FtrLinearCCS = 1;
        refFtrTable.FtrCCSRing = 1;
        refFtrTable.FtrCCSNode = 1;
        refFtrTable.FtrMemTypeMocsDeferPAT = 1;
        refFtrTable.FtrLocalMemory = 1;
        refFtrTable.FtrLocalMemoryAllows4KB = 1;
        refFtrTable.FtrSVM = 1;
        refFtrTable.FtrFlatPhysCCS = 1;
        refFtrTable.FtrMultiTileArch = 1;
        refFtrTable.FtrCCSMultiInstance = 1;
        refFtrTable.FtrPpgtt64KBWalkOptimization = 1;
        refFtrTable.FtrUnified3DMediaCompressionFormats = 1;
        refFtrTable.Ftr57bGPUAddressing = 1;
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
        refWaTable.WaAuxTable64KGranular = 1;
    }

    static void fillReferenceFtrToReceive(FeatureTable &refFtrTable) {
        refFtrTable = {};
        refFtrTable.flags.ftrDesktop = true;
        refFtrTable.flags.ftrChannelSwizzlingXOREnabled = true;

        refFtrTable.flags.ftrIVBM0M1Platform = true;
        refFtrTable.flags.ftrSGTPVSKUStrapPresent = true;
        refFtrTable.flags.ftr5Slice = true;

        refFtrTable.flags.ftrGpGpuMidBatchPreempt = true;
        refFtrTable.flags.ftrGpGpuThreadGroupLevelPreempt = true;
        refFtrTable.flags.ftrGpGpuMidThreadLevelPreempt = true;

        refFtrTable.flags.ftrIoMmuPageFaulting = true;
        refFtrTable.flags.ftrWddm2Svm = true;
        refFtrTable.flags.ftrPooledEuEnabled = true;

        refFtrTable.flags.ftrResourceStreamer = true;

        refFtrTable.flags.ftrPPGTT = true;
        refFtrTable.flags.ftrSVM = true;
        refFtrTable.flags.ftrEDram = true;
        refFtrTable.flags.ftrL3IACoherency = true;
        refFtrTable.flags.ftrIA32eGfxPTEs = true;

        refFtrTable.flags.ftr3dMidBatchPreempt = true;
        refFtrTable.flags.ftr3dObjectLevelPreempt = true;
        refFtrTable.flags.ftrPerCtxtPreemptionGranularityControl = true;

        refFtrTable.flags.ftrTileY = true;
        refFtrTable.flags.ftrDisplayYTiling = true;
        refFtrTable.flags.ftrTranslationTable = true;
        refFtrTable.flags.ftrUserModeTranslationTable = true;

        refFtrTable.flags.ftrEnableGuC = true;

        refFtrTable.flags.ftrFbc = true;
        refFtrTable.flags.ftrFbc2AddressTranslation = true;
        refFtrTable.flags.ftrFbcBlitterTracking = true;
        refFtrTable.flags.ftrFbcCpuTracking = true;

        refFtrTable.flags.ftrULT = true;
        refFtrTable.flags.ftrLCIA = true;
        refFtrTable.flags.ftrGttCacheInvalidation = true;
        refFtrTable.flags.ftrTileMappedResource = true;
        refFtrTable.flags.ftrAstcHdr2D = true;
        refFtrTable.flags.ftrAstcLdr2D = true;

        refFtrTable.flags.ftrStandardMipTailFormat = true;
        refFtrTable.flags.ftrFrameBufferLLC = true;
        refFtrTable.flags.ftrCrystalwell = true;
        refFtrTable.flags.ftrLLCBypass = true;
        refFtrTable.flags.ftrDisplayEngineS3d = true;
        refFtrTable.flags.ftrWddm2GpuMmu = true;
        refFtrTable.flags.ftrWddm2_1_64kbPages = true;

        refFtrTable.flags.ftrKmdDaf = true;
        refFtrTable.flags.ftrSimulationMode = true;

        refFtrTable.flags.ftrE2ECompression = true;
        refFtrTable.flags.ftrLinearCCS = true;
        refFtrTable.flags.ftrCCSRing = true;
        refFtrTable.flags.ftrCCSNode = true;
        refFtrTable.flags.ftrRcsNode = true;
        refFtrTable.flags.ftrMemTypeMocsDeferPAT = true;
        refFtrTable.flags.ftrLocalMemory = true;
        refFtrTable.flags.ftrLocalMemoryAllows4KB = true;

        refFtrTable.flags.ftrFlatPhysCCS = true;
        refFtrTable.flags.ftrMultiTileArch = true;
        refFtrTable.flags.ftrCCSMultiInstance = true;
        refFtrTable.flags.ftrPpgtt64KBWalkOptimization = true;
        refFtrTable.flags.ftrUnified3DMediaCompressionFormats = true;
        refFtrTable.flags.ftr57bGPUAddressing = true;
    }

    static void fillReferenceWaToReceive(WorkaroundTable &refWaTable) {
        refWaTable = {};
        refWaTable.flags.waDoNotUseMIReportPerfCount = true;

        refWaTable.flags.waEnablePreemptionGranularityControlByUMD = true;
        refWaTable.flags.waSendMIFLUSHBeforeVFE = true;
        refWaTable.flags.waReportPerfCountUseGlobalContextID = true;
        refWaTable.flags.waDisableLSQCROPERFforOCL = true;
        refWaTable.flags.waMsaa8xTileYDepthPitchAlignment = true;
        refWaTable.flags.waLosslessCompressionSurfaceStride = true;
        refWaTable.flags.waFbcLinearSurfaceStride = true;
        refWaTable.flags.wa4kAlignUVOffsetNV12LinearSurface = true;
        refWaTable.flags.waEncryptedEdramOnlyPartials = true;
        refWaTable.flags.waDisableEdramForDisplayRT = true;
        refWaTable.flags.waForcePcBbFullCfgRestore = true;
        refWaTable.flags.waCompressedResourceRequiresConstVA21 = true;
        refWaTable.flags.waDisablePerCtxtPreemptionGranularityControl = true;
        refWaTable.flags.waLLCCachingUnsupported = true;
        refWaTable.flags.waUseVAlign16OnTileXYBpp816 = true;
        refWaTable.flags.waModifyVFEStateAfterGPGPUPreemption = true;
        refWaTable.flags.waCSRUncachable = true;
        refWaTable.flags.waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;
        refWaTable.flags.waRestrictPitch128KB = true;
        refWaTable.flags.waLimit128BMediaCompr = true;
        refWaTable.flags.waUntypedBufferCompression = true;
        refWaTable.flags.waAuxTable16KGranular = true;
        refWaTable.flags.waDisableFusedThreadScheduling = true;
        refWaTable.flags.waAuxTable64KGranular = true;
    }
}; // namespace SkuInfoBaseReference
} // namespace NEO
