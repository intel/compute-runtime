/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "sku_info.h"

namespace NEO {
struct SkuInfoBaseReference {
    static void fillReferenceFtrForTransfer(_SKU_FEATURE_TABLE &refFtrTable) {
        memset(&refFtrTable, 0, sizeof(refFtrTable));
        refFtrTable.FtrStandardMipTailFormat = 1;
        refFtrTable.FtrULT = 1;
        refFtrTable.FtrEDram = 1;
        refFtrTable.FtrFrameBufferLLC = 1;
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
        refFtrTable.FtrPpgtt64KBWalkOptimization = 1;
        refFtrTable.FtrUnified3DMediaCompressionFormats = 1;
        refFtrTable.Ftr57bGPUAddressing = 1;
        refFtrTable.FtrXe2Compression = 1;
        refFtrTable.FtrXe2PlusTiling = 1;
        refFtrTable.FtrPml5Support = 1;
        refFtrTable.FtrL3TransientDataFlush = 1;
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
        refWaTable.Wa_15010089951 = 1;
        refWaTable.Wa_14018976079 = 1;
        refWaTable.Wa_14018984349 = 1;
    }

    static void fillReferenceFtrToReceive(FeatureTable &refFtrTable) {
        refFtrTable = {};

        refFtrTable.flags.ftrGpGpuMidBatchPreempt = true;
        refFtrTable.flags.ftrGpGpuThreadGroupLevelPreempt = true;
        refFtrTable.flags.ftrWddm2Svm = true;
        refFtrTable.flags.ftrPooledEuEnabled = true;

        refFtrTable.flags.ftrPPGTT = true;
        refFtrTable.flags.ftrSVM = true;
        refFtrTable.flags.ftrEDram = true;
        refFtrTable.flags.ftrL3IACoherency = true;
        refFtrTable.flags.ftrIA32eGfxPTEs = true;

        refFtrTable.flags.ftrTileY = true;
        refFtrTable.flags.ftrDisplayYTiling = true;
        refFtrTable.flags.ftrTranslationTable = true;
        refFtrTable.flags.ftrUserModeTranslationTable = true;

        refFtrTable.flags.ftrFbc = true;

        refFtrTable.flags.ftrULT = true;
        refFtrTable.flags.ftrLCIA = true;
        refFtrTable.flags.ftrTileMappedResource = true;
        refFtrTable.flags.ftrAstcHdr2D = true;
        refFtrTable.flags.ftrAstcLdr2D = true;

        refFtrTable.flags.ftrStandardMipTailFormat = true;
        refFtrTable.flags.ftrFrameBufferLLC = true;
        refFtrTable.flags.ftrLLCBypass = true;
        refFtrTable.flags.ftrDisplayEngineS3d = true;
        refFtrTable.flags.ftrWddm2GpuMmu = true;
        refFtrTable.flags.ftrWddm2_1_64kbPages = true;

        refFtrTable.flags.ftrKmdDaf = true;

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
        refFtrTable.flags.ftrPpgtt64KBWalkOptimization = true;
        refFtrTable.flags.ftrUnified3DMediaCompressionFormats = true;
        refFtrTable.flags.ftr57bGPUAddressing = true;
        refFtrTable.flags.ftrTile64Optimization = true;

        refFtrTable.flags.ftrWddmHwQueues = true;
        refFtrTable.flags.ftrWalkerMTP = true;
        refFtrTable.flags.ftrXe2Compression = true;
        refFtrTable.flags.ftrXe2PlusTiling = true;
        refFtrTable.flags.ftrPml5Support = true;
        refFtrTable.flags.ftrL3TransientDataFlush = true;
    }

    static void fillReferenceWaToReceive(WorkaroundTable &refWaTable) {
        refWaTable = {};

        refWaTable.flags.waSendMIFLUSHBeforeVFE = true;
        refWaTable.flags.waDisableLSQCROPERFforOCL = true;
        refWaTable.flags.waMsaa8xTileYDepthPitchAlignment = true;
        refWaTable.flags.waLosslessCompressionSurfaceStride = true;
        refWaTable.flags.waFbcLinearSurfaceStride = true;
        refWaTable.flags.wa4kAlignUVOffsetNV12LinearSurface = true;
        refWaTable.flags.waEncryptedEdramOnlyPartials = true;
        refWaTable.flags.waDisableEdramForDisplayRT = true;
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
        refWaTable.flags.wa_15010089951 = true;
        refWaTable.flags.wa_14018976079 = true;
        refWaTable.flags.wa_14018984349 = true;
    }
}; // namespace SkuInfoBaseReference
} // namespace NEO
