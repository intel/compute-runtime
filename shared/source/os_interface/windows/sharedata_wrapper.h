/*
 * Copyright (C) 2021-2025 Intel Corporation
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
using PLATFORM_GMM = PLATFORM;

#if defined(UMD_KMD_COMMAND_BUFFER_REV_ID)
using SKU_FEATURE_TABLE_KMD = SKU_FEATURE_TABLE_GMM;
using WA_TABLE_KMD = WA_TABLE_GMM;
using ADAPTER_INFO_KMD = ADAPTER_INFO_GMM;
using PLATFORM_KMD = PLATFORM_GMM;

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
struct SKU_FEATURE_TABLE_KMD : SKU_FEATURE_TABLE_GMM { // NOLINT(readability-identifier-naming)
    bool FtrGpGpuMidBatchPreempt : 1;                  // NOLINT(readability-identifier-naming)
    bool FtrGpGpuThreadGroupLevelPreempt : 1;          // NOLINT(readability-identifier-naming)
    bool FtrGpGpuMidThreadLevelPreempt : 1;            // NOLINT(readability-identifier-naming)

    bool FtrWddm2Svm : 1;        // NOLINT(readability-identifier-naming)
    bool FtrPooledEuEnabled : 1; // NOLINT(readability-identifier-naming)

    bool FtrPPGTT : 1;         // NOLINT(readability-identifier-naming)
    bool FtrSVM : 1;           // NOLINT(readability-identifier-naming)
    bool FtrEDram : 1;         // NOLINT(readability-identifier-naming)
    bool FtrL3IACoherency : 1; // NOLINT(readability-identifier-naming)
    bool FtrIA32eGfxPTEs : 1;  // NOLINT(readability-identifier-naming)

    bool FtrTileY : 1;                    // NOLINT(readability-identifier-naming)
    bool FtrDisplayYTiling : 1;           // NOLINT(readability-identifier-naming)
    bool FtrTranslationTable : 1;         // NOLINT(readability-identifier-naming)
    bool FtrUserModeTranslationTable : 1; // NOLINT(readability-identifier-naming)

    bool FtrFbc : 1; // NOLINT(readability-identifier-naming)

    bool FtrULT : 1;                // NOLINT(readability-identifier-naming)
    bool FtrLCIA : 1;               // NOLINT(readability-identifier-naming)
    bool FtrTileMappedResource : 1; // NOLINT(readability-identifier-naming)
    bool FtrAstcHdr2D : 1;          // NOLINT(readability-identifier-naming)
    bool FtrAstcLdr2D : 1;          // NOLINT(readability-identifier-naming)

    bool FtrStandardMipTailFormat : 1; // NOLINT(readability-identifier-naming)
    bool FtrFrameBufferLLC : 1;        // NOLINT(readability-identifier-naming)
    bool FtrLLCBypass : 1;             // NOLINT(readability-identifier-naming)
    bool FtrDisplayEngineS3d : 1;      // NOLINT(readability-identifier-naming)
    bool FtrWddm2GpuMmu : 1;           // NOLINT(readability-identifier-naming)
    bool FtrWddm2_1_64kbPages : 1;     // NOLINT(readability-identifier-naming)
    bool FtrWddmHwQueues : 1;          // NOLINT(readability-identifier-naming)
    bool FtrMemTypeMocsDeferPAT : 1;   // NOLINT(readability-identifier-naming)

    bool FtrKmdDaf : 1; // NOLINT(readability-identifier-naming)

    bool FtrE2ECompression : 1;       // NOLINT(readability-identifier-naming)
    bool FtrLinearCCS : 1;            // NOLINT(readability-identifier-naming)
    bool FtrCCSRing : 1;              // NOLINT(readability-identifier-naming)
    bool FtrCCSNode : 1;              // NOLINT(readability-identifier-naming)
    bool FtrRcsNode : 1;              // NOLINT(readability-identifier-naming)
    bool FtrLocalMemory : 1;          // NOLINT(readability-identifier-naming)
    bool FtrLocalMemoryAllows4KB : 1; // NOLINT(readability-identifier-naming)

    bool FtrHwScheduling : 1; // NOLINT(readability-identifier-naming)
    bool FtrWalkerMTP : 1;    // NOLINT(readability-identifier-naming)
};

struct WA_TABLE_KMD : WA_TABLE_GMM { // NOLINT(readability-identifier-naming)

    bool WaSendMIFLUSHBeforeVFE = false;                            // NOLINT(readability-identifier-naming)
    bool WaDisableLSQCROPERFforOCL = false;                         // NOLINT(readability-identifier-naming)
    bool WaMsaa8xTileYDepthPitchAlignment = false;                  // NOLINT(readability-identifier-naming)
    bool WaLosslessCompressionSurfaceStride = false;                // NOLINT(readability-identifier-naming)
    bool WaFbcLinearSurfaceStride = false;                          // NOLINT(readability-identifier-naming)
    bool Wa4kAlignUVOffsetNV12LinearSurface = false;                // NOLINT(readability-identifier-naming)
    bool WaEncryptedEdramOnlyPartials = false;                      // NOLINT(readability-identifier-naming)
    bool WaDisableEdramForDisplayRT = false;                        // NOLINT(readability-identifier-naming)
    bool WaCompressedResourceRequiresConstVA21 = false;             // NOLINT(readability-identifier-naming)
    bool WaDisablePerCtxtPreemptionGranularityControl = false;      // NOLINT(readability-identifier-naming)
    bool WaLLCCachingUnsupported = false;                           // NOLINT(readability-identifier-naming)
    bool WaUseVAlign16OnTileXYBpp816 = false;                       // NOLINT(readability-identifier-naming)
    bool WaModifyVFEStateAfterGPGPUPreemption = false;              // NOLINT(readability-identifier-naming)
    bool WaCSRUncachable = false;                                   // NOLINT(readability-identifier-naming)
    bool WaSamplerCacheFlushBetweenRedescribedSurfaceReads = false; // NOLINT(readability-identifier-naming)
    bool WaRestrictPitch128KB = false;                              // NOLINT(readability-identifier-naming)
    bool WaLimit128BMediaCompr = false;                             // NOLINT(readability-identifier-naming)
    bool WaUntypedBufferCompression = false;                        // NOLINT(readability-identifier-naming)
    bool WaAuxTable16KGranular = false;                             // NOLINT(readability-identifier-naming)
    bool WaDisableFusedThreadScheduling = false;                    // NOLINT(readability-identifier-naming)
};

typedef struct COMMAND_BUFFER_HEADER_REC { // NOLINT(readability-identifier-naming)
    uint32_t UmdContextType : 4;           // NOLINT(readability-identifier-naming)
    uint32_t UmdPatchList : 1;             // NOLINT(readability-identifier-naming)

    uint32_t UmdRequestedSliceState : 3;    // NOLINT(readability-identifier-naming)
    uint32_t UmdRequestedSubsliceCount : 3; // NOLINT(readability-identifier-naming)
    uint32_t UmdRequestedEUCount : 5;       // NOLINT(readability-identifier-naming)

    uint32_t UsesResourceStreamer : 1;           // NOLINT(readability-identifier-naming)
    uint32_t NeedsMidBatchPreEmptionSupport : 1; // NOLINT(readability-identifier-naming)
    uint32_t UsesGPGPUPipeline : 1;              // NOLINT(readability-identifier-naming)
    uint32_t RequiresCoherency : 1;              // NOLINT(readability-identifier-naming)

    uint32_t PerfTag;           // NOLINT(readability-identifier-naming)
    uint64_t MonitorFenceVA;    // NOLINT(readability-identifier-naming)
    uint64_t MonitorFenceValue; // NOLINT(readability-identifier-naming)
} COMMAND_BUFFER_HEADER;

typedef struct __GMM_GFX_PARTITIONING {
    struct
    {
        uint64_t Base, Limit; // NOLINT(readability-identifier-naming)
    } Standard,               // NOLINT(readability-identifier-naming)
        Standard64KB,         // NOLINT(readability-identifier-naming)
        Reserved0,            // NOLINT(readability-identifier-naming)
        Reserved1,            // NOLINT(readability-identifier-naming)
        SVM,                  // NOLINT(readability-identifier-naming)
        TR,                   // NOLINT(readability-identifier-naming)
        Heap32[4];            // NOLINT(readability-identifier-naming)
} GMM_GFX_PARTITIONING;

struct CREATECONTEXT_PVTDATA { // NOLINT(readability-identifier-naming)
    unsigned long *pHwContextId;
    uint32_t NumberOfHwContextIds; // NOLINT(readability-identifier-naming)

    uint32_t ProcessID;              // NOLINT(readability-identifier-naming)
    uint8_t IsProtectedProcess;      // NOLINT(readability-identifier-naming)
    uint8_t IsDwm;                   // NOLINT(readability-identifier-naming)
    uint8_t IsMediaUsage;            // NOLINT(readability-identifier-naming)
    uint8_t GpuVAContext;            // NOLINT(readability-identifier-naming)
    BOOLEAN NoRingFlushes;           // NOLINT(readability-identifier-naming)
    BOOLEAN DummyPageBackingEnabled; // NOLINT(readability-identifier-naming)
    uint32_t UmdContextType;         // NOLINT(readability-identifier-naming)
};

struct PLATFORM_KMD : PLATFORM_GMM { // NOLINT(readability-identifier-naming)

    struct HwIpVersion {
        uint32_t Value; // NOLINT(readability-identifier-naming)
    };

    HwIpVersion sRenderBlockID;
};

struct ADAPTER_INFO_KMD : ADAPTER_INFO_GMM { // NOLINT(readability-identifier-naming)
    SKU_FEATURE_TABLE_KMD SkuTable;          // NOLINT(readability-identifier-naming)
    WA_TABLE_KMD WaTable;                    // NOLINT(readability-identifier-naming)
    GMM_GFX_PARTITIONING GfxPartition;       // NOLINT(readability-identifier-naming)
    ADAPTER_BDF stAdapterBDF;
    PLATFORM_KMD GfxPlatform; // NOLINT(readability-identifier-naming)
    uint64_t LMemBarSize;     // NOLINT(readability-identifier-naming)
    uint8_t SegmentId[3];     // NOLINT(readability-identifier-naming)
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
    base.GfxPlatform = adapterInfo.GfxPlatform;
}
#endif
