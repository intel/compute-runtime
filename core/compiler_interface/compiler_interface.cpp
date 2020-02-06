/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/compiler_interface/compiler_interface.h"

#include "core/compiler_interface/compiler_cache.h"
#include "core/compiler_interface/compiler_interface.inl"
#include "core/debug_settings/debug_settings_manager.h"
#include "core/device/device.h"
#include "core/helpers/hw_info.h"
#include "runtime/os_interface/os_inc_base.h"

#include "cif/common/cif_main.h"
#include "cif/helpers/error.h"
#include "cif/import/library_api.h"
#include "ocl_igc_interface/code_type.h"
#include "ocl_igc_interface/fcl_ocl_device_ctx.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"
#include "ocl_igc_interface/platform_helper.h"

#include <fstream>

namespace NEO {
SpinLock CompilerInterface::spinlock;

enum CachingMode {
    None,
    Direct,
    PreProcess
};

CompilerInterface::CompilerInterface()
    : cache() {
}
CompilerInterface::~CompilerInterface() = default;

TranslationOutput::ErrorCode CompilerInterface::build(
    const NEO::Device &device,
    const TranslationInput &input,
    TranslationOutput &output) {
    if (false == isCompilerAvailable(input.srcType, input.outType)) {
        return TranslationOutput::ErrorCode::CompilerNotAvailable;
    }

    IGC::CodeType::CodeType_t srcCodeType = input.srcType;
    IGC::CodeType::CodeType_t intermediateCodeType = IGC::CodeType::undefined;

    if (input.preferredIntermediateType != IGC::CodeType::undefined) {
        intermediateCodeType = input.preferredIntermediateType;
    }

    CachingMode cachingMode = None;

    if (input.allowCaching) {
        if ((srcCodeType == IGC::CodeType::oclC) && (std::strstr(input.src.begin(), "#include") == nullptr)) {
            cachingMode = CachingMode::Direct;
        } else {
            cachingMode = CachingMode::PreProcess;
        }
    }

    std::string kernelFileHash;
    if (cachingMode == CachingMode::Direct) {
        kernelFileHash = CompilerCache::getCachedFileName(device.getHardwareInfo(),
                                                          input.src,
                                                          input.apiOptions,
                                                          input.internalOptions);
        output.deviceBinary.mem = cache->loadCachedBinary(kernelFileHash, output.deviceBinary.size);
        if (output.deviceBinary.mem) {
            return TranslationOutput::ErrorCode::Success;
        }
    }

    auto inSrc = CIF::Builtins::CreateConstBuffer(igcMain.get(), input.src.begin(), input.src.size());
    auto fclOptions = CIF::Builtins::CreateConstBuffer(igcMain.get(), input.apiOptions.begin(), input.apiOptions.size());
    auto fclInternalOptions = CIF::Builtins::CreateConstBuffer(igcMain.get(), input.internalOptions.begin(), input.internalOptions.size());

    CIF::RAII::UPtr_t<CIF::Builtins::BufferSimple> intermediateRepresentation;

    if (srcCodeType == IGC::CodeType::oclC) {
        if (intermediateCodeType == IGC::CodeType::undefined) {
            intermediateCodeType = getPreferredIntermediateRepresentation(device);
        }

        auto fclTranslationCtx = createFclTranslationCtx(device, srcCodeType, intermediateCodeType);
        auto fclOutput = translate(fclTranslationCtx.get(), inSrc.get(),
                                   fclOptions.get(), fclInternalOptions.get());

        if (fclOutput == nullptr) {
            return TranslationOutput::ErrorCode::UnknownError;
        }

        TranslationOutput::makeCopy(output.frontendCompilerLog, fclOutput->GetBuildLog());

        if (fclOutput->Successful() == false) {
            return TranslationOutput::ErrorCode::BuildFailure;
        }

        output.intermediateCodeType = intermediateCodeType;
        TranslationOutput::makeCopy(output.intermediateRepresentation, fclOutput->GetOutput());

        fclOutput->GetOutput()->Retain(); // will be used as input to compiler
        intermediateRepresentation.reset(fclOutput->GetOutput());
    } else {
        inSrc->Retain(); // will be used as input to compiler directly
        intermediateRepresentation.reset(inSrc.get());
        intermediateCodeType = srcCodeType;
    }

    if (cachingMode == CachingMode::PreProcess) {
        kernelFileHash = CompilerCache::getCachedFileName(device.getHardwareInfo(), ArrayRef<const char>(intermediateRepresentation->GetMemory<char>(), intermediateRepresentation->GetSize<char>()),
                                                          input.apiOptions,
                                                          input.internalOptions);
        output.deviceBinary.mem = cache->loadCachedBinary(kernelFileHash, output.deviceBinary.size);
        if (output.deviceBinary.mem) {
            return TranslationOutput::ErrorCode::Success;
        }
    }

    auto igcTranslationCtx = createIgcTranslationCtx(device, intermediateCodeType, IGC::CodeType::oclGenBin);

    auto igcOutput = translate(igcTranslationCtx.get(), intermediateRepresentation.get(), input.specConstants.idsBuffer, input.specConstants.valuesBuffer,
                               fclOptions.get(), fclInternalOptions.get(), input.GTPinInput);

    if (igcOutput == nullptr) {
        return TranslationOutput::ErrorCode::UnknownError;
    }

    TranslationOutput::makeCopy(output.backendCompilerLog, igcOutput->GetBuildLog());

    if (igcOutput->Successful() == false) {
        return TranslationOutput::ErrorCode::BuildFailure;
    }

    if (input.allowCaching) {
        cache->cacheBinary(kernelFileHash, igcOutput->GetOutput()->GetMemory<char>(), static_cast<uint32_t>(igcOutput->GetOutput()->GetSize<char>()));
    }

    TranslationOutput::makeCopy(output.deviceBinary, igcOutput->GetOutput());
    TranslationOutput::makeCopy(output.debugData, igcOutput->GetDebugData());

    return TranslationOutput::ErrorCode::Success;
}

TranslationOutput::ErrorCode CompilerInterface::compile(
    const NEO::Device &device,
    const TranslationInput &input,
    TranslationOutput &output) {

    if ((IGC::CodeType::oclC != input.srcType) && (IGC::CodeType::elf != input.srcType)) {
        return TranslationOutput::ErrorCode::AlreadyCompiled;
    }

    if (false == isCompilerAvailable(input.srcType, input.outType)) {
        return TranslationOutput::ErrorCode::CompilerNotAvailable;
    }

    auto outType = input.outType;

    if (outType == IGC::CodeType::undefined) {
        outType = getPreferredIntermediateRepresentation(device);
    }

    auto fclSrc = CIF::Builtins::CreateConstBuffer(fclMain.get(), input.src.begin(), input.src.size());
    auto fclOptions = CIF::Builtins::CreateConstBuffer(fclMain.get(), input.apiOptions.begin(), input.apiOptions.size());
    auto fclInternalOptions = CIF::Builtins::CreateConstBuffer(fclMain.get(), input.internalOptions.begin(), input.internalOptions.size());

    auto fclTranslationCtx = createFclTranslationCtx(device, input.srcType, outType);

    auto fclOutput = translate(fclTranslationCtx.get(), fclSrc.get(),
                               fclOptions.get(), fclInternalOptions.get());

    if (fclOutput == nullptr) {
        return TranslationOutput::ErrorCode::UnknownError;
    }

    TranslationOutput::makeCopy(output.frontendCompilerLog, fclOutput->GetBuildLog());

    if (fclOutput->Successful() == false) {
        return TranslationOutput::ErrorCode::CompilationFailure;
    }

    output.intermediateCodeType = outType;
    TranslationOutput::makeCopy(output.intermediateRepresentation, fclOutput->GetOutput());

    return TranslationOutput::ErrorCode::Success;
}

TranslationOutput::ErrorCode CompilerInterface::link(
    const NEO::Device &device,
    const TranslationInput &input,
    TranslationOutput &output) {
    if (false == isCompilerAvailable(input.srcType, input.outType)) {
        return TranslationOutput::ErrorCode::CompilerNotAvailable;
    }

    auto inSrc = CIF::Builtins::CreateConstBuffer(igcMain.get(), input.src.begin(), input.src.size());
    auto igcOptions = CIF::Builtins::CreateConstBuffer(igcMain.get(), input.apiOptions.begin(), input.apiOptions.size());
    auto igcInternalOptions = CIF::Builtins::CreateConstBuffer(igcMain.get(), input.internalOptions.begin(), input.internalOptions.size());

    if (inSrc == nullptr) {
        return TranslationOutput::ErrorCode::UnknownError;
    }

    CIF::RAII::UPtr_t<IGC::OclTranslationOutputTagOCL> currOut;
    inSrc->Retain(); // shared with currSrc
    CIF::RAII::UPtr_t<CIF::Builtins::BufferSimple> currSrc(inSrc.get());
    IGC::CodeType::CodeType_t translationChain[] = {IGC::CodeType::elf, IGC::CodeType::llvmBc, IGC::CodeType::oclGenBin};
    constexpr size_t numTranslations = sizeof(translationChain) / sizeof(translationChain[0]);
    for (size_t ti = 1; ti < numTranslations; ti++) {
        IGC::CodeType::CodeType_t inType = translationChain[ti - 1];
        IGC::CodeType::CodeType_t outType = translationChain[ti];

        auto igcTranslationCtx = createIgcTranslationCtx(device, inType, outType);
        currOut = translate(igcTranslationCtx.get(), currSrc.get(),
                            igcOptions.get(), igcInternalOptions.get());

        if (currOut == nullptr) {
            return TranslationOutput::ErrorCode::UnknownError;
        }

        if (currOut->Successful() == false) {
            TranslationOutput::makeCopy(output.backendCompilerLog, currOut->GetBuildLog());
            return TranslationOutput::ErrorCode::LinkFailure;
        }

        currOut->GetOutput()->Retain(); // shared with currSrc
        currSrc.reset(currOut->GetOutput());
    }

    TranslationOutput::makeCopy(output.backendCompilerLog, currOut->GetBuildLog());
    TranslationOutput::makeCopy(output.deviceBinary, currOut->GetOutput());
    TranslationOutput::makeCopy(output.debugData, currOut->GetDebugData());

    return TranslationOutput::ErrorCode::Success;
}

TranslationOutput::ErrorCode CompilerInterface::getSpecConstantsInfo(const NEO::Device &device, ArrayRef<const char> srcSpirV, SpecConstantInfo &output) {
    if (false == isIgcAvailable()) {
        return TranslationOutput::ErrorCode::CompilerNotAvailable;
    }

    auto igcTranslationCtx = createIgcTranslationCtx(device, IGC::CodeType::spirV, IGC::CodeType::oclGenBin);

    auto inSrc = CIF::Builtins::CreateConstBuffer(igcMain.get(), srcSpirV.begin(), srcSpirV.size());
    output.idsBuffer = CIF::Builtins::CreateConstBuffer(igcMain.get(), nullptr, 0);
    output.sizesBuffer = CIF::Builtins::CreateConstBuffer(igcMain.get(), nullptr, 0);
    output.valuesBuffer = CIF::Builtins::CreateConstBuffer(igcMain.get(), nullptr, 0);

    auto retVal = getSpecConstantsInfoImpl(igcTranslationCtx.get(), inSrc.get(), output.idsBuffer.get(), output.sizesBuffer.get(), output.valuesBuffer.get());

    if (!retVal) {
        return TranslationOutput::ErrorCode::UnknownError;
    }

    return TranslationOutput::ErrorCode::Success;
}

TranslationOutput::ErrorCode CompilerInterface::createLibrary(
    NEO::Device &device,
    const TranslationInput &input,
    TranslationOutput &output) {
    if (false == isIgcAvailable()) {
        return TranslationOutput::ErrorCode::CompilerNotAvailable;
    }

    auto igcSrc = CIF::Builtins::CreateConstBuffer(igcMain.get(), input.src.begin(), input.src.size());
    auto igcOptions = CIF::Builtins::CreateConstBuffer(igcMain.get(), input.apiOptions.begin(), input.apiOptions.size());
    auto igcInternalOptions = CIF::Builtins::CreateConstBuffer(igcMain.get(), input.internalOptions.begin(), input.internalOptions.size());

    auto intermediateRepresentation = IGC::CodeType::llvmBc;
    auto igcTranslationCtx = createIgcTranslationCtx(device, IGC::CodeType::elf, intermediateRepresentation);

    auto igcOutput = translate(igcTranslationCtx.get(), igcSrc.get(),
                               igcOptions.get(), igcInternalOptions.get());

    if (igcOutput == nullptr) {
        return TranslationOutput::ErrorCode::UnknownError;
    }

    TranslationOutput::makeCopy(output.backendCompilerLog, igcOutput->GetBuildLog());

    if (igcOutput->Successful() == false) {
        return TranslationOutput::ErrorCode::LinkFailure;
    }

    output.intermediateCodeType = intermediateRepresentation;
    TranslationOutput::makeCopy(output.intermediateRepresentation, igcOutput->GetOutput());

    return TranslationOutput::ErrorCode::Success;
}

TranslationOutput::ErrorCode CompilerInterface::getSipKernelBinary(NEO::Device &device, SipKernelType type, std::vector<char> &retBinary) {
    if (false == isIgcAvailable()) {
        return TranslationOutput::ErrorCode::CompilerNotAvailable;
    }

    const char *sipSrc = getSipLlSrc(device);
    std::string sipInternalOptions = getSipKernelCompilerInternalOptions(type);

    auto igcSrc = CIF::Builtins::CreateConstBuffer(igcMain.get(), sipSrc, strlen(sipSrc) + 1);
    auto igcOptions = CIF::Builtins::CreateConstBuffer(igcMain.get(), nullptr, 0);
    auto igcInternalOptions = CIF::Builtins::CreateConstBuffer(igcMain.get(), sipInternalOptions.c_str(), sipInternalOptions.size() + 1);

    auto igcTranslationCtx = createIgcTranslationCtx(device, IGC::CodeType::llvmLl, IGC::CodeType::oclGenBin);

    auto igcOutput = translate(igcTranslationCtx.get(), igcSrc.get(),
                               igcOptions.get(), igcInternalOptions.get());

    if ((igcOutput == nullptr) || (igcOutput->Successful() == false)) {
        return TranslationOutput::ErrorCode::UnknownError;
    }

    retBinary.assign(igcOutput->GetOutput()->GetMemory<char>(), igcOutput->GetOutput()->GetMemory<char>() + igcOutput->GetOutput()->GetSizeRaw());
    return TranslationOutput::ErrorCode::Success;
}

bool CompilerInterface::loadFcl() {
    return NEO::loadCompiler<IGC::FclOclDeviceCtx>(Os::frontEndDllName, fclLib, fclMain);
    ;
}

bool CompilerInterface::loadIgc() {
    return NEO::loadCompiler<IGC::IgcOclDeviceCtx>(Os::igcDllName, igcLib, igcMain);
}

bool CompilerInterface::initialize(std::unique_ptr<CompilerCache> cache, bool requireFcl) {
    bool fclAvailable = requireFcl ? this->loadFcl() : false;
    bool igcAvailable = this->loadIgc();

    this->cache.swap(cache);

    return this->cache && igcAvailable && (fclAvailable || (false == requireFcl));
}

IGC::FclOclDeviceCtxTagOCL *CompilerInterface::getFclDeviceCtx(const Device &device) {
    auto ulock = this->lock();
    auto it = fclDeviceContexts.find(&device);
    if (it != fclDeviceContexts.end()) {
        return it->second.get();
    }

    if (fclMain == nullptr) {
        DEBUG_BREAK_IF(true); // compiler not available
        return nullptr;
    }

    auto newDeviceCtx = fclMain->CreateInterface<IGC::FclOclDeviceCtxTagOCL>();
    if (newDeviceCtx == nullptr) {
        DEBUG_BREAK_IF(true); // could not create device context
        return nullptr;
    }
    newDeviceCtx->SetOclApiVersion(device.getHardwareInfo().capabilityTable.clVersionSupport * 10);
    fclDeviceContexts[&device] = std::move(newDeviceCtx);

    return fclDeviceContexts[&device].get();
}

IGC::IgcOclDeviceCtxTagOCL *CompilerInterface::getIgcDeviceCtx(const Device &device) {
    auto ulock = this->lock();
    auto it = igcDeviceContexts.find(&device);
    if (it != igcDeviceContexts.end()) {
        return it->second.get();
    }

    if (igcMain == nullptr) {
        DEBUG_BREAK_IF(true); // compiler not available
        return nullptr;
    }

    auto newDeviceCtx = igcMain->CreateInterface<IGC::IgcOclDeviceCtxTagOCL>();
    if (newDeviceCtx == nullptr) {
        DEBUG_BREAK_IF(true); // could not create device context
        return nullptr;
    }

    newDeviceCtx->SetProfilingTimerResolution(static_cast<float>(device.getDeviceInfo().outProfilingTimerResolution));
    auto igcPlatform = newDeviceCtx->GetPlatformHandle();
    auto igcGtSystemInfo = newDeviceCtx->GetGTSystemInfoHandle();
    auto igcFeWa = newDeviceCtx->GetIgcFeaturesAndWorkaroundsHandle();
    if (false == NEO::areNotNullptr(igcPlatform.get(), igcGtSystemInfo.get(), igcFeWa.get())) {
        DEBUG_BREAK_IF(true); // could not acquire handles to device descriptors
        return nullptr;
    }
    const HardwareInfo *hwInfo = &device.getHardwareInfo();
    auto productFamily = DebugManager.flags.ForceCompilerUsePlatform.get();
    if (productFamily != "unk") {
        getHwInfoForPlatformString(productFamily, hwInfo);
    }
    IGC::PlatformHelper::PopulateInterfaceWith(*igcPlatform, hwInfo->platform);
    IGC::GtSysInfoHelper::PopulateInterfaceWith(*igcGtSystemInfo, hwInfo->gtSystemInfo);

    igcFeWa.get()->SetFtrDesktop(device.getHardwareInfo().featureTable.ftrDesktop);
    igcFeWa.get()->SetFtrChannelSwizzlingXOREnabled(device.getHardwareInfo().featureTable.ftrChannelSwizzlingXOREnabled);

    igcFeWa.get()->SetFtrGtBigDie(device.getHardwareInfo().featureTable.ftrGtBigDie);
    igcFeWa.get()->SetFtrGtMediumDie(device.getHardwareInfo().featureTable.ftrGtMediumDie);
    igcFeWa.get()->SetFtrGtSmallDie(device.getHardwareInfo().featureTable.ftrGtSmallDie);

    igcFeWa.get()->SetFtrGT1(device.getHardwareInfo().featureTable.ftrGT1);
    igcFeWa.get()->SetFtrGT1_5(device.getHardwareInfo().featureTable.ftrGT1_5);
    igcFeWa.get()->SetFtrGT2(device.getHardwareInfo().featureTable.ftrGT2);
    igcFeWa.get()->SetFtrGT3(device.getHardwareInfo().featureTable.ftrGT3);
    igcFeWa.get()->SetFtrGT4(device.getHardwareInfo().featureTable.ftrGT4);

    igcFeWa.get()->SetFtrIVBM0M1Platform(device.getHardwareInfo().featureTable.ftrIVBM0M1Platform);
    igcFeWa.get()->SetFtrGTL(device.getHardwareInfo().featureTable.ftrGT1);
    igcFeWa.get()->SetFtrGTM(device.getHardwareInfo().featureTable.ftrGT2);
    igcFeWa.get()->SetFtrGTH(device.getHardwareInfo().featureTable.ftrGT3);

    igcFeWa.get()->SetFtrSGTPVSKUStrapPresent(device.getHardwareInfo().featureTable.ftrSGTPVSKUStrapPresent);
    igcFeWa.get()->SetFtrGTA(device.getHardwareInfo().featureTable.ftrGTA);
    igcFeWa.get()->SetFtrGTC(device.getHardwareInfo().featureTable.ftrGTC);
    igcFeWa.get()->SetFtrGTX(device.getHardwareInfo().featureTable.ftrGTX);
    igcFeWa.get()->SetFtr5Slice(device.getHardwareInfo().featureTable.ftr5Slice);

    igcFeWa.get()->SetFtrGpGpuMidThreadLevelPreempt(device.getHardwareInfo().featureTable.ftrGpGpuMidThreadLevelPreempt);
    igcFeWa.get()->SetFtrIoMmuPageFaulting(device.getHardwareInfo().featureTable.ftrIoMmuPageFaulting);
    igcFeWa.get()->SetFtrWddm2Svm(device.getHardwareInfo().featureTable.ftrWddm2Svm);
    igcFeWa.get()->SetFtrPooledEuEnabled(device.getHardwareInfo().featureTable.ftrPooledEuEnabled);

    igcFeWa.get()->SetFtrResourceStreamer(device.getHardwareInfo().featureTable.ftrResourceStreamer);

    igcDeviceContexts[&device] = std::move(newDeviceCtx);
    return igcDeviceContexts[&device].get();
}

IGC::CodeType::CodeType_t CompilerInterface::getPreferredIntermediateRepresentation(const Device &device) {
    return getFclDeviceCtx(device)->GetPreferredIntermediateRepresentation();
}

CIF::RAII::UPtr_t<IGC::FclOclTranslationCtxTagOCL> CompilerInterface::createFclTranslationCtx(const Device &device, IGC::CodeType::CodeType_t inType, IGC::CodeType::CodeType_t outType) {
    auto deviceCtx = getFclDeviceCtx(device);
    if (deviceCtx == nullptr) {
        DEBUG_BREAK_IF(true); // could not create device context
        return nullptr;
    }

    if (fclBaseTranslationCtx == nullptr) {
        fclBaseTranslationCtx = deviceCtx->CreateTranslationCtx(inType, outType);
    }

    return deviceCtx->CreateTranslationCtx(inType, outType);
}

CIF::RAII::UPtr_t<IGC::IgcOclTranslationCtxTagOCL> CompilerInterface::createIgcTranslationCtx(const Device &device, IGC::CodeType::CodeType_t inType, IGC::CodeType::CodeType_t outType) {
    auto deviceCtx = getIgcDeviceCtx(device);
    if (deviceCtx == nullptr) {
        DEBUG_BREAK_IF(true); // could not create device context
        return nullptr;
    }

    return deviceCtx->CreateTranslationCtx(inType, outType);
}

} // namespace NEO
