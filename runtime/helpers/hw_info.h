/*
 * Copyright (c) 2017, Intel Corporation
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
#include "igfxfmid.h"
#include "gtsysinfo.h"

namespace OCLRT {

enum PreemptionMode {
    // Keep in sync with ForcePreemptionMode debug variable
    Disabled = 1,
    MidBatch,
    ThreadGroup,
    MidThread
};

struct WhitelistedRegisters {
    bool csChicken1_0x2580;
    bool chicken0hdc_0xE5F0;
};

struct RuntimeCapabilityTable {
    uint32_t maxRenderFrequency;
    double defaultProfilingTimerResolution;

    unsigned int clVersionSupport;
    bool ftrSupportsFP64;
    bool ftrSupports64BitMath;
    bool ftrSvm;
    bool ftrSupportsCoherency;
    bool ftrSupportsVmeAvcTextureSampler;
    bool ftrSupportsVmeAvcPreemption;
    bool ftrCompression;
    PreemptionMode defaultPreemptionMode;
    WhitelistedRegisters whitelistedRegisters;

    bool (*isSimulation)(unsigned short);
    bool instrumentationEnabled;

    bool forceStatelessCompilationFor32Bit;

    bool enableKmdNotify;
    int64_t delayKmdNotifyMs;
    bool ftr64KBpages;

    int32_t nodeOrdinal;
};

struct FeatureTable {
    bool ftrDesktop = false;
    bool ftrChannelSwizzlingXOREnabled = false;

    bool ftrGtBigDie = false;
    bool ftrGtMediumDie = false;
    bool ftrGtSmallDie = false;

    bool ftrGT1 = false;
    bool ftrGT1_5 = false;
    bool ftrGT2 = false;
    bool ftrGT2_5 = false;
    bool ftrGT3 = false;
    bool ftrGT4 = false;

    bool ftrIVBM0M1Platform = false;
    bool ftrSGTPVSKUStrapPresent = false;
    bool ftrGTA = false;
    bool ftrGTC = false;
    bool ftrGTX = false;
    bool ftr5Slice = false;

    bool ftrGpGpuMidBatchPreempt = false;
    bool ftrGpGpuThreadGroupLevelPreempt = false;
    bool ftrGpGpuMidThreadLevelPreempt = false;

    bool ftrIoMmuPageFaulting = false;
    bool ftrWddm2Svm = false;
    bool ftrPooledEuEnabled = false;

    bool ftrResourceStreamer = false;

    bool ftrPPGTT = false;
    bool ftrSVM = false;
    bool ftrEDram = false;
    bool ftrL3IACoherency = false;
    bool ftrIA32eGfxPTEs = false;

    bool ftr3dMidBatchPreempt = false;
    bool ftr3dObjectLevelPreempt = false;
    bool ftrPerCtxtPreemptionGranularityControl = false;

    bool ftrDisplayYTiling = false;
    bool ftrTranslationTable = false;
    bool ftrUserModeTranslationTable = false;

    bool ftrEnableGuC = false;

    bool ftrFbc = false;
    bool ftrFbc2AddressTranslation = false;
    bool ftrFbcBlitterTracking = false;
    bool ftrFbcCpuTracking = false;

    bool ftrVcs2 = false;
    bool ftrVEBOX = false;
    bool ftrSingleVeboxSlice = false;
    bool ftrULT = false;
    bool ftrLCIA = false;
    bool ftrGttCacheInvalidation = false;
    bool ftrTileMappedResource = false;
    bool ftrAstcHdr2D = false;
    bool ftrAstcLdr2D = false;

    bool ftrStandardMipTailFormat = false; // Gmmlib
    bool ftrFrameBufferLLC = false;        // Gmmlib
    bool ftrCrystalwell = false;           // Gmmlib
    bool ftrLLCBypass = false;             // Gmmlib
    bool ftrDisplayEngineS3d = false;      // Gmmlib
    bool ftrVERing = false;                // Gmmlib
};

struct WorkaroundTable {
    bool waDoNotUseMIReportPerfCount = false;

    bool waEnablePreemptionGranularityControlByUMD = false;
    bool waSendMIFLUSHBeforeVFE = false;
    bool waReportPerfCountUseGlobalContextID = false;
    bool waDisableLSQCROPERFforOCL = false;
    bool waMsaa8xTileYDepthPitchAlignment = false;
    bool waLosslessCompressionSurfaceStride = false;
    bool waFbcLinearSurfaceStride = false; // Gmmlib
    bool wa4kAlignUVOffsetNV12LinearSurface = false;
    bool waEncryptedEdramOnlyPartials = false; // Gmmlib
    bool waDisableEdramForDisplayRT = false;   // Gmmlib
    bool waForcePcBbFullCfgRestore = false;
    bool waCompressedResourceRequiresConstVA21 = false;
    bool waDisablePerCtxtPreemptionGranularityControl = false;
    bool waLLCCachingUnsupported = false;
    bool waUseVAlign16OnTileXYBpp816 = false;
    bool waModifyVFEStateAfterGPGPUPreemption = false;
    bool waCSRUncachable = false;
};

struct HardwareInfo {
    const PLATFORM *pPlatform;
    const FeatureTable *pSkuTable;
    const WorkaroundTable *pWaTable;
    const GT_SYSTEM_INFO *pSysInfo;

    RuntimeCapabilityTable capabilityTable;
};

extern const WorkaroundTable emptyWaTable;
extern const FeatureTable emptySkuTable;
extern const HardwareInfo unknownHardware;

template <PRODUCT_FAMILY product>
struct HwMapper {};

template <GFXCORE_FAMILY gfxFamily>
struct GfxFamilyMapper {};

// Global table of hardware prefixes
extern bool familyEnabled[IGFX_MAX_CORE];
extern const char *familyName[IGFX_MAX_CORE];
extern const char *hardwarePrefix[];
extern const HardwareInfo *hardwareInfoTable[IGFX_MAX_PRODUCT];
extern void (*hardwareInfoSetupGt[IGFX_MAX_PRODUCT])(GT_SYSTEM_INFO *);

template <GFXCORE_FAMILY gfxFamily>
struct EnableGfxFamilyHw {
    EnableGfxFamilyHw() {
        familyEnabled[gfxFamily] = true;
        familyName[gfxFamily] = GfxFamilyMapper<gfxFamily>::name;
    }
};

} // namespace OCLRT
