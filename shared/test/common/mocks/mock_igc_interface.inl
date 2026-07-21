/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "ocl_igc_interface/fcl_ocl_device_ctx.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"

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

void CIF_GET_INTERFACE_CLASS(IgcOclDeviceCtx, 1)::SetProfilingTimerResolution(float v) {}

PlatformBase *CIF_GET_INTERFACE_CLASS(IgcOclDeviceCtx, 1)::GetPlatformHandleImpl(CIF::Version_t ver) {
    return nullptr;
}

GTSystemInfoBase *CIF_GET_INTERFACE_CLASS(IgcOclDeviceCtx, 1)::GetGTSystemInfoHandleImpl(CIF::Version_t ver) {
    return nullptr;
}

IgcFeaturesAndWorkaroundsBase *CIF_GET_INTERFACE_CLASS(IgcOclDeviceCtx, 1)::GetIgcFeaturesAndWorkaroundsHandleImpl(CIF::Version_t ver) {
    return nullptr;
}

IgcOclTranslationCtxBase *CIF_GET_INTERFACE_CLASS(IgcOclDeviceCtx, 1)::CreateTranslationCtxImpl(CIF::Version_t ver,
                                                                                                CodeType::CodeType_t inType,
                                                                                                CodeType::CodeType_t outType) {
    return nullptr;
}

bool CIF_GET_INTERFACE_CLASS(IgcOclDeviceCtx, 2)::GetSystemRoutine(IGC::SystemRoutineType::SystemRoutineType_t typeOfSystemRoutine,
                                                                   bool bindless,
                                                                   CIF::Builtins::BufferSimple *outSystemRoutineBuffer,
                                                                   CIF::Builtins::BufferSimple *stateSaveAreaHeaderInit) {
    return true;
}

const char *CIF_GET_INTERFACE_CLASS(IgcOclDeviceCtx, 3)::GetIGCRevision() {
    return "";
}

IgcBuiltinsBase *CIF_GET_INTERFACE_CLASS(IgcOclDeviceCtx, 4)::GetIgcBuiltinsHandleImpl(CIF::Version_t ver) {
    return nullptr;
}

IgcOptionsAndCapabilitiesBase *CIF_GET_INTERFACE_CLASS(IgcOclDeviceCtx, 5)::GetIgcOptionsAndCapabilitiesHandleImpl(CIF::Version_t ver) {
    return nullptr;
}

OclTranslationOutputBase *CIF_GET_INTERFACE_CLASS(IgcOclTranslationCtx, 1)::TranslateImpl(
    CIF::Version_t outVersion,
    CIF::Builtins::BufferSimple *src,
    CIF::Builtins::BufferSimple *options,
    CIF::Builtins::BufferSimple *internalOptions,
    CIF::Builtins::BufferSimple *tracingOptions,
    uint32_t tracingOptionsCount) {
    return nullptr;
}

OclTranslationOutputBase *CIF_GET_INTERFACE_CLASS(IgcOclTranslationCtx, 2)::TranslateImpl(
    CIF::Version_t outVersion,
    CIF::Builtins::BufferSimple *src,
    CIF::Builtins::BufferSimple *options,
    CIF::Builtins::BufferSimple *internalOptions,
    CIF::Builtins::BufferSimple *tracingOptions,
    uint32_t tracingOptionsCount,
    void *gtPinInput) {
    return nullptr;
}

bool CIF_GET_INTERFACE_CLASS(IgcOclTranslationCtx, 3)::GetSpecConstantsInfoImpl(
    CIF::Builtins::BufferSimple *src,
    CIF::Builtins::BufferSimple *outSpecConstantsIds,
    CIF::Builtins::BufferSimple *outSpecConstantsSizes) {
    return true;
}

OclTranslationOutputBase *CIF_GET_INTERFACE_CLASS(IgcOclTranslationCtx, 3)::TranslateImpl(
    CIF::Version_t outVersion,
    CIF::Builtins::BufferSimple *src,
    CIF::Builtins::BufferSimple *specConstantsIds,
    CIF::Builtins::BufferSimple *specConstantsValues,
    CIF::Builtins::BufferSimple *options,
    CIF::Builtins::BufferSimple *internalOptions,
    CIF::Builtins::BufferSimple *tracingOptions,
    uint32_t tracingOptionsCount,
    void *gtPinInput) {
    return nullptr;
}

