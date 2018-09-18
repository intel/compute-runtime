/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_compilers.h"

#include "runtime/helpers/file_io.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/os_inc_base.h"
#include "unit_tests/helpers/test_files.h"
#include "unit_tests/mocks/mock_sip.h"

#include "ocl_igc_interface/igc_ocl_device_ctx.h"
#include "ocl_igc_interface/fcl_ocl_device_ctx.h"

#include <fstream>
#include <map>

#include "cif/macros/enable.h"
namespace OCLRT {

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
        this->oldIgcDllName = Os::igcDllName;
        Os::frontEndDllName = "";
        Os::igcDllName = "";
        MockCIFMain::setGlobalCreatorFunc<OCLRT::MockIgcOclDeviceCtx>(OCLRT::MockIgcOclDeviceCtx::Create);
        MockCIFMain::setGlobalCreatorFunc<OCLRT::MockFclOclDeviceCtx>(OCLRT::MockFclOclDeviceCtx::Create);
        if (fclDebugVars == nullptr) {
            fclDebugVars.reset(new MockCompilerDebugVars);
        }
        if (fclDebugVars == nullptr) {
            igcDebugVars.reset(new MockCompilerDebugVars);
        }

        enabled = true;
    }
}

void MockCompilerEnableGuard::Disable() {
    if (enabled) {
        Os::frontEndDllName = this->oldFclDllName;
        Os::igcDllName = this->oldIgcDllName;

        MockCIFMain::removeGlobalCreatorFunc<OCLRT::MockIgcOclDeviceCtx>();
        MockCIFMain::removeGlobalCreatorFunc<OCLRT::MockFclOclDeviceCtx>();

        clearFclDebugVars();
        clearIgcDebugVars();

        enabled = false;
    }
}
} // namespace OCLRT

