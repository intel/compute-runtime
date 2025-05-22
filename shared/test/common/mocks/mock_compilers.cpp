/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_compilers.h"

#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/os_inc_base.h"
#include "shared/test/common/helpers/mock_file_io.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/mocks/mock_compiler_interface.h"
#include "shared/test/common/mocks/mock_sip.h"

#include "cif/macros/enable.h"
#include "common/StateSaveAreaHeader.h"
#include "ocl_igc_interface/fcl_ocl_device_ctx.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"

#include <filesystem>
#include <fstream>
#include <map>
namespace NEO {

std::unique_ptr<MockCompilerDebugVars> fclDebugVars;
std::unique_ptr<MockCompilerDebugVars> igcDebugVars;

void setFclDebugVars(MockCompilerDebugVars &dbgv) {
    fclDebugVars.reset(new MockCompilerDebugVars(dbgv));
}

void setIgcDebugVars(MockCompilerDebugVars &dbgv) {
    igcDebugVars.reset(new MockCompilerDebugVars(dbgv));
}

MockCompilerDebugVars getFclDebugVars() {
    if (fclDebugVars == nullptr) {
        fclDebugVars.reset(new MockCompilerDebugVars());
    }
    return *fclDebugVars.get();
}

MockCompilerDebugVars getIgcDebugVars() {
    if (igcDebugVars == nullptr) {
        igcDebugVars.reset(new MockCompilerDebugVars());
    }
    return *igcDebugVars.get();
}

void clearFclDebugVars() {
    fclDebugVars.reset();
}
void clearIgcDebugVars() {
    igcDebugVars.reset();
}

MockCompilerEnableGuard::MockCompilerEnableGuard(bool autoEnable) {
    if (autoEnable) {
        Enable();
    }
}

MockCompilerEnableGuard::~MockCompilerEnableGuard() {
    Disable();
}

void MockCompilerEnableGuard::Enable() {
    if (enabled == false) {
        // load mock from self (don't load dynamic libraries)
        this->oldFclDllName = Os::frontEndDllName;
        Os::frontEndDllName = "";
        igcNameGuard = pushIgcDllName("");
        MockCIFMain::setGlobalCreatorFunc<NEO::MockIgcOclDeviceCtx>(NEO::MockIgcOclDeviceCtx::Create);
        MockCIFMain::setGlobalCreatorFunc<NEO::MockFclOclDeviceCtx>(NEO::MockFclOclDeviceCtx::Create);
        if (fclDebugVars == nullptr) {
            fclDebugVars.reset(new MockCompilerDebugVars);
        }
        if (igcDebugVars == nullptr) {
            igcDebugVars.reset(new MockCompilerDebugVars);
        }

        enabled = true;
    }
}

void MockCompilerEnableGuard::Disable() {
    if (enabled) {
        Os::frontEndDllName = this->oldFclDllName;
        igcNameGuard.reset();

        MockCIFMain::removeGlobalCreatorFunc<NEO::MockIgcOclDeviceCtx>();
        MockCIFMain::removeGlobalCreatorFunc<NEO::MockFclOclDeviceCtx>();

        clearFclDebugVars();
        clearIgcDebugVars();

        enabled = false;
    }
}
} // namespace NEO

