/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <array>
#include <bitset>
#include <cstdint>

namespace NEO {
constexpr size_t bcsInfoMaskSize = 9u;
using BcsInfoMask = std::bitset<bcsInfoMaskSize>;

struct FeatureTableBase {
  public:
    FeatureTableBase() {
        flags.ftrRcsNode = 1;
    }

    struct Flags {
        // DW0
        uint32_t ftrGpGpuMidBatchPreempt : 1;
        uint32_t ftrGpGpuThreadGroupLevelPreempt : 1;
        uint32_t ftrWddm2Svm : 1;
        uint32_t ftrPooledEuEnabled : 1;
        uint32_t ftrPPGTT : 1;
        uint32_t ftrSVM : 1;
        uint32_t ftrEDram : 1;
        uint32_t ftrL3IACoherency : 1;
        uint32_t ftrIA32eGfxPTEs : 1;
        uint32_t ftrTileY : 1;
        uint32_t ftrDisplayYTiling : 1;
        uint32_t ftrTranslationTable : 1;
        uint32_t ftrUserModeTranslationTable : 1;
        uint32_t ftrFbc : 1;
        uint32_t ftrULT : 1;
        uint32_t ftrLCIA : 1;
        uint32_t ftrTileMappedResource : 1;
        uint32_t ftrAstcHdr2D : 1;
        uint32_t ftrAstcLdr2D : 1;
        uint32_t ftrStandardMipTailFormat : 1;
        uint32_t ftrFrameBufferLLC : 1;
        uint32_t ftrLLCBypass : 1;
        uint32_t ftrDisplayEngineS3d : 1;
        uint32_t ftrWddm2GpuMmu : 1;
        uint32_t ftrWddm2_1_64kbPages : 1; // NOLINT(readability-identifier-naming)
        uint32_t ftrWddmHwQueues : 1;
        uint32_t ftrMemTypeMocsDeferPAT : 1;
        uint32_t ftrKmdDaf : 1;
        uint32_t ftrE2ECompression : 1;
        uint32_t ftrLinearCCS : 1;
        uint32_t ftrCCSRing : 1;
        uint32_t ftrCCSNode : 1;
        // DW1
        uint32_t ftrRcsNode : 1;
        uint32_t ftrLocalMemory : 1;
        uint32_t ftrLocalMemoryAllows4KB : 1;
        uint32_t ftrFlatPhysCCS : 1;
        uint32_t ftrMultiTileArch : 1;
        uint32_t ftrPpgtt64KBWalkOptimization : 1;
        uint32_t ftrUnified3DMediaCompressionFormats : 1;
        uint32_t ftr57bGPUAddressing : 1;
        uint32_t ftrTile64Optimization : 1;
        uint32_t ftrWalkerMTP : 1;
        uint32_t ftrXe2Compression : 1;
        uint32_t ftrXe2PlusTiling : 1;
        uint32_t ftrL3TransientDataFlush : 1;
        uint32_t ftrPml5Support : 1;
        uint32_t ftrHeaplessMode : 1;
        uint32_t reserved : 17;
    };

    BcsInfoMask ftrBcsInfo = 1;
    uint32_t regionCount = 1;

    union {
        Flags flags;
        std::array<uint32_t, 2> packed = {};
    };
};

static_assert(sizeof(FeatureTableBase::flags) == sizeof(FeatureTableBase::packed));

struct WorkaroundTableBase {

    struct Flags {
        // DW0
        uint32_t waSendMIFLUSHBeforeVFE : 1;
        uint32_t waDisableLSQCROPERFforOCL : 1;
        uint32_t waMsaa8xTileYDepthPitchAlignment : 1;
        uint32_t waLosslessCompressionSurfaceStride : 1;
        uint32_t waFbcLinearSurfaceStride : 1;
        uint32_t wa4kAlignUVOffsetNV12LinearSurface : 1;
        uint32_t waEncryptedEdramOnlyPartials : 1;
        uint32_t waDisableEdramForDisplayRT : 1;
        uint32_t waCompressedResourceRequiresConstVA21 : 1;
        uint32_t waDisablePerCtxtPreemptionGranularityControl : 1;
        uint32_t waLLCCachingUnsupported : 1;
        uint32_t waUseVAlign16OnTileXYBpp816 : 1;
        uint32_t waModifyVFEStateAfterGPGPUPreemption : 1;
        uint32_t waCSRUncachable : 1;
        uint32_t waSamplerCacheFlushBetweenRedescribedSurfaceReads : 1;
        uint32_t waRestrictPitch128KB : 1;
        uint32_t waLimit128BMediaCompr : 1;
        uint32_t waUntypedBufferCompression : 1;
        uint32_t waAuxTable16KGranular : 1;
        uint32_t waDisableFusedThreadScheduling : 1;
        uint32_t waAuxTable64KGranular : 1;
        uint32_t wa_15010089951 : 1; // NOLINT(readability-identifier-naming)
        uint32_t wa_14018976079 : 1; // NOLINT(readability-identifier-naming)
        uint32_t wa_14018984349 : 1; // NOLINT(readability-identifier-naming)
        uint32_t reserved : 8;
    };

    union {
        Flags flags;
        std::array<uint32_t, 1> packed = {};
    };
};

static_assert(sizeof(WorkaroundTableBase::flags) == sizeof(WorkaroundTableBase::packed));
} // namespace NEO