namespace IGC {

// Stub versions - overridable in mocks
// IgcOclDeviceCtx stubs
IgcOclDeviceCtx<0>::~IgcOclDeviceCtx() {}

template <typename... ArgsT>
IgcOclDeviceCtx<0>::IgcOclDeviceCtx(ArgsT &&... args) {}

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

// Platform stubs
Platform<0>::~Platform() {}

template <typename... ArgsT>
Platform<0>::Platform(ArgsT &&... args) {}

#define DEFINE_GET_SET_PREFIX(INTERFACE, VERSION, NAME, TYPE, PREFIX)                       \
    TYPE CIF_GET_INTERFACE_CLASS(INTERFACE, VERSION)::Get##NAME() const { return (TYPE)0; } \
    void CIF_GET_INTERFACE_CLASS(INTERFACE, VERSION)::Set##NAME(TYPE v) {}

DEFINE_GET_SET_PREFIX(Platform, 1, ProductFamily, TypeErasedEnum, e);
DEFINE_GET_SET_PREFIX(Platform, 1, PCHProductFamily, TypeErasedEnum, e);
DEFINE_GET_SET_PREFIX(Platform, 1, DisplayCoreFamily, TypeErasedEnum, e);
DEFINE_GET_SET_PREFIX(Platform, 1, RenderCoreFamily, TypeErasedEnum, e);
DEFINE_GET_SET_PREFIX(Platform, 1, PlatformType, TypeErasedEnum, e);
DEFINE_GET_SET_PREFIX(Platform, 1, DeviceID, unsigned short, us);
DEFINE_GET_SET_PREFIX(Platform, 1, RevId, unsigned short, us);
DEFINE_GET_SET_PREFIX(Platform, 1, DeviceID_PCH, unsigned short, us);
DEFINE_GET_SET_PREFIX(Platform, 1, RevId_PCH, unsigned short, us);
DEFINE_GET_SET_PREFIX(Platform, 1, GTType, TypeErasedEnum, e);

#undef DEFINE_GET_SET_PREFIX

// GtSystemInfo stubs
GTSystemInfo<0>::~GTSystemInfo() {}

template <typename... ArgsT>
GTSystemInfo<0>::GTSystemInfo(ArgsT &&... args) {}

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

#undef DEFINE_GET_SET

// IgcFeaturesAndWorkarounds stubs
IgcFeaturesAndWorkarounds<0>::~IgcFeaturesAndWorkarounds() {}

template <typename... ArgsT>
IgcFeaturesAndWorkarounds<0>::IgcFeaturesAndWorkarounds(ArgsT &&... args) {}

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

#undef DEFINE_GET_SET

// IgcOclTranslationCtx
IgcOclTranslationCtx<0>::~IgcOclTranslationCtx() {}

template <typename... ArgsT>
IgcOclTranslationCtx<0>::IgcOclTranslationCtx(ArgsT &&... args) {}

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

// OclTranslationOutput
OclTranslationOutput<0>::~OclTranslationOutput() {}

template <typename... ArgsT>
OclTranslationOutput<0>::OclTranslationOutput(ArgsT &&... args) {}

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
FclOclTranslationCtx<0>::FclOclTranslationCtx(ArgsT &&... args) {}

IGC::OclTranslationOutputBase *CIF_GET_INTERFACE_CLASS(FclOclTranslationCtx, 1)::TranslateImpl(
    CIF::Version_t outVersion,
    CIF::Builtins::BufferSimple *src,
    CIF::Builtins::BufferSimple *options,
    CIF::Builtins::BufferSimple *internalOptions,
    CIF::Builtins::BufferSimple *tracingOptions,
    uint32_t tracingOptionsCount) {
    return nullptr;
}

// MockFclOclDeviceCtx
FclOclDeviceCtx<0>::~FclOclDeviceCtx() {}

template <typename... ArgsT>
FclOclDeviceCtx<0>::FclOclDeviceCtx(ArgsT &&... args) {}

void CIF_GET_INTERFACE_CLASS(FclOclDeviceCtx, 1)::SetOclApiVersion(uint32_t version) {}

IGC::FclOclTranslationCtxBase *CIF_GET_INTERFACE_CLASS(FclOclDeviceCtx, 1)::CreateTranslationCtxImpl(CIF::Version_t ver,
                                                                                                     IGC::CodeType::CodeType_t inType,
                                                                                                     IGC::CodeType::CodeType_t outType) {
    return nullptr;
}

CodeType::CodeType_t CIF_GET_INTERFACE_CLASS(FclOclDeviceCtx, 2)::GetPreferredIntermediateRepresentation() {
    return CodeType::spirV;
}

} // namespace IGC

#include "cif/macros/disable.h"

