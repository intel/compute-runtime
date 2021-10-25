/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/windows_wrapper.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#pragma clang diagnostic ignored "-Wbraced-scalar-init"
#endif
#include "umKmInc/sharedata.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

using SKU_FEATURE_TABLE_GMM = SKU_FEATURE_TABLE;
using WA_TABLE_GMM = WA_TABLE;
using ADAPTER_INFO_GMM = ADAPTER_INFO;

#if defined(UMD_KMD_COMMAND_BUFFER_REV_ID)
using SKU_FEATURE_TABLE_KMD = SKU_FEATURE_TABLE_GMM;
using WA_TABLE_KMD = WA_TABLE_GMM;
using ADAPTER_INFO_KMD = ADAPTER_INFO_GMM;

inline void propagateData(ADAPTER_INFO_KMD &) {
}

#if defined(__clang__) || defined(__GNUC__)
static constexpr COMMAND_BUFFER_HEADER initCommandBufferHeader(uint32_t umdContextType, uint32_t umdPatchList, uint32_t usesResourceStreamer, uint32_t perfTag) {
    COMMAND_BUFFER_HEADER ret = {};
    ret.UmdContextType = umdContextType;
    ret.UmdPatchList = umdPatchList;
    ret.UsesResourceStreamer = usesResourceStreamer;
    ret.PerfTag = perfTag;
    return ret;
}

#undef DECLARE_COMMAND_BUFFER
#define DECLARE_COMMAND_BUFFER(VARNAME, CONTEXTTYPE, PATCHLIST, STREAMER, PERFTAG) \
    static constexpr COMMAND_BUFFER_HEADER VARNAME = initCommandBufferHeader(CONTEXTTYPE, PATCHLIST, STREAMER, PERFTAG);
#endif

#else
struct SKU_FEATURE_TABLE_KMD : SKU_FEATURE_TABLE_GMM {
    bool FtrDesktop : 1;
    bool FtrChannelSwizzlingXOREnabled : 1;

    bool FtrGtBigDie : 1;
    bool FtrGtMediumDie : 1;
    bool FtrGtSmallDie : 1;

    bool FtrGT1 : 1;
    bool FtrGT1_5 : 1;
    bool FtrGT2 : 1;
    bool FtrGT2_5 : 1;
    bool FtrGT3 : 1;
    bool FtrGT4 : 1;

    bool FtrIVBM0M1Platform : 1;
    bool FtrSGTPVSKUStrapPresent : 1;
    bool FtrGTA : 1;
    bool FtrGTC : 1;
    bool FtrGTX : 1;
    bool Ftr5Slice : 1;

    bool FtrGpGpuMidBatchPreempt : 1;
    bool FtrGpGpuThreadGroupLevelPreempt : 1;
    bool FtrGpGpuMidThreadLevelPreempt : 1;

    bool FtrIoMmuPageFaulting : 1;
    bool FtrWddm2Svm : 1;
    bool FtrPooledEuEnabled : 1;

    bool FtrResourceStreamer : 1;

    bool FtrPPGTT : 1;
    bool FtrSVM : 1;
    bool FtrEDram : 1;
    bool FtrL3IACoherency : 1;
    bool FtrIA32eGfxPTEs : 1;

    bool Ftr3dMidBatchPreempt : 1;
    bool Ftr3dObjectLevelPreempt : 1;
    bool FtrPerCtxtPreemptionGranularityControl : 1;

    bool FtrTileY : 1;
    bool FtrDisplayYTiling : 1;
    bool FtrTranslationTable : 1;
    bool FtrUserModeTranslationTable : 1;

    bool FtrEnableGuC : 1;

    bool FtrFbc : 1;
    bool FtrFbc2AddressTranslation : 1;
    bool FtrFbcBlitterTracking : 1;
    bool FtrFbcCpuTracking : 1;

    bool FtrVcs2 : 1;
    bool FtrVEBOX : 1;
    bool FtrSingleVeboxSlice : 1;
    bool FtrULT : 1;
    bool FtrLCIA : 1;
    bool FtrGttCacheInvalidation : 1;
    bool FtrTileMappedResource : 1;
    bool FtrAstcHdr2D : 1;
    bool FtrAstcLdr2D : 1;

    bool FtrStandardMipTailFormat : 1;
    bool FtrFrameBufferLLC : 1;
    bool FtrCrystalwell : 1;
    bool FtrLLCBypass : 1;
    bool FtrDisplayEngineS3d : 1;
    bool FtrVERing : 1;
    bool FtrWddm2GpuMmu : 1;
    bool FtrWddm2_1_64kbPages : 1;
    bool FtrWddmHwQueues : 1;
    bool FtrMemTypeMocsDeferPAT : 1;

    bool FtrKmdDaf : 1;
    bool FtrSimulationMode : 1;

