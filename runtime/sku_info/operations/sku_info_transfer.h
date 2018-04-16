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
#include "runtime/sku_info/operations/sku_info_transfer.h"
#include "runtime/gmm_helper/gmm_lib.h"

namespace OCLRT {
class SkuInfoTransfer {
  public:
    static void transferFtrTableForGmm(_SKU_FEATURE_TABLE *dstFtrTable, const FeatureTable *srcFtrTable);
    static void transferWaTableForGmm(_WA_TABLE *dstWaTable, const WorkaroundTable *srcWaTable);

  protected:
    static void transferFtrTableForGmmBase(_SKU_FEATURE_TABLE *dstFtrTable, const OCLRT::FeatureTable *srcFtrTable) {
#define TRANSFER_FTR_TO_GMM(VAL_NAME) dstFtrTable->Ftr##VAL_NAME = srcFtrTable->ftr##VAL_NAME
        TRANSFER_FTR_TO_GMM(StandardMipTailFormat);
        TRANSFER_FTR_TO_GMM(ULT);
        TRANSFER_FTR_TO_GMM(EDram);
        TRANSFER_FTR_TO_GMM(FrameBufferLLC);
        TRANSFER_FTR_TO_GMM(Crystalwell);
        TRANSFER_FTR_TO_GMM(DisplayEngineS3d);
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

    static void transferWaTableForGmmBase(_WA_TABLE *dstWaTable, const OCLRT::WorkaroundTable *srcWaTable) {
#define TRANSFER_WA_TO_GMM(VAL_NAME) dstWaTable->Wa##VAL_NAME = srcWaTable->wa##VAL_NAME
        TRANSFER_WA_TO_GMM(FbcLinearSurfaceStride);
        TRANSFER_WA_TO_GMM(DisableEdramForDisplayRT);
        TRANSFER_WA_TO_GMM(EncryptedEdramOnlyPartials);
        TRANSFER_WA_TO_GMM(LosslessCompressionSurfaceStride);
#undef TRANSFER_WA_TO_GMM
    }
};

} // namespace OCLRT
