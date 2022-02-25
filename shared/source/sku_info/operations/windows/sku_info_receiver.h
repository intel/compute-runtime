/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/windows/sharedata_wrapper.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"

#include "sku_info.h"

namespace NEO {
class SkuInfoReceiver {
  public:
    static void receiveFtrTableFromAdapterInfo(FeatureTable *ftrTable, ADAPTER_INFO_KMD *adapterInfo);
    static void receiveWaTableFromAdapterInfo(WorkaroundTable *workaroundTable, ADAPTER_INFO_KMD *adapterInfo);

  protected:
    static void receiveFtrTableFromAdapterInfoBase(FeatureTable *ftrTable, ADAPTER_INFO_KMD *adapterInfo) {
#define RECEIVE_FTR(VAL_NAME) ftrTable->flags.ftr##VAL_NAME = adapterInfo->SkuTable.Ftr##VAL_NAME
        RECEIVE_FTR(Desktop);
        RECEIVE_FTR(ChannelSwizzlingXOREnabled);

        RECEIVE_FTR(IVBM0M1Platform);
        RECEIVE_FTR(SGTPVSKUStrapPresent);
        RECEIVE_FTR(5Slice);

        RECEIVE_FTR(GpGpuMidBatchPreempt);
        RECEIVE_FTR(GpGpuThreadGroupLevelPreempt);
        RECEIVE_FTR(GpGpuMidThreadLevelPreempt);

        RECEIVE_FTR(IoMmuPageFaulting);
        RECEIVE_FTR(Wddm2Svm);
        RECEIVE_FTR(PooledEuEnabled);

        RECEIVE_FTR(ResourceStreamer);

        RECEIVE_FTR(PPGTT);
        RECEIVE_FTR(SVM);
        RECEIVE_FTR(EDram);
        RECEIVE_FTR(L3IACoherency);
        RECEIVE_FTR(IA32eGfxPTEs);

        RECEIVE_FTR(3dMidBatchPreempt);
        RECEIVE_FTR(3dObjectLevelPreempt);
        RECEIVE_FTR(PerCtxtPreemptionGranularityControl);

        RECEIVE_FTR(TileY);
        RECEIVE_FTR(DisplayYTiling);
        RECEIVE_FTR(TranslationTable);
        RECEIVE_FTR(UserModeTranslationTable);

        RECEIVE_FTR(EnableGuC);

        RECEIVE_FTR(Fbc);
        RECEIVE_FTR(Fbc2AddressTranslation);
        RECEIVE_FTR(FbcBlitterTracking);
        RECEIVE_FTR(FbcCpuTracking);

        RECEIVE_FTR(ULT);
        RECEIVE_FTR(LCIA);
        RECEIVE_FTR(GttCacheInvalidation);
        RECEIVE_FTR(TileMappedResource);
        RECEIVE_FTR(AstcHdr2D);
        RECEIVE_FTR(AstcLdr2D);

        RECEIVE_FTR(StandardMipTailFormat);
        RECEIVE_FTR(FrameBufferLLC);
        RECEIVE_FTR(Crystalwell);
        RECEIVE_FTR(LLCBypass);
        RECEIVE_FTR(DisplayEngineS3d);
        RECEIVE_FTR(Wddm2GpuMmu);
        RECEIVE_FTR(Wddm2_1_64kbPages);

        RECEIVE_FTR(KmdDaf);
        RECEIVE_FTR(SimulationMode);

        RECEIVE_FTR(E2ECompression);
        RECEIVE_FTR(LinearCCS);
        RECEIVE_FTR(CCSRing);
        RECEIVE_FTR(CCSNode);
        RECEIVE_FTR(MemTypeMocsDeferPAT);
        RECEIVE_FTR(LocalMemory);
        RECEIVE_FTR(LocalMemoryAllows4KB);
        RECEIVE_FTR(FlatPhysCCS);
        RECEIVE_FTR(MultiTileArch);
        RECEIVE_FTR(CCSMultiInstance);
        RECEIVE_FTR(Ppgtt64KBWalkOptimization);
        RECEIVE_FTR(Unified3DMediaCompressionFormats);
        RECEIVE_FTR(57bGPUAddressing);

#undef RECEIVE_FTR
    }

    static void receiveWaTableFromAdapterInfoBase(WorkaroundTable *workaroundTable, ADAPTER_INFO_KMD *adapterInfo) {
#define RECEIVE_WA(VAL_NAME) workaroundTable->flags.wa##VAL_NAME = adapterInfo->WaTable.Wa##VAL_NAME
        RECEIVE_WA(DoNotUseMIReportPerfCount);

        RECEIVE_WA(EnablePreemptionGranularityControlByUMD);
        RECEIVE_WA(SendMIFLUSHBeforeVFE);
        RECEIVE_WA(ReportPerfCountUseGlobalContextID);
        RECEIVE_WA(DisableLSQCROPERFforOCL);
        RECEIVE_WA(Msaa8xTileYDepthPitchAlignment);
        RECEIVE_WA(LosslessCompressionSurfaceStride);
        RECEIVE_WA(FbcLinearSurfaceStride);
        RECEIVE_WA(4kAlignUVOffsetNV12LinearSurface);
        RECEIVE_WA(EncryptedEdramOnlyPartials);
        RECEIVE_WA(DisableEdramForDisplayRT);
        RECEIVE_WA(ForcePcBbFullCfgRestore);
        RECEIVE_WA(CompressedResourceRequiresConstVA21);
        RECEIVE_WA(DisablePerCtxtPreemptionGranularityControl);
        RECEIVE_WA(LLCCachingUnsupported);
        RECEIVE_WA(UseVAlign16OnTileXYBpp816);
        RECEIVE_WA(ModifyVFEStateAfterGPGPUPreemption);
        RECEIVE_WA(CSRUncachable);
        RECEIVE_WA(SamplerCacheFlushBetweenRedescribedSurfaceReads);
        RECEIVE_WA(RestrictPitch128KB);
        RECEIVE_WA(AuxTable16KGranular);
        RECEIVE_WA(Limit128BMediaCompr);
        RECEIVE_WA(UntypedBufferCompression);
        RECEIVE_WA(DisableFusedThreadScheduling);
        RECEIVE_WA(AuxTable64KGranular);

#undef RECEIVE_WA
    }
};

} // namespace NEO