    bool FtrE2ECompression : 1;
    bool FtrLinearCCS : 1;
    bool FtrCCSRing : 1;
    bool FtrCCSNode : 1;
    bool FtrRcsNode : 1;
    bool FtrLocalMemory : 1;
    bool FtrLocalMemoryAllows4KB : 1;
};

struct WA_TABLE_KMD : WA_TABLE_GMM {
    bool WaDoNotUseMIReportPerfCount = false;

    bool WaEnablePreemptionGranularityControlByUMD = false;
    bool WaSendMIFLUSHBeforeVFE = false;
    bool WaReportPerfCountUseGlobalContextID = false;
    bool WaDisableLSQCROPERFforOCL = false;
    bool WaMsaa8xTileYDepthPitchAlignment = false;
    bool WaLosslessCompressionSurfaceStride = false;
    bool WaFbcLinearSurfaceStride = false;
    bool Wa4kAlignUVOffsetNV12LinearSurface = false;
    bool WaEncryptedEdramOnlyPartials = false;
    bool WaDisableEdramForDisplayRT = false;
    bool WaForcePcBbFullCfgRestore = false;
    bool WaCompressedResourceRequiresConstVA21 = false;
    bool WaDisablePerCtxtPreemptionGranularityControl = false;
    bool WaLLCCachingUnsupported = false;
    bool WaUseVAlign16OnTileXYBpp816 = false;
    bool WaModifyVFEStateAfterGPGPUPreemption = false;
    bool WaCSRUncachable = false;
    bool WaSamplerCacheFlushBetweenRedescribedSurfaceReads = false;
    bool WaRestrictPitch128KB = false;
    bool WaLimit128BMediaCompr = false;
    bool WaUntypedBufferCompression = false;
    bool WaAuxTable16KGranular = false;
    bool WaDisableFusedThreadScheduling = false;
};

typedef struct COMMAND_BUFFER_HEADER_REC {
    uint32_t UmdContextType : 4;
    uint32_t UmdPatchList : 1;

    uint32_t UmdRequestedSliceState : 3;
    uint32_t UmdRequestedSubsliceCount : 3;
    uint32_t UmdRequestedEUCount : 5;

    uint32_t UsesResourceStreamer : 1;
    uint32_t NeedsMidBatchPreEmptionSupport : 1;
    uint32_t UsesGPGPUPipeline : 1;
    uint32_t RequiresCoherency : 1;

    uint32_t PerfTag;
    uint64_t MonitorFenceVA;
    uint64_t MonitorFenceValue;
} COMMAND_BUFFER_HEADER;

typedef struct __GMM_GFX_PARTITIONING {
    struct
    {
        uint64_t Base, Limit;
    } Standard,
        Standard64KB,
        Reserved0,
        Reserved1,
        SVM,
        Reserved2,
        Heap32[4];
} GMM_GFX_PARTITIONING;

struct CREATECONTEXT_PVTDATA {
    unsigned long *pHwContextId;
    uint32_t NumberOfHwContextIds;

    uint32_t ProcessID;
    uint8_t IsProtectedProcess;
    uint8_t IsDwm;
    uint8_t IsMediaUsage;
    uint8_t GpuVAContext;
    BOOLEAN NoRingFlushes;
};

struct ADAPTER_INFO_KMD : ADAPTER_INFO_GMM {
    SKU_FEATURE_TABLE_KMD SkuTable;
    WA_TABLE_KMD WaTable;
    GMM_GFX_PARTITIONING GfxPartition;
    ADAPTER_BDF stAdapterBDF;
};

static constexpr COMMAND_BUFFER_HEADER initCommandBufferHeader(uint32_t umdContextType, uint32_t umdPatchList, uint32_t usesResourceStreamer, uint32_t perfTag) {
    COMMAND_BUFFER_HEADER ret = {};
    ret.UmdContextType = umdContextType;
    ret.UmdPatchList = umdPatchList;
    ret.UsesResourceStreamer = usesResourceStreamer;
    ret.PerfTag = perfTag;
    return ret;
}

#ifdef DECLARE_COMMAND_BUFFER
#undef DECLARE_COMMAND_BUFFER
#endif
#define DECLARE_COMMAND_BUFFER(VARNAME, CONTEXTTYPE, PATCHLIST, STREAMER, PERFTAG) \
    static constexpr COMMAND_BUFFER_HEADER VARNAME = initCommandBufferHeader(CONTEXTTYPE, PATCHLIST, STREAMER, PERFTAG);

inline void propagateData(ADAPTER_INFO_KMD &adapterInfo) {
    ADAPTER_INFO &base = static_cast<ADAPTER_INFO &>(adapterInfo);
    base.SkuTable = adapterInfo.SkuTable;
    base.WaTable = adapterInfo.WaTable;
}
#endif
