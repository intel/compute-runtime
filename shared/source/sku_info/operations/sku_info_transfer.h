/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm_lib.h"

#include "sku_info.h"

namespace NEO {
class SkuInfoTransfer {
  public:
    static void transferFtrTableForGmm(_SKU_FEATURE_TABLE *dstFtrTable, const FeatureTable *srcFtrTable);
    static void transferWaTableForGmm(_WA_TABLE *dstWaTable, const WorkaroundTable *srcWaTable);

  protected:
    static void transferFtrTableForGmmBase(_SKU_FEATURE_TABLE *dstFtrTable, const NEO::FeatureTable *srcFtrTable) {
#define TRANSFER_FTR_TO_GMM(VAL_NAME) dstFtrTable->Ftr##VAL_NAME = srcFtrTable->flags.ftr##VAL_NAME
        TRANSFER_FTR_TO_GMM(StandardMipTailFormat);
        TRANSFER_FTR_TO_GMM(ULT);
        TRANSFER_FTR_TO_GMM(EDram);
        TRANSFER_FTR_TO_GMM(FrameBufferLLC);
        TRANSFER_FTR_TO_GMM(DisplayEngineS3d);
        TRANSFER_FTR_TO_GMM(TileY);
        TRANSFER_FTR_TO_GMM(DisplayYTiling);
        TRANSFER_FTR_TO_GMM(Fbc);
        TRANSFER_FTR_TO_GMM(LCIA);
        TRANSFER_FTR_TO_GMM(IA32eGfxPTEs);
        TRANSFER_FTR_TO_GMM(Wddm2GpuMmu);
        TRANSFER_FTR_TO_GMM(Wddm2_1_64kbPages);
        TRANSFER_FTR_TO_GMM(TranslationTable);
        TRANSFER_FTR_TO_GMM(UserModeTranslationTable);
        TRANSFER_FTR_TO_GMM(Wddm2Svm);
        TRANSFER_FTR_TO_GMM(LLCBypass);
        TRANSFER_FTR_TO_GMM(E2ECompression);
        TRANSFER_FTR_TO_GMM(LinearCCS);
        TRANSFER_FTR_TO_GMM(CCSRing);
        TRANSFER_FTR_TO_GMM(CCSNode);
        TRANSFER_FTR_TO_GMM(MemTypeMocsDeferPAT);
        TRANSFER_FTR_TO_GMM(LocalMemory);
        TRANSFER_FTR_TO_GMM(LocalMemoryAllows4KB);
        TRANSFER_FTR_TO_GMM(SVM);
        TRANSFER_FTR_TO_GMM(FlatPhysCCS);
        TRANSFER_FTR_TO_GMM(MultiTileArch);
        TRANSFER_FTR_TO_GMM(Ppgtt64KBWalkOptimization);
        TRANSFER_FTR_TO_GMM(Unified3DMediaCompressionFormats);
        TRANSFER_FTR_TO_GMM(57bGPUAddressing);
        TRANSFER_FTR_TO_GMM(Xe2Compression);
        TRANSFER_FTR_TO_GMM(Xe2PlusTiling);
        TRANSFER_FTR_TO_GMM(Pml5Support);
        TRANSFER_FTR_TO_GMM(L3TransientDataFlush);

#undef TRANSFER_FTR_TO_GMM
    }

    static void transferWaTableForGmmBase(_WA_TABLE *dstWaTable, const NEO::WorkaroundTable *srcWaTable) {
#define TRANSFER_WA_TO_GMM(VAL_NAME) dstWaTable->Wa##VAL_NAME = srcWaTable->flags.wa##VAL_NAME
        TRANSFER_WA_TO_GMM(FbcLinearSurfaceStride);
        TRANSFER_WA_TO_GMM(DisableEdramForDisplayRT);
        TRANSFER_WA_TO_GMM(EncryptedEdramOnlyPartials);
        TRANSFER_WA_TO_GMM(LosslessCompressionSurfaceStride);
        TRANSFER_WA_TO_GMM(RestrictPitch128KB);
        TRANSFER_WA_TO_GMM(AuxTable16KGranular);
        TRANSFER_WA_TO_GMM(Limit128BMediaCompr);
        TRANSFER_WA_TO_GMM(UntypedBufferCompression);
        TRANSFER_WA_TO_GMM(AuxTable64KGranular);
        TRANSFER_WA_TO_GMM(_15010089951);
        TRANSFER_WA_TO_GMM(_14018976079);
        TRANSFER_WA_TO_GMM(_14018984349);
#undef TRANSFER_WA_TO_GMM
    }
};

} // namespace NEO