bool CIF_GET_INTERFACE_CLASS(OclTranslationOutput, 1)::Successful() const {
    return true;
}

bool CIF_GET_INTERFACE_CLASS(OclTranslationOutput, 1)::HasWarnings() const {
    return false;
}

CIF::Builtins::BufferBase *CIF_GET_INTERFACE_CLASS(OclTranslationOutput, 1)::GetBuildLogImpl(CIF::Version_t bufferVersion) {
    return nullptr;
}

CIF::Builtins::BufferBase *CIF_GET_INTERFACE_CLASS(OclTranslationOutput, 1)::GetOutputImpl(CIF::Version_t bufferVersion) {
    return nullptr;
}

CIF::Builtins::BufferBase *CIF_GET_INTERFACE_CLASS(OclTranslationOutput, 1)::GetDebugDataImpl(CIF::Version_t bufferVersion) {
    return nullptr;
}

CodeType::CodeType_t CIF_GET_INTERFACE_CLASS(OclTranslationOutput, 1)::GetOutputType() const {
    return IGC::CodeType::undefined;
}

IGC::OclTranslationOutputBase *CIF_GET_INTERFACE_CLASS(FclOclTranslationCtx, 1)::TranslateImpl(
    CIF::Version_t outVersion,
    CIF::Builtins::BufferSimple *src,
    CIF::Builtins::BufferSimple *options,
    CIF::Builtins::BufferSimple *internalOptions,
    CIF::Builtins::BufferSimple *tracingOptions,
    uint32_t tracingOptionsCount) {
    return nullptr;
}

void CIF_GET_INTERFACE_CLASS(FclOclTranslationCtx, 2)::GetFclOptions(CIF::Builtins::BufferSimple *options) {
}

void CIF_GET_INTERFACE_CLASS(FclOclTranslationCtx, 2)::GetFclInternalOptions(CIF::Builtins::BufferSimple *internalOptions) {
}

void CIF_GET_INTERFACE_CLASS(FclOclDeviceCtx, 1)::SetOclApiVersion(uint32_t version) {}

IGC::FclOclTranslationCtxBase *CIF_GET_INTERFACE_CLASS(FclOclDeviceCtx, 1)::CreateTranslationCtxImpl(CIF::Version_t ver,
                                                                                                     IGC::CodeType::CodeType_t inType,
                                                                                                     IGC::CodeType::CodeType_t outType) {
    return nullptr;
}

CodeType::CodeType_t CIF_GET_INTERFACE_CLASS(FclOclDeviceCtx, 2)::GetPreferredIntermediateRepresentation() {
    return CodeType::spirV;
}

IGC::FclOclTranslationCtxBase *CIF_GET_INTERFACE_CLASS(FclOclDeviceCtx, 3)::CreateTranslationCtxImpl(CIF::Version_t ver,
                                                                                                     IGC::CodeType::CodeType_t inType,
                                                                                                     IGC::CodeType::CodeType_t outType,
                                                                                                     CIF::Builtins::BufferSimple *err) {
    return nullptr;
}

IGC::PlatformBase *CIF_GET_INTERFACE_CLASS(FclOclDeviceCtx, 4)::GetPlatformHandleImpl(CIF::Version_t ver) {
    return nullptr;
}

void CIF_GET_INTERFACE_CLASS(IgcOptionsAndCapabilities, 1)::GetCompilerAPIOptionsHelp(CIF::Builtins::BufferSimple *outAPIOptions) const {
}

void CIF_GET_INTERFACE_CLASS(IgcOptionsAndCapabilities, 1)::GetCompilerInternalOptionsHelp(CIF::Builtins::BufferSimple *outInternalOptions) const {
}

void CIF_GET_INTERFACE_CLASS(IgcOptionsAndCapabilities, 1)::GetCompilerSupportedSPIRVExtensionsYAML(CIF::Builtins::BufferSimple *
                                                                                                        outSupportedSPIRVExtensionsYAML) const {
}

#include "cif/macros/disable.h"
} // namespace IGC
