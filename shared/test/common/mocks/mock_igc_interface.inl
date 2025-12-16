/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "ocl_igc_interface/gt_system_info.h"
#include "ocl_igc_interface/igc_features_and_workarounds.h"
#include "ocl_igc_interface/platform.h"

namespace IGC {
#include "cif/macros/enable.h"
#define DEFINE_GET_SET(INTERFACE, VERSION, NAME, TYPE)                                      \
    TYPE CIF_GET_INTERFACE_CLASS(INTERFACE, VERSION)::Get##NAME() const { return (TYPE)0; } \
    void CIF_GET_INTERFACE_CLASS(INTERFACE, VERSION)::Set##NAME(TYPE v) {}

DEFINE_GET_SET(Platform, 1, ProductFamily, TypeErasedEnum);
DEFINE_GET_SET(Platform, 1, PCHProductFamily, TypeErasedEnum);
DEFINE_GET_SET(Platform, 1, DisplayCoreFamily, TypeErasedEnum);
DEFINE_GET_SET(Platform, 1, RenderCoreFamily, TypeErasedEnum);
DEFINE_GET_SET(Platform, 1, PlatformType, TypeErasedEnum);
DEFINE_GET_SET(Platform, 1, DeviceID, unsigned short);
DEFINE_GET_SET(Platform, 1, RevId, unsigned short);
DEFINE_GET_SET(Platform, 1, DeviceID_PCH, unsigned short);
DEFINE_GET_SET(Platform, 1, RevId_PCH, unsigned short);
DEFINE_GET_SET(Platform, 1, GTType, TypeErasedEnum);

DEFINE_GET_SET(Platform, 2, RenderBlockID, unsigned int);
DEFINE_GET_SET(Platform, 2, MediaBlockID, unsigned int);
DEFINE_GET_SET(Platform, 2, DisplayBlockID, unsigned int);

DEFINE_GET_SET(GTSystemInfo, 1, EUCount, uint32_t);
DEFINE_GET_SET(GTSystemInfo, 1, ThreadCount, uint32_t);
DEFINE_GET_SET(GTSystemInfo, 1, SliceCount, uint32_t);
DEFINE_GET_SET(GTSystemInfo, 1, SubSliceCount, uint32_t);
DEFINE_GET_SET(GTSystemInfo, 1, L3CacheSizeInKb, uint64_t);
DEFINE_GET_SET(GTSystemInfo, 1, LLCCacheSizeInKb, uint64_t);
DEFINE_GET_SET(GTSystemInfo, 1, EdramSizeInKb, uint64_t);
DEFINE_GET_SET(GTSystemInfo, 1, L3BankCount, uint32_t);
DEFINE_GET_SET(GTSystemInfo, 1, MaxFillRate, uint32_t);
DEFINE_GET_SET(GTSystemInfo, 1, EuCountPerPoolMax, uint32_t);
DEFINE_GET_SET(GTSystemInfo, 1, EuCountPerPoolMin, uint32_t);
DEFINE_GET_SET(GTSystemInfo, 1, TotalVsThreads, uint32_t);
DEFINE_GET_SET(GTSystemInfo, 1, TotalHsThreads, uint32_t);
DEFINE_GET_SET(GTSystemInfo, 1, TotalDsThreads, uint32_t);
DEFINE_GET_SET(GTSystemInfo, 1, TotalGsThreads, uint32_t);
DEFINE_GET_SET(GTSystemInfo, 1, TotalPsThreadsWindowerRange, uint32_t);
DEFINE_GET_SET(GTSystemInfo, 1, CsrSizeInMb, uint32_t);
DEFINE_GET_SET(GTSystemInfo, 1, MaxEuPerSubSlice, uint32_t);
DEFINE_GET_SET(GTSystemInfo, 1, MaxSlicesSupported, uint32_t);
DEFINE_GET_SET(GTSystemInfo, 1, MaxSubSlicesSupported, uint32_t);
DEFINE_GET_SET(GTSystemInfo, 1, IsL3HashModeEnabled, bool);
DEFINE_GET_SET(GTSystemInfo, 1, IsDynamicallyPopulated, bool);

DEFINE_GET_SET(GTSystemInfo, 3, DualSubSliceCount, uint32_t);
DEFINE_GET_SET(GTSystemInfo, 3, MaxDualSubSlicesSupported, uint32_t);

DEFINE_GET_SET(GTSystemInfo, 4, SLMSizeInKb, uint32_t);

DEFINE_GET_SET(IgcFeaturesAndWorkarounds, 1, FtrDesktop, bool);
DEFINE_GET_SET(IgcFeaturesAndWorkarounds, 1, FtrChannelSwizzlingXOREnabled, bool);
DEFINE_GET_SET(IgcFeaturesAndWorkarounds, 1, FtrGtBigDie, bool);
DEFINE_GET_SET(IgcFeaturesAndWorkarounds, 1, FtrGtMediumDie, bool);
DEFINE_GET_SET(IgcFeaturesAndWorkarounds, 1, FtrGtSmallDie, bool);
DEFINE_GET_SET(IgcFeaturesAndWorkarounds, 1, FtrGT1, bool);
DEFINE_GET_SET(IgcFeaturesAndWorkarounds, 1, FtrGT1_5, bool);
DEFINE_GET_SET(IgcFeaturesAndWorkarounds, 1, FtrGT2, bool);
DEFINE_GET_SET(IgcFeaturesAndWorkarounds, 1, FtrGT3, bool);
DEFINE_GET_SET(IgcFeaturesAndWorkarounds, 1, FtrGT4, bool);
DEFINE_GET_SET(IgcFeaturesAndWorkarounds, 1, FtrIVBM0M1Platform, bool);
DEFINE_GET_SET(IgcFeaturesAndWorkarounds, 1, FtrGTL, bool);
DEFINE_GET_SET(IgcFeaturesAndWorkarounds, 1, FtrGTM, bool);
DEFINE_GET_SET(IgcFeaturesAndWorkarounds, 1, FtrGTH, bool);
DEFINE_GET_SET(IgcFeaturesAndWorkarounds, 1, FtrSGTPVSKUStrapPresent, bool);
DEFINE_GET_SET(IgcFeaturesAndWorkarounds, 1, FtrGTA, bool);
DEFINE_GET_SET(IgcFeaturesAndWorkarounds, 1, FtrGTC, bool);
DEFINE_GET_SET(IgcFeaturesAndWorkarounds, 1, FtrGTX, bool);
DEFINE_GET_SET(IgcFeaturesAndWorkarounds, 1, Ftr5Slice, bool);
DEFINE_GET_SET(IgcFeaturesAndWorkarounds, 1, FtrGpGpuMidThreadLevelPreempt, bool);
DEFINE_GET_SET(IgcFeaturesAndWorkarounds, 1, FtrIoMmuPageFaulting, bool);
DEFINE_GET_SET(IgcFeaturesAndWorkarounds, 1, FtrWddm2Svm, bool);
DEFINE_GET_SET(IgcFeaturesAndWorkarounds, 1, FtrPooledEuEnabled, bool);
DEFINE_GET_SET(IgcFeaturesAndWorkarounds, 1, FtrResourceStreamer, bool);

DEFINE_GET_SET(IgcFeaturesAndWorkarounds, 2, MaxOCLParamSize, uint32_t);

#undef DEFINE_GET_SET
#include "cif/macros/disable.h"
} // namespace IGC