namespace OCLRT {

template <typename StrT>
std::unique_ptr<unsigned char[]> loadBinaryFile(StrT &&fileName, size_t &fileSize) {
    std::ifstream f{fileName, std::ios::binary | std::ios::in | std::ios::ate};
    auto end = f.tellg();
    f.seekg(0, std::ios::beg);
    auto beg = f.tellg();
    auto s = static_cast<size_t>(end - beg);
    if (s == 0) {
        fileSize = 0;
        return nullptr;
    }
    std::unique_ptr<unsigned char[]> data{new unsigned char[s]};
    f.read(reinterpret_cast<char *>(data.get()), s);
    fileSize = s;
    return data;
};

void translate(bool usingIgc, CIF::Builtins::BufferSimple *src, CIF::Builtins::BufferSimple *options,
               CIF::Builtins::BufferSimple *internalOptions, MockOclTranslationOutput *out) {
    MockCompilerDebugVars &debugVars = (usingIgc) ? *OCLRT::igcDebugVars : *fclDebugVars;
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

    if ((debugVars.forceBuildFailure == false) &&
        (out && src && src->GetMemoryRaw() && src->GetSizeRaw())) {

        if (debugVars.internalOptionsExpected) {
            if (internalOptions->GetSizeRaw() < 1 || internalOptions->GetMemoryRaw() == nullptr) {
                if (out) {
                    out->setError();
                }
            }
        }

        std::string inputFile = "";
        inputFile.append(debugVars.fileName);

        std::string debugFile;
        auto pos = inputFile.rfind(".");
        debugFile = inputFile.substr(0, pos);
        debugFile.append(".dbg");

        if (debugVars.appendOptionsToFileName &&
            options->GetSizeRaw()) {
            std::string opts(options->GetMemory<char>(), options->GetMemory<char>() + options->GetSize<char>());
            // handle special option "-create-library" - just erase it
            size_t pos = opts.find("-create-library", 0);
            if (pos != std::string::npos) {
                opts.erase(pos, 15); // length of "-create-library" is 15 chars
            }
            std::replace(opts.begin(), opts.end(), ' ', '_');
            inputFile.append(opts);

            if (debugVars.debugDataToReturn == nullptr) {
                debugFile.append(opts);
            }
        }

        if ((debugVars.binaryToReturn != nullptr) || (debugVars.binaryToReturnSize != 0)) {
            out->setOutput(debugVars.binaryToReturn, debugVars.binaryToReturnSize);
        } else {
            size_t fileSize = 0;
            auto fileData = loadBinaryFile(inputFile, fileSize);

            out->setOutput(fileData.get(), fileSize);
            if (fileSize == 0) {
                out->setError("error: Mock compiler could not find cached input file: " + inputFile);
            }
        }

        if (debugVars.debugDataToReturn != nullptr) {
            out->setDebugData(debugVars.debugDataToReturn, debugVars.debugDataToReturnSize);
        } else {
            size_t fileSize = 0;
            auto fileData = loadBinaryFile(debugFile, fileSize);
            out->setDebugData(fileData.get(), fileSize);
        }
    } else {
        out->setError();
    }
}

MockIgcOclDeviceCtx::MockIgcOclDeviceCtx() {
    platform = new MockPlatform;
    gtSystemInfo = new MockGTSystemInfo;
    igcFeWa = new MockIgcFeaturesAndWorkarounds;
}

MockIgcOclDeviceCtx::~MockIgcOclDeviceCtx() {
    if (platform != nullptr) {
        platform->Release();
    }
    if (gtSystemInfo != nullptr) {
        gtSystemInfo->Release();
    }
    if (igcFeWa != nullptr) {
        igcFeWa->Release();
    }
}

CIF::ICIF *MockIgcOclDeviceCtx::Create(CIF::InterfaceId_t intId, CIF::Version_t version) {
    return new MockIgcOclDeviceCtx;
}

IGC::IgcOclTranslationCtxBase *MockIgcOclDeviceCtx::CreateTranslationCtxImpl(CIF::Version_t ver,
                                                                             IGC::CodeType::CodeType_t inType,
                                                                             IGC::CodeType::CodeType_t outType) {
    requestedTranslationCtxs.emplace_back(inType, outType);
    return new MockIgcOclTranslationCtx;
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
    auto out = new MockOclTranslationOutput();
    translate(false, src, options, internalOptions, out);
    return out;
}

MockFclOclDeviceCtx::MockFclOclDeviceCtx() = default;
MockFclOclDeviceCtx::~MockFclOclDeviceCtx() = default;

CIF::ICIF *MockFclOclDeviceCtx::Create(CIF::InterfaceId_t intId, CIF::Version_t version) {
    return new MockFclOclDeviceCtx;
}

IGC::FclOclTranslationCtxBase *MockFclOclDeviceCtx::CreateTranslationCtxImpl(CIF::Version_t ver,
                                                                             IGC::CodeType::CodeType_t inType,
                                                                             IGC::CodeType::CodeType_t outType) {
    return new MockFclOclTranslationCtx;
}

std::vector<char> MockCompilerInterface::getDummyGenBinary() {
    return MockSipKernel::getDummyGenBinary();
}
} // namespace OCLRT