namespace IGC {

// Stub versions - overridable in mocks
// IgcOclDeviceCtx stubs
IgcOclDeviceCtx<0>::~IgcOclDeviceCtx() {}

template <typename... ArgsT>
IgcOclDeviceCtx<0>::IgcOclDeviceCtx(ArgsT &&...args) {}

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

// Platform stubs
Platform<0>::~Platform() {}

template <typename... ArgsT>
Platform<0>::Platform(ArgsT &&...args) {}

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

#undef DEFINE_GET_SET

// GtSystemInfo stubs
GTSystemInfo<0>::~GTSystemInfo() {}

template <typename... ArgsT>
GTSystemInfo<0>::GTSystemInfo(ArgsT &&...args) {}

#define DEFINE_GET_SET(INTERFACE, VERSION, NAME, TYPE)                                      \
    TYPE CIF_GET_INTERFACE_CLASS(INTERFACE, VERSION)::Get##NAME() const { return (TYPE)0; } \
    void CIF_GET_INTERFACE_CLASS(INTERFACE, VERSION)::Set##NAME(TYPE v) {}

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

#undef DEFINE_GET_SET

// IgcFeaturesAndWorkarounds stubs
IgcFeaturesAndWorkarounds<0>::~IgcFeaturesAndWorkarounds() {}

template <typename... ArgsT>
IgcFeaturesAndWorkarounds<0>::IgcFeaturesAndWorkarounds(ArgsT &&...args) {}

template <>
IgcFeaturesAndWorkarounds<0>::IgcFeaturesAndWorkarounds() {}

#define DEFINE_GET_SET(INTERFACE, VERSION, NAME, TYPE)                                      \
    TYPE CIF_GET_INTERFACE_CLASS(INTERFACE, VERSION)::Get##NAME() const { return (TYPE)0; } \
    void CIF_GET_INTERFACE_CLASS(INTERFACE, VERSION)::Set##NAME(TYPE v) {}

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

// IgcOclTranslationCtx
IgcOclTranslationCtx<0>::~IgcOclTranslationCtx() {}

template <typename... ArgsT>
IgcOclTranslationCtx<0>::IgcOclTranslationCtx(ArgsT &&...args) {}

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

// OclTranslationOutput
OclTranslationOutput<0>::~OclTranslationOutput() {}

template <typename... ArgsT>
OclTranslationOutput<0>::OclTranslationOutput(ArgsT &&...args) {}

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

// FclOclTranslationCtx
FclOclTranslationCtx<0>::~FclOclTranslationCtx() {}

template <typename... ArgsT>
FclOclTranslationCtx<0>::FclOclTranslationCtx(ArgsT &&...args) {}

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

// MockFclOclDeviceCtx
FclOclDeviceCtx<0>::~FclOclDeviceCtx() {}

template <typename... ArgsT>
FclOclDeviceCtx<0>::FclOclDeviceCtx(ArgsT &&...args) {}

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

} // namespace IGC

#include "cif/macros/disable.h"

