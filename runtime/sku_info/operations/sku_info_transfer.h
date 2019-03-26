/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/gmm_helper/gmm_lib.h"
#include "runtime/sku_info/operations/sku_info_transfer.h"

#include "sku_info.h"

namespace NEO {
class SkuInfoTransfer {
  public:
    static void transferFtrTableForGmm(_SKU_FEATURE_TABLE *dstFtrTable, const FeatureTable *srcFtrTable);
    static void transferWaTableForGmm(_WA_TABLE *dstWaTable, const WorkaroundTable *srcWaTable);

  protected:
    static void transferFtrTableForGmmBase(_SKU_FEATURE_TABLE *dstFtrTable, const NEO::FeatureTable *srcFtrTable) {
#define TRANSFER_FTR_TO_GMM(VAL_NAME) dstFtrTable->Ftr##VAL_NAME = srcFtrTable->ftr##VAL_NAME
        TRANSFER_FTR_TO_GMM(StandardMipTailFormat);
        TRANSFER_FTR_TO_GMM(ULT);
        TRANSFER_FTR_TO_GMM(EDram);
        TRANSFER_FTR_TO_GMM(FrameBufferLLC);
        TRANSFER_FTR_TO_GMM(Crystalwell);
        TRANSFER_FTR_TO_GMM(DisplayEngineS3d);
        TRANSFER_FTR_TO_GMM(TileY);
        TRANSFER_FTR_TO_GMM(DisplayYTiling);
        TRANSFER_FTR_TO_GMM(Fbc);
        TRANSFER_FTR_TO_GMM(VERing);
        TRANSFER_FTR_TO_GMM(Vcs2);
        TRANSFER_FTR_TO_GMM(LCIA);
        TRANSFER_FTR_TO_GMM(IA32eGfxPTEs);
        TRANSFER_FTR_TO_GMM(Wddm2GpuMmu);
        TRANSFER_FTR_TO_GMM(Wddm2_1_64kbPages);
        TRANSFER_FTR_TO_GMM(TranslationTable);
        TRANSFER_FTR_TO_GMM(UserModeTranslationTable);
#undef TRANSFER_FTR_TO_GMM
    }

    static void transferWaTableForGmmBase(_WA_TABLE *dstWaTable, const NEO::WorkaroundTable *srcWaTable) {
#define TRANSFER_WA_TO_GMM(VAL_NAME) dstWaTable->Wa##VAL_NAME = srcWaTable->wa##VAL_NAME
        TRANSFER_WA_TO_GMM(FbcLinearSurfaceStride);
        TRANSFER_WA_TO_GMM(DisableEdramForDisplayRT);
        TRANSFER_WA_TO_GMM(EncryptedEdramOnlyPartials);
        TRANSFER_WA_TO_GMM(LosslessCompressionSurfaceStride);
#undef TRANSFER_WA_TO_GMM
    }
};

} // namespace NEO
