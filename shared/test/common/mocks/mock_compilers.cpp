/*
 * Copyright (C) 2018-2026 Intel Corporation
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

#include "StateSaveAreaHeaderWrapper.h"
#include "ocl_igc_interface/fcl_ocl_device_ctx.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"

#include <filesystem>
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

// Platform stubs
Platform<0>::~Platform() {}

template <typename... ArgsT>
Platform<0>::Platform(ArgsT &&...args) {}

// GtSystemInfo stubs
GTSystemInfo<0>::~GTSystemInfo() {}

template <typename... ArgsT>
GTSystemInfo<0>::GTSystemInfo(ArgsT &&...args) {}

// IgcFeaturesAndWorkarounds stubs
IgcFeaturesAndWorkarounds<0>::~IgcFeaturesAndWorkarounds() {}

template <typename... ArgsT>
IgcFeaturesAndWorkarounds<0>::IgcFeaturesAndWorkarounds(ArgsT &&...args) {}

template <>
IgcFeaturesAndWorkarounds<0>::IgcFeaturesAndWorkarounds() {}

// IgcOclTranslationCtx
IgcOclTranslationCtx<0>::~IgcOclTranslationCtx() {}

template <typename... ArgsT>
IgcOclTranslationCtx<0>::IgcOclTranslationCtx(ArgsT &&...args) {}

// OclTranslationOutput
OclTranslationOutput<0>::~OclTranslationOutput() {}

template <typename... ArgsT>
OclTranslationOutput<0>::OclTranslationOutput(ArgsT &&...args) {}

// FclOclTranslationCtx
FclOclTranslationCtx<0>::~FclOclTranslationCtx() {}

template <typename... ArgsT>
FclOclTranslationCtx<0>::FclOclTranslationCtx(ArgsT &&...args) {}

// MockFclOclDeviceCtx
FclOclDeviceCtx<0>::~FclOclDeviceCtx() {}

template <typename... ArgsT>
FclOclDeviceCtx<0>::FclOclDeviceCtx(ArgsT &&...args) {}

IgcOptionsAndCapabilities<0>::~IgcOptionsAndCapabilities() {}

template <typename... ArgsT>
IgcOptionsAndCapabilities<0>::IgcOptionsAndCapabilities(ArgsT &&...args) {}

} // namespace IGC

namespace NEO {

void translate(bool usingIgc, CIF::Builtins::BufferSimple *src, CIF::Builtins::BufferSimple *options,
               CIF::Builtins::BufferSimple *internalOptions, MockOclTranslationOutput *out,
               IGC::CodeType::CodeType_t outType = IGC::CodeType::undefined) {
    MockCompilerDebugVars &debugVars = (usingIgc) ? *NEO::igcDebugVars : *fclDebugVars;

    // Only the first call's input is captured - a build may invoke translate() more than
    // once, and callers generally want the earliest one, not whichever ran last.
    if (debugVars.receivedInput != nullptr && debugVars.receivedInput->empty()) {
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

    bool forceFailure = debugVars.forceBuildFailure;
    if (forceFailure && debugVars.forceBuildFailureBackendOnly && (outType != IGC::CodeType::oclGenBin)) {
        forceFailure = false;
    }

    if (debugVars.forceSuccessWithEmptyOutput) {
        if (out) {
            out->setOutput(nullptr, 0);
        }
    } else if ((forceFailure == false) &&
               (out && src && src->GetMemoryRaw() && src->GetSizeRaw())) {

        if (debugVars.internalOptionsExpected) {
            if (internalOptions->GetSizeRaw() < 1 || internalOptions->GetMemoryRaw() == nullptr) { // NOLINT(clang-analyzer-core.CallAndMessage)
                if (out) {
                    out->setError();
                }
            }
        }

        out->setOutput(debugVars.binaryToReturn, debugVars.binaryToReturnSize);
        if (debugVars.binaryToReturn == nullptr && debugVars.binaryToReturnSize == 0) {
            out->setError("error: Mock compiler has no binaryToReturn configured");
        }

        if (debugVars.debugDataToReturn != nullptr || debugVars.debugDataToReturnSize != 0) {
            out->setDebugData(debugVars.debugDataToReturn, debugVars.debugDataToReturnSize);
        }
    } else {
        out->setError();
    }

    if (out != nullptr && !debugVars.buildLogToReturn.empty()) {
        out->setBuildLog(debugVars.buildLogToReturn);
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
    auto ctx = new MockIgcOclTranslationCtx;
    ctx->createdOutType = outType;
    return ctx;
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

void MockIgcOclDeviceCtx::GetIGCRegKeys(CIF::Builtins::BufferSimple *outIgcRegKeysBuffer) {
    if (outIgcRegKeysBuffer != nullptr && !igcRegKeysToReturn.empty()) {
        outIgcRegKeysBuffer->PushBackRawBytes(igcRegKeysToReturn.data(), igcRegKeysToReturn.size());
    }
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
    translate(true, src, options, internalOptions, out, createdOutType);
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
    translate(true, src, options, internalOptions, out, createdOutType);
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
    translate(true, src, options, internalOptions, out, createdOutType);
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

void MockOclTranslationOutput::setBuildLog(const std::string &message) {
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