namespace NEO {

template <typename StrT>
std::unique_ptr<unsigned char[]> loadBinaryFile(StrT &&fileName, size_t &fileSize) {

    std::unique_ptr<char[]> data = loadDataFromFile(fileName.c_str(), fileSize);
    return std::unique_ptr<unsigned char[]>(reinterpret_cast<unsigned char *>(data.release()));
};

template <typename StrT>
std::unique_ptr<unsigned char[]> loadVirtualBinaryFile(StrT &&fileName, size_t &fileSize) {
    std::filesystem::path filePath = std::forward<StrT>(fileName);
    std::string fileNameWithExtension = filePath.filename().string();
    if (!virtualFileExists(fileNameWithExtension)) {
        return loadBinaryFile(fileName, fileSize);
    }

    std::unique_ptr<char[]> charData = loadDataFromVirtualFile(fileNameWithExtension.c_str(), fileSize);

    std::unique_ptr<unsigned char[]> ucharData(new unsigned char[fileSize]);
    std::memcpy(ucharData.get(), charData.get(), fileSize);

    return ucharData;
};

void translate(bool usingIgc, CIF::Builtins::BufferSimple *src, CIF::Builtins::BufferSimple *options,
               CIF::Builtins::BufferSimple *internalOptions, MockOclTranslationOutput *out) {
    MockCompilerDebugVars &debugVars = (usingIgc) ? *NEO::igcDebugVars : *fclDebugVars;
    if (debugVars.receivedInput != nullptr) {
        if (src != nullptr) {
            debugVars.receivedInput->assign(src->GetMemory<char>(),
                                            src->GetMemory<char>() + src->GetSizeRaw());
        }
    }

    if (debugVars.receivedInternalOptionsOutput != nullptr) {
        if (internalOptions != nullptr) {
            debugVars.receivedInternalOptionsOutput->assign(internalOptions->GetMemory<char>(),
                                                            internalOptions->GetMemory<char>() + internalOptions->GetSizeRaw());
        }
    }

    if (debugVars.forceSuccessWithEmptyOutput) {
        if (out) {
            out->setOutput(nullptr, 0);
        }
        return;
    }

    if ((debugVars.forceBuildFailure == false) &&
        (out && src && src->GetMemoryRaw() && src->GetSizeRaw())) {

        if (debugVars.internalOptionsExpected) {
            if (internalOptions->GetSizeRaw() < 1 || internalOptions->GetMemoryRaw() == nullptr) { // NOLINT(clang-analyzer-core.CallAndMessage)
                if (out) {
                    out->setError();
                }
            }
        }

        std::string inputFile{}, debugFile{};
        std::string opts(options->GetMemory<char>(), options->GetMemory<char>() + options->GetSize<char>());
        if (false == debugVars.fileName.empty()) {
            auto fileBaseName = debugVars.fileName;
            auto pos = debugVars.fileName.rfind(".");
            auto extension = debugVars.fileName.substr(pos, debugVars.fileName.length());
            if (false == debugVars.fileNameSuffix.empty()) {
                pos = debugVars.fileName.rfind(debugVars.fileNameSuffix);
            }
            fileBaseName = fileBaseName.substr(0, pos);

            if (debugVars.appendOptionsToFileName && false == opts.empty()) {
                // handle special option "-create-library" - just erase it
                auto optPos = opts.find(CompilerOptions::createLibrary.data(), 0);
                if (optPos != std::string::npos) {
                    opts.erase(optPos, CompilerOptions::createLibrary.length());
                }
                std::replace(opts.begin(), opts.end(), ' ', '_');
            }

            inputFile.append(fileBaseName);
            debugFile.append(fileBaseName);

            if (debugVars.appendOptionsToFileName && false == opts.empty()) {
                auto optString = opts + "_";
                inputFile.append(optString);
                debugFile.append(optString);
            }
            if (false == debugVars.fileNameSuffix.empty()) {
                inputFile.append(debugVars.fileNameSuffix);
                debugFile.append(debugVars.fileNameSuffix);
            }
            inputFile.append(extension);
            debugFile.append(".dbg");
        }
        if ((debugVars.binaryToReturn != nullptr) || (debugVars.binaryToReturnSize != 0)) {
            out->setOutput(debugVars.binaryToReturn, debugVars.binaryToReturnSize);
        } else {
            size_t fileSize = 0;
            auto fileData = loadVirtualBinaryFile(inputFile, fileSize);

            out->setOutput(fileData.get(), fileSize);
            if (fileSize == 0) {
                out->setError("error: Mock compiler could not find cached input file: " + inputFile);
            }
        }

        if (debugVars.debugDataToReturn != nullptr) {
            out->setDebugData(debugVars.debugDataToReturn, debugVars.debugDataToReturnSize);
        } else {
            size_t fileSize = 0;
            auto fileData = loadVirtualBinaryFile(debugFile, fileSize);
            out->setDebugData(fileData.get(), fileSize);
        }
    } else {
        out->setError();
    }
}

MockIgcOclDeviceCtx::MockIgcOclDeviceCtx() {
    platform = new MockCIFPlatform;
    gtSystemInfo = new MockGTSystemInfo;
    igcFtrWa = new MockIgcFeaturesAndWorkarounds;
}

MockIgcOclDeviceCtx::~MockIgcOclDeviceCtx() {
    if (platform != nullptr) {
        platform->Release();
    }
    if (gtSystemInfo != nullptr) {
        gtSystemInfo->Release();
    }
    if (igcFtrWa != nullptr) {
        igcFtrWa->Release();
    }
}

CIF::ICIF *MockIgcOclDeviceCtx::Create(CIF::InterfaceId_t intId, CIF::Version_t version) {
    return new MockIgcOclDeviceCtx;
}

IGC::IgcOclTranslationCtxBase *MockIgcOclDeviceCtx::CreateTranslationCtxImpl(CIF::Version_t ver,
                                                                             IGC::CodeType::CodeType_t inType,
                                                                             IGC::CodeType::CodeType_t outType) {
    if (igcDebugVars->shouldFailCreationOfTranslationContext) {
        return nullptr;
    }

    requestedTranslationCtxs.emplace_back(inType, outType);
    return new MockIgcOclTranslationCtx;
}

bool MockIgcOclDeviceCtx::GetSystemRoutine(IGC::SystemRoutineType::SystemRoutineType_t typeOfSystemRoutine,
                                           bool bindless,
                                           CIF::Builtins::BufferSimple *outSystemRoutineBuffer,
                                           CIF::Builtins::BufferSimple *stateSaveAreaHeaderInit) {
    MockCompilerDebugVars &debugVars = *NEO::igcDebugVars;
    debugVars.typeOfSystemRoutine = typeOfSystemRoutine;
    debugVars.receivedSipAddressingType = bindless ? MockCompilerDebugVars::SipAddressingType::bindless : MockCompilerDebugVars::SipAddressingType::bindful;

    const char mockData1[64] = {'C', 'T', 'N', 'I'};
    const SIP::StateSaveAreaHeaderV3 mockSipStateSaveAreaHeaderV3 = {
        .versionHeader{
            .magic = "tssarea",
            .reserved1 = 0,
            .version = {3, 0, 0},
            .size = static_cast<uint8_t>(sizeof(SIP::StateSaveArea)),
            .reserved2 = {0, 0, 0}},
        .regHeader{}};

    if (debugVars.forceBuildFailure || typeOfSystemRoutine == IGC::SystemRoutineType::undefined) {
        return false;
    }

    if (debugVars.binaryToReturnSize > 0 && debugVars.binaryToReturn != nullptr) {
        outSystemRoutineBuffer->PushBackRawBytes(debugVars.binaryToReturn, debugVars.binaryToReturnSize);
    } else {
        outSystemRoutineBuffer->PushBackRawBytes(mockData1, 64);
    }

    if (debugVars.stateSaveAreaHeaderToReturnSize > 0 && debugVars.stateSaveAreaHeaderToReturn != nullptr) {
        stateSaveAreaHeaderInit->PushBackRawBytes(debugVars.stateSaveAreaHeaderToReturn, debugVars.stateSaveAreaHeaderToReturnSize);
    } else {
        stateSaveAreaHeaderInit->PushBackRawBytes(&mockSipStateSaveAreaHeaderV3, sizeof(mockSipStateSaveAreaHeaderV3));
    }
    return true;
}

const char *MockIgcOclDeviceCtx::GetIGCRevision() {
    return "mockigcrevision";
}

MockIgcOclTranslationCtx::MockIgcOclTranslationCtx() = default;
MockIgcOclTranslationCtx::~MockIgcOclTranslationCtx() = default;

IGC::OclTranslationOutputBase *MockIgcOclTranslationCtx::TranslateImpl(
    CIF::Version_t outVersion,
    CIF::Builtins::BufferSimple *src,
    CIF::Builtins::BufferSimple *options,
    CIF::Builtins::BufferSimple *internalOptions,
    CIF::Builtins::BufferSimple *tracingOptions,
    uint32_t tracingOptionsCount) {
    if (igcDebugVars->shouldReturnInvalidTranslationOutput) {
        return nullptr;
    }

    auto out = new MockOclTranslationOutput();
    translate(true, src, options, internalOptions, out);
    return out;
}

IGC::OclTranslationOutputBase *MockIgcOclTranslationCtx::TranslateImpl(
    CIF::Version_t outVersion,
    CIF::Builtins::BufferSimple *src,
    CIF::Builtins::BufferSimple *options,
    CIF::Builtins::BufferSimple *internalOptions,
    CIF::Builtins::BufferSimple *tracingOptions,
    uint32_t tracingOptionsCount,
    void *gtpinInput) {
    if (igcDebugVars->shouldReturnInvalidTranslationOutput) {
        return nullptr;
    }

    auto out = new MockOclTranslationOutput();
    translate(true, src, options, internalOptions, out);
    return out;
}

bool MockIgcOclTranslationCtx::GetSpecConstantsInfoImpl(
    CIF::Builtins::BufferSimple *src,
    CIF::Builtins::BufferSimple *outSpecConstantsIds,
    CIF::Builtins::BufferSimple *outSpecConstantsSizes) {
    return true;
}

IGC::OclTranslationOutputBase *MockIgcOclTranslationCtx::TranslateImpl(
    CIF::Version_t outVersion,
    CIF::Builtins::BufferSimple *src,
    CIF::Builtins::BufferSimple *specConstantsIds,
    CIF::Builtins::BufferSimple *specConstantsValues,
    CIF::Builtins::BufferSimple *options,
    CIF::Builtins::BufferSimple *internalOptions,
    CIF::Builtins::BufferSimple *tracingOptions,
    uint32_t tracingOptionsCount,
    void *gtPinInput) {
    if (igcDebugVars->shouldReturnInvalidTranslationOutput) {
        return nullptr;
    }

    auto out = new MockOclTranslationOutput();
    translate(true, src, options, internalOptions, out);
    return out;
}

MockOclTranslationOutput::MockOclTranslationOutput() {
    this->log = new MockCIFBuffer();
    this->output = new MockCIFBuffer();
    this->debugData = new MockCIFBuffer();
}

MockOclTranslationOutput::~MockOclTranslationOutput() {
    if (this->log != nullptr) {
        this->log->Release();
    }

    if (this->output != nullptr) {
        this->output->Release();
    }

    if (this->debugData != nullptr) {
        this->debugData->Release();
    }
}

void MockOclTranslationOutput::setError(const std::string &message) {
    failed = true;
    this->log->SetUnderlyingStorage(message.c_str(), message.size());
}

void MockOclTranslationOutput::setOutput(const void *data, size_t dataLen) {
    this->output->SetUnderlyingStorage(data, dataLen);
}

void MockOclTranslationOutput::setDebugData(const void *data, size_t dataLen) {
    this->debugData->SetUnderlyingStorage(data, dataLen);
}

CIF::Builtins::BufferBase *MockOclTranslationOutput::GetBuildLogImpl(CIF::Version_t bufferVersion) {
    return log;
}

CIF::Builtins::BufferBase *MockOclTranslationOutput::GetOutputImpl(CIF::Version_t bufferVersion) {
    return output;
}
CIF::Builtins::BufferBase *MockOclTranslationOutput::GetDebugDataImpl(CIF::Version_t bufferVersion) {
    return debugData;
}

MockFclOclTranslationCtx::MockFclOclTranslationCtx() {
}

MockFclOclTranslationCtx::~MockFclOclTranslationCtx() {
}

IGC::OclTranslationOutputBase *MockFclOclTranslationCtx::TranslateImpl(
    CIF::Version_t outVersion,
    CIF::Builtins::BufferSimple *src,
    CIF::Builtins::BufferSimple *options,
    CIF::Builtins::BufferSimple *internalOptions,
    CIF::Builtins::BufferSimple *tracingOptions,
    uint32_t tracingOptionsCount) {
    if (fclDebugVars->shouldReturnInvalidTranslationOutput) {
        return nullptr;
    }

    auto out = new MockOclTranslationOutput();
    translate(false, src, options, internalOptions, out);
    return out;
}

MockFclOclDeviceCtx::MockFclOclDeviceCtx() {
    platform = new MockCIFPlatform;
}

MockFclOclDeviceCtx::~MockFclOclDeviceCtx() {
    if (nullptr != platform) {
        platform->Release();
    }
}

CIF::ICIF *MockFclOclDeviceCtx::Create(CIF::InterfaceId_t intId, CIF::Version_t version) {
    return new MockFclOclDeviceCtx;
}

IGC::FclOclTranslationCtxBase *MockFclOclDeviceCtx::CreateTranslationCtxImpl(CIF::Version_t ver,
                                                                             IGC::CodeType::CodeType_t inType,
                                                                             IGC::CodeType::CodeType_t outType) {
    if (fclDebugVars->shouldFailCreationOfTranslationContext) {
        return nullptr;
    }

    requestedTranslationCtxs.emplace_back(inType, outType);
    return new MockFclOclTranslationCtx;
}

IGC::FclOclTranslationCtxBase *MockFclOclDeviceCtx::CreateTranslationCtxImpl(CIF::Version_t ver,
                                                                             IGC::CodeType::CodeType_t inType,
                                                                             IGC::CodeType::CodeType_t outType,
                                                                             CIF::Builtins::BufferSimple *err) {
    if (!fclDebugVars->translationContextCreationError.empty() && err) {
        err->PushBackRawBytes(fclDebugVars->translationContextCreationError.c_str(), fclDebugVars->translationContextCreationError.size());
    }

    if (fclDebugVars->shouldFailCreationOfTranslationContext) {
        return nullptr;
    }

    requestedTranslationCtxs.emplace_back(inType, outType);
    return new MockFclOclTranslationCtx;
}

std::vector<char> MockCompilerInterface::getDummyGenBinary() {
    return MockSipKernel::getDummyGenBinary();
}
void MockCompilerInterface::releaseDummyGenBinary() {
}

} // namespace NEO