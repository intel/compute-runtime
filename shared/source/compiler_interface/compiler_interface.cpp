/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_interface.h"

#include "shared/source/built_ins/sip_kernel_type.h"
#include "shared/source/compiler_interface/compiler_cache.h"
#include "shared/source/compiler_interface/compiler_interface.inl"
#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/compiler_interface/igc_platform_helper.h"
#include "shared/source/compiler_interface/os_compiler_cache_helper.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/os_inc_base.h"

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

void TranslationOutput::makeCopy(MemAndSize &dst, CIF::Builtins::BufferSimple *src) {
    if ((nullptr == src) || (src->GetSizeRaw() == 0)) {
        dst.mem.reset();
        dst.size = 0U;
        return;
    }

    dst.size = src->GetSize<char>();
    dst.mem = ::makeCopy(src->GetMemory<void>(), src->GetSize<char>());
}

CompilerInterface::CompilerInterface()
    : cache() {
    if (debugManager.flags.FinalizerInputType.get() != "unk") {
        this->finalizerInputType = IGC::CodeType::CodeTypeCoder::Enc(debugManager.flags.FinalizerInputType.get().c_str());
    }
}
CompilerInterface::~CompilerInterface() = default;

TranslationOutput::ErrorCode CompilerInterface::build(
    const NEO::Device &device,
    const TranslationInput &input,
    TranslationOutput &output) {
    if (false == isCompilerAvailable(&device, input.srcType, input.outType)) {
        return TranslationOutput::ErrorCode::compilerNotAvailable;
    }

    IGC::CodeType::CodeType_t srcCodeType = input.srcType;
    IGC::CodeType::CodeType_t intermediateCodeType = IGC::CodeType::undefined;

    if (input.preferredIntermediateType != IGC::CodeType::undefined) {
        intermediateCodeType = input.preferredIntermediateType;
    }

    CachingMode cachingMode = CompilerCacheHelper::getCachingMode(cache.get(), srcCodeType, input.src);

    std::string kernelFileHash;
    const auto &igc = *getIgc(&device);
    if (cachingMode == CachingMode::direct) {
        kernelFileHash = cache->getCachedFileName(device.getHardwareInfo(),
                                                  input.src,
                                                  input.apiOptions,
                                                  input.internalOptions, ArrayRef<const char>(), ArrayRef<const char>(), igc.revision, igc.libSize, igc.libMTime);

        bool success = CompilerCacheHelper::loadCacheAndSetOutput(*cache, kernelFileHash, output, device);
        if (success) {
            return TranslationOutput::ErrorCode::success;
        }
    }

    auto *igcMain = igc.entryPoint.get();
    auto inSrc = CIF::Builtins::CreateConstBuffer(igcMain, input.src.begin(), input.src.size());
    auto fclOptions = CIF::Builtins::CreateConstBuffer(igcMain, input.apiOptions.begin(), input.apiOptions.size());
    auto fclInternalOptions = CIF::Builtins::CreateConstBuffer(igcMain, input.internalOptions.begin(), input.internalOptions.size());

    auto idsBuffer = CIF::Builtins::CreateConstBuffer(igcMain, nullptr, 0);
    auto valuesBuffer = CIF::Builtins::CreateConstBuffer(igcMain, nullptr, 0);
    for (const auto &specConst : input.specializedValues) {
        idsBuffer->PushBackRawCopy(specConst.first);
        valuesBuffer->PushBackRawCopy(specConst.second);
    }

    CIF::RAII::UPtr_t<CIF::Builtins::BufferSimple> intermediateRepresentation;

    if (srcCodeType == IGC::CodeType::oclC) {
        if (intermediateCodeType == IGC::CodeType::undefined) {
            intermediateCodeType = getPreferredIntermediateRepresentation(device);
        }

        CIF::RAII::UPtr_t<IGC::OclTranslationOutputTagOCL> fclOutput;
        if (this->useIgcAsFcl(&device)) {
            auto igcTranslationCtx = createIgcTranslationCtx(device, srcCodeType, intermediateCodeType);
            fclOutput = translate(igcTranslationCtx.get(), inSrc.get(),
                                  fclOptions.get(), fclInternalOptions.get());
        } else {
            auto fclTranslationCtx = createFclTranslationCtx(device, srcCodeType, intermediateCodeType);
            fclOutput = translate(fclTranslationCtx.get(), inSrc.get(),
                                  fclOptions.get(), fclInternalOptions.get());
        }

        if (fclOutput == nullptr) {
            return TranslationOutput::ErrorCode::unknownError;
        }

        TranslationOutput::makeCopy(output.frontendCompilerLog, fclOutput->GetBuildLog());

        if (fclOutput->Successful() == false) {
            return TranslationOutput::ErrorCode::buildFailure;
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

    if (cachingMode == CachingMode::preProcess) {
        const ArrayRef<const char> irRef(intermediateRepresentation->GetMemory<char>(), intermediateRepresentation->GetSize<char>());
        const ArrayRef<const char> specIdsRef(idsBuffer->GetMemory<char>(), idsBuffer->GetSize<char>());
        const ArrayRef<const char> specValuesRef(valuesBuffer->GetMemory<char>(), valuesBuffer->GetSize<char>());
        const auto &igc = *getIgc(&device);
        kernelFileHash = cache->getCachedFileName(device.getHardwareInfo(), irRef,
                                                  input.apiOptions,
                                                  input.internalOptions, specIdsRef, specValuesRef, igc.revision, igc.libSize, igc.libMTime);

        bool success = CompilerCacheHelper::loadCacheAndSetOutput(*cache, kernelFileHash, output, device);
        if (success) {
            return TranslationOutput::ErrorCode::success;
        }
    }

    auto igcOutputType = IGC::CodeType::oclGenBin;
    if (this->finalizerInputType != IGC::CodeType::undefined) {
        igcOutputType = this->finalizerInputType;
    }

    auto igcTranslationCtx = createIgcTranslationCtx(device, intermediateCodeType, igcOutputType);

    auto buildOutput = translate(igcTranslationCtx.get(), intermediateRepresentation.get(), idsBuffer.get(), valuesBuffer.get(),
                                 fclOptions.get(), fclInternalOptions.get(), input.gtPinInput);

    if (buildOutput == nullptr) {
        return TranslationOutput::ErrorCode::unknownError;
    }

    TranslationOutput::makeCopy(output.backendCompilerLog, buildOutput->GetBuildLog());

    if (buildOutput->Successful() == false) {
        return TranslationOutput::ErrorCode::buildFailure;
    }

    if (igcOutputType == this->finalizerInputType) {
        TranslationOutput::makeCopy(output.finalizerInputRepresentation, buildOutput->GetOutput());

        auto finalizerTranslationCtx = createFinalizerTranslationCtx(device, this->finalizerInputType, IGC::CodeType::oclGenBin);

        auto finalizerOutput = translate(finalizerTranslationCtx.get(), buildOutput->GetOutput(),
                                         fclOptions.get(), fclInternalOptions.get(), nullptr);
        buildOutput = std::move(finalizerOutput);

        TranslationOutput::append(output.backendCompilerLog, buildOutput->GetBuildLog(), "\n", 0);
        if (buildOutput->Successful() == false) {
            return TranslationOutput::ErrorCode::buildFailure;
        }
    }

    TranslationOutput::makeCopy(output.deviceBinary, buildOutput->GetOutput());
    TranslationOutput::makeCopy(output.debugData, buildOutput->GetDebugData());

    if (cache != nullptr && cache->getConfig().enabled) {
        CompilerCacheHelper::packAndCacheBinary(*cache, kernelFileHash, NEO::getTargetDevice(device.getRootDeviceEnvironment()), output);
    }

    return TranslationOutput::ErrorCode::success;
}

TranslationOutput::ErrorCode CompilerInterface::compile(
    const NEO::Device &device,
    const TranslationInput &input,
    TranslationOutput &output) {

    if ((IGC::CodeType::oclC != input.srcType) && (IGC::CodeType::elf != input.srcType)) {
        return TranslationOutput::ErrorCode::alreadyCompiled;
    }

    if (false == isCompilerAvailable(&device, input.srcType, input.outType)) {
        return TranslationOutput::ErrorCode::compilerNotAvailable;
    }

    auto outType = input.outType;

    if (outType == IGC::CodeType::undefined) {
        outType = getPreferredIntermediateRepresentation(device);
    }

    CIF::CIFMain *fclMain = nullptr;
    if (this->useIgcAsFcl(&device)) {
        fclMain = getIgc(&device)->entryPoint.get();
    } else {
        fclMain = fcl.entryPoint.get();
    }
    auto fclSrc = CIF::Builtins::CreateConstBuffer(fclMain, input.src.begin(), input.src.size());
    auto fclOptions = CIF::Builtins::CreateConstBuffer(fclMain, input.apiOptions.begin(), input.apiOptions.size());
    auto fclInternalOptions = CIF::Builtins::CreateConstBuffer(fclMain, input.internalOptions.begin(), input.internalOptions.size());

    CIF::RAII::UPtr_t<IGC::OclTranslationOutputTagOCL> fclOutput;
    if (this->useIgcAsFcl(&device)) {
        auto igcTranslationCtx = createIgcTranslationCtx(device, input.srcType, outType);
        fclOutput = translate(igcTranslationCtx.get(), fclSrc.get(),
                              fclOptions.get(), fclInternalOptions.get());
    } else {
        auto fclTranslationCtx = createFclTranslationCtx(device, input.srcType, outType);
        fclOutput = translate(fclTranslationCtx.get(), fclSrc.get(),
                              fclOptions.get(), fclInternalOptions.get());
    }

    if (fclOutput == nullptr) {
        return TranslationOutput::ErrorCode::unknownError;
    }

    TranslationOutput::makeCopy(output.frontendCompilerLog, fclOutput->GetBuildLog());

    if (fclOutput->Successful() == false) {
        return TranslationOutput::ErrorCode::compilationFailure;
    }

    output.intermediateCodeType = outType;
    TranslationOutput::makeCopy(output.intermediateRepresentation, fclOutput->GetOutput());

    return TranslationOutput::ErrorCode::success;
}

TranslationOutput::ErrorCode CompilerInterface::link(
    const NEO::Device &device,
    const TranslationInput &input,
    TranslationOutput &output) {
    if (false == isCompilerAvailable(&device, input.srcType, input.outType)) {
        return TranslationOutput::ErrorCode::compilerNotAvailable;
    }

    auto *igcMain = getIgc(&device)->entryPoint.get();
    auto inSrc = CIF::Builtins::CreateConstBuffer(igcMain, input.src.begin(), input.src.size());
    auto igcOptions = CIF::Builtins::CreateConstBuffer(igcMain, input.apiOptions.begin(), input.apiOptions.size());
    auto igcInternalOptions = CIF::Builtins::CreateConstBuffer(igcMain, input.internalOptions.begin(), input.internalOptions.size());

    if (inSrc == nullptr) {
        return TranslationOutput::ErrorCode::unknownError;
    }

    CIF::RAII::UPtr_t<IGC::OclTranslationOutputTagOCL> currOut;
    inSrc->Retain(); // shared with currSrc
    CIF::RAII::UPtr_t<CIF::Builtins::BufferSimple> currSrc(inSrc.get());
    IGC::CodeType::CodeType_t translationChain[] = {IGC::CodeType::elf, IGC::CodeType::oclGenBin};
    constexpr size_t numTranslations = sizeof(translationChain) / sizeof(translationChain[0]);
    for (size_t ti = 1; ti < numTranslations; ti++) {
        IGC::CodeType::CodeType_t inType = translationChain[ti - 1];
        IGC::CodeType::CodeType_t outType = translationChain[ti];

        auto igcTranslationCtx = createIgcTranslationCtx(device, inType, outType);
        currOut = translate(igcTranslationCtx.get(), currSrc.get(),
                            igcOptions.get(), igcInternalOptions.get(), input.gtPinInput);

        if (currOut == nullptr) {
            return TranslationOutput::ErrorCode::unknownError;
        }

        if (currOut->Successful() == false) {
            TranslationOutput::makeCopy(output.backendCompilerLog, currOut->GetBuildLog());
            return TranslationOutput::ErrorCode::linkFailure;
        }

        currOut->GetOutput()->Retain(); // shared with currSrc
        currSrc.reset(currOut->GetOutput());
    }

    TranslationOutput::makeCopy(output.backendCompilerLog, currOut->GetBuildLog());
    TranslationOutput::makeCopy(output.deviceBinary, currOut->GetOutput());
    TranslationOutput::makeCopy(output.debugData, currOut->GetDebugData());

    return TranslationOutput::ErrorCode::success;
}

TranslationOutput::ErrorCode CompilerInterface::getSpecConstantsInfo(const NEO::Device &device, ArrayRef<const char> srcSpirV, SpecConstantInfo &output) {
    if (false == isIgcAvailable(&device)) {
        return TranslationOutput::ErrorCode::compilerNotAvailable;
    }

    auto igcTranslationCtx = createIgcTranslationCtx(device, IGC::CodeType::spirV, IGC::CodeType::oclGenBin);

    auto *igcMain = getIgc(&device)->entryPoint.get();
    auto inSrc = CIF::Builtins::CreateConstBuffer(igcMain, srcSpirV.begin(), srcSpirV.size());
    output.idsBuffer = CIF::Builtins::CreateConstBuffer(igcMain, nullptr, 0);
    output.sizesBuffer = CIF::Builtins::CreateConstBuffer(igcMain, nullptr, 0);

    auto retVal = getSpecConstantsInfoImpl(igcTranslationCtx.get(), inSrc.get(), output.idsBuffer.get(), output.sizesBuffer.get());

    if (!retVal) {
        return TranslationOutput::ErrorCode::unknownError;
    }

    return TranslationOutput::ErrorCode::success;
}

TranslationOutput::ErrorCode CompilerInterface::createLibrary(
    NEO::Device &device,
    const TranslationInput &input,
    TranslationOutput &output) {
    if (false == isIgcAvailable(&device)) {
        return TranslationOutput::ErrorCode::compilerNotAvailable;
    }

    auto *igcMain = getIgc(&device)->entryPoint.get();
    auto igcSrc = CIF::Builtins::CreateConstBuffer(igcMain, input.src.begin(), input.src.size());
    auto igcOptions = CIF::Builtins::CreateConstBuffer(igcMain, input.apiOptions.begin(), input.apiOptions.size());
    auto igcInternalOptions = CIF::Builtins::CreateConstBuffer(igcMain, input.internalOptions.begin(), input.internalOptions.size());

    auto intermediateRepresentation = IGC::CodeType::llvmBc;
    auto igcTranslationCtx = createIgcTranslationCtx(device, IGC::CodeType::elf, intermediateRepresentation);

    auto igcOutput = translate(igcTranslationCtx.get(), igcSrc.get(),
                               igcOptions.get(), igcInternalOptions.get());

    if (igcOutput == nullptr) {
        return TranslationOutput::ErrorCode::unknownError;
    }

    TranslationOutput::makeCopy(output.backendCompilerLog, igcOutput->GetBuildLog());

    if (igcOutput->Successful() == false) {
        return TranslationOutput::ErrorCode::linkFailure;
    }

    output.intermediateCodeType = intermediateRepresentation;
    TranslationOutput::makeCopy(output.intermediateRepresentation, igcOutput->GetOutput());

    return TranslationOutput::ErrorCode::success;
}

TranslationOutput::ErrorCode CompilerInterface::getSipKernelBinary(NEO::Device &device, SipKernelType type, std::vector<char> &retBinary,
                                                                   std::vector<char> &stateSaveAreaHeader) {
    if (false == isIgcAvailable(&device)) {
        return TranslationOutput::ErrorCode::compilerNotAvailable;
    }

    bool bindlessSip = false;
    IGC::SystemRoutineType::SystemRoutineType_t typeOfSystemRoutine = IGC::SystemRoutineType::undefined;
    switch (type) {
    case SipKernelType::csr:
        typeOfSystemRoutine = IGC::SystemRoutineType::contextSaveRestore;
        break;
    case SipKernelType::dbgCsr:
        typeOfSystemRoutine = IGC::SystemRoutineType::debug;
        break;
    case SipKernelType::dbgCsrLocal:
        typeOfSystemRoutine = IGC::SystemRoutineType::debugSlm;
        break;
    case SipKernelType::dbgBindless:
        typeOfSystemRoutine = IGC::SystemRoutineType::debug;
        bindlessSip = true;
        break;
    case SipKernelType::dbgHeapless:
        typeOfSystemRoutine = IGC::SystemRoutineType::debug;
        bindlessSip = false;
        break;
    default:
        break;
    }

    auto deviceCtx = getIgcDeviceCtx(device);

    if (deviceCtx == nullptr) {
        return TranslationOutput::ErrorCode::unknownError;
    }

    auto *igcMain = getIgc(&device)->entryPoint.get();
    auto systemRoutineBuffer = igcMain->CreateBuiltin<CIF::Builtins::BufferLatest>();
    auto stateSaveAreaBuffer = igcMain->CreateBuiltin<CIF::Builtins::BufferLatest>();

    auto result = deviceCtx->GetSystemRoutine(typeOfSystemRoutine,
                                              bindlessSip,
                                              systemRoutineBuffer.get(),
                                              stateSaveAreaBuffer.get());

    if (!result) {
        return TranslationOutput::ErrorCode::unknownError;
    }

    retBinary.assign(systemRoutineBuffer->GetMemory<char>(), systemRoutineBuffer->GetMemory<char>() + systemRoutineBuffer->GetSizeRaw());
    stateSaveAreaHeader.assign(stateSaveAreaBuffer->GetMemory<char>(), stateSaveAreaBuffer->GetMemory<char>() + stateSaveAreaBuffer->GetSizeRaw());

    return TranslationOutput::ErrorCode::success;
}

CIF::RAII::UPtr_t<IGC::IgcFeaturesAndWorkaroundsTagOCL> CompilerInterface::getIgcFeaturesAndWorkarounds(NEO::Device const &device) {
    return getIgcDeviceCtx(device)->GetIgcFeaturesAndWorkaroundsHandle();
}

bool CompilerInterface::loadFcl() {
    return NEO::loadCompiler<IGC::FclOclDeviceCtx>(Os::frontEndDllName, fcl.library, fcl.entryPoint);
}

bool CompilerInterface::loadIgcBasedCompiler(CompilerLibraryEntry &entry, const char *libName) {
    bool result = NEO::loadCompiler<IGC::IgcOclDeviceCtx>(libName, entry.library, entry.entryPoint);

    if (result) {
        std::string libPath = entry.library->getFullPath();
        entry.libSize = NEO::getFileSize(libPath);
        entry.libMTime = NEO::getFileModificationTime(libPath);

        auto igcDeviceCtx3 = entry.entryPoint->CreateInterface<IGC::IgcOclDeviceCtx<3>>();
        if (igcDeviceCtx3) {
            entry.revision = igcDeviceCtx3->GetIGCRevision();
        }
    }
    return result;
}

bool CompilerInterface::initialize(std::unique_ptr<CompilerCache> &&cache, bool requireFcl) {
    bool fclAvailable = requireFcl ? this->loadFcl() : false;
    bool igcAvailable = this->loadIgcBasedCompiler(defaultIgc, Os::igcDllName);

    this->cache.swap(cache);

    return this->cache && igcAvailable && (fclAvailable || (false == requireFcl));
}

IGC::FclOclDeviceCtxTagOCL *CompilerInterface::getFclDeviceCtx(const Device &device) {
    auto ulock = this->lock();
    auto it = fclDeviceContexts.find(&device);
    if (it != fclDeviceContexts.end()) {
        return it->second.get();
    }

    if (fcl.entryPoint == nullptr) {
        DEBUG_BREAK_IF(true); // compiler not available
        return nullptr;
    }

    auto newDeviceCtx = fcl.entryPoint->CreateInterface<IGC::FclOclDeviceCtxTagOCL>();
    if (newDeviceCtx == nullptr) {
        DEBUG_BREAK_IF(true); // could not create device context
        return nullptr;
    }
    newDeviceCtx->SetOclApiVersion(device.getHardwareInfo().capabilityTable.clVersionSupport * 10);
    if (newDeviceCtx->GetUnderlyingVersion() > 4U) {
        auto igcPlatform = newDeviceCtx->GetPlatformHandle();
        if (nullptr == igcPlatform.get()) {
            DEBUG_BREAK_IF(true); // could not acquire handles to platform descriptor
            return nullptr;
        }
        const auto &hwInfo = device.getHardwareInfo();
        populateIgcPlatform(*igcPlatform, hwInfo);
    }
    fclDeviceContexts[&device] = std::move(newDeviceCtx);

    return fclDeviceContexts[&device].get();
}

IGC::IgcOclDeviceCtxTagOCL *CompilerInterface::getIgcDeviceCtx(const Device &device) {
    auto ulock = this->lock();
    auto it = igcDeviceContexts.find(&device);
    if (it != igcDeviceContexts.end()) {
        return it->second.get();
    }

    auto *igc = getIgc(&device);
    if (igc == nullptr) {
        DEBUG_BREAK_IF(true); // compiler not available
        return nullptr;
    }
    auto *igcMain = igc->entryPoint.get();

    auto newDeviceCtx = igcMain->CreateInterface<IGC::IgcOclDeviceCtxTagOCL>();
    if (newDeviceCtx == nullptr) {
        DEBUG_BREAK_IF(true); // could not create device context
        return nullptr;
    }

    newDeviceCtx->SetProfilingTimerResolution(static_cast<float>(device.getDeviceInfo().outProfilingTimerResolution));
    auto igcPlatform = newDeviceCtx->GetPlatformHandle();
    auto igcGtSystemInfo = newDeviceCtx->GetGTSystemInfoHandle();
    auto igcFtrWa = newDeviceCtx->GetIgcFeaturesAndWorkaroundsHandle();
    if (false == NEO::areNotNullptr(igcPlatform.get(), igcGtSystemInfo.get(), igcFtrWa.get())) {
        DEBUG_BREAK_IF(true); // could not acquire handles to device descriptors
        return nullptr;
    }
    const HardwareInfo *hwInfo = &device.getHardwareInfo();
    auto productFamily = debugManager.flags.ForceCompilerUsePlatform.get();
    if (productFamily != "unk") {
        getHwInfoForPlatformString(productFamily, hwInfo);
    }

    populateIgcPlatform(*igcPlatform, *hwInfo);
    IGC::GtSysInfoHelper::PopulateInterfaceWith(*igcGtSystemInfo, hwInfo->gtSystemInfo);

    auto &compilerProductHelper = device.getCompilerProductHelper();
    igcFtrWa->SetFtrGpGpuMidThreadLevelPreempt(compilerProductHelper.isMidThreadPreemptionSupported(*hwInfo));
    igcFtrWa->SetFtrWddm2Svm(device.getHardwareInfo().featureTable.flags.ftrWddm2Svm);
    igcFtrWa->SetFtrPooledEuEnabled(device.getHardwareInfo().featureTable.flags.ftrPooledEuEnabled);

    igcDeviceContexts[&device] = std::move(newDeviceCtx);
    return igcDeviceContexts[&device].get();
}

IGC::IgcOclDeviceCtxTagOCL *CompilerInterface::getFinalizerDeviceCtx(const Device &device) {
    auto ulock = this->lock();
    auto it = finalizerDeviceContexts.find(&device);
    if (it != finalizerDeviceContexts.end()) {
        return it->second.get();
    }

    auto finalizer = this->getFinalizer(&device);
    if (finalizer == nullptr) {
        DEBUG_BREAK_IF(true); // compiler not available
        return nullptr;
    }

    auto newDeviceCtx = finalizer->entryPoint->CreateInterface<IGC::IgcOclDeviceCtxTagOCL>();
    if (newDeviceCtx == nullptr) {
        DEBUG_BREAK_IF(true); // could not create device context
        return nullptr;
    }

    newDeviceCtx->SetProfilingTimerResolution(static_cast<float>(device.getDeviceInfo().outProfilingTimerResolution));
    auto igcPlatform = newDeviceCtx->GetPlatformHandle();
    auto igcGtSystemInfo = newDeviceCtx->GetGTSystemInfoHandle();
    auto igcFtrWa = newDeviceCtx->GetIgcFeaturesAndWorkaroundsHandle();
    if (false == NEO::areNotNullptr(igcPlatform.get(), igcGtSystemInfo.get(), igcFtrWa.get())) {
        DEBUG_BREAK_IF(true); // could not acquire handles to device descriptors
        return nullptr;
    }
    const HardwareInfo *hwInfo = &device.getHardwareInfo();
    auto productFamily = debugManager.flags.ForceCompilerUsePlatform.get();
    if (productFamily != "unk") {
        getHwInfoForPlatformString(productFamily, hwInfo);
    }

    populateIgcPlatform(*igcPlatform, *hwInfo);
    IGC::GtSysInfoHelper::PopulateInterfaceWith(*igcGtSystemInfo, hwInfo->gtSystemInfo);

    auto &compilerProductHelper = device.getCompilerProductHelper();
    igcFtrWa->SetFtrGpGpuMidThreadLevelPreempt(compilerProductHelper.isMidThreadPreemptionSupported(*hwInfo));
    igcFtrWa->SetFtrWddm2Svm(device.getHardwareInfo().featureTable.flags.ftrWddm2Svm);
    igcFtrWa->SetFtrPooledEuEnabled(device.getHardwareInfo().featureTable.flags.ftrPooledEuEnabled);

    finalizerDeviceContexts[&device] = std::move(newDeviceCtx);
    return finalizerDeviceContexts[&device].get();
}

IGC::CodeType::CodeType_t CompilerInterface::getPreferredIntermediateRepresentation(const Device &device) {
    if (useIgcAsFcl(&device)) {
        return device.getCompilerProductHelper().getPreferredIntermediateRepresentation();
    } else {
        return getFclDeviceCtx(device)->GetPreferredIntermediateRepresentation();
    }
}

CIF::RAII::UPtr_t<IGC::FclOclTranslationCtxTagOCL> CompilerInterface::createFclTranslationCtx(const Device &device, IGC::CodeType::CodeType_t inType, IGC::CodeType::CodeType_t outType) {
    auto deviceCtx = getFclDeviceCtx(device);
    if (deviceCtx == nullptr) {
        DEBUG_BREAK_IF(true); // could not create device context
        return nullptr;
    }

    if (fclBaseTranslationCtx == nullptr) {
        auto ulock = this->lock();
        if (fclBaseTranslationCtx == nullptr) {
            fclBaseTranslationCtx = deviceCtx->CreateTranslationCtx(inType, outType);
        }
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

CIF::RAII::UPtr_t<IGC::IgcOclTranslationCtxTagOCL> CompilerInterface::createFinalizerTranslationCtx(const Device &device, IGC::CodeType::CodeType_t inType, IGC::CodeType::CodeType_t outType) {
    auto deviceCtx = getFinalizerDeviceCtx(device);
    if (deviceCtx == nullptr) {
        DEBUG_BREAK_IF(true); // could not create device context
        return nullptr;
    }

    return deviceCtx->CreateTranslationCtx(inType, outType);
}

bool CompilerInterface::addOptionDisableZebin(std::string &options, std::string &internalOptions) {
    CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::disableZebin);
    return true;
}

bool CompilerInterface::disableZebin(std::string &options, std::string &internalOptions) {
    return addOptionDisableZebin(options, internalOptions);
}

bool CompilerInterface::isFclAvailable(const Device *device) {
    return useIgcAsFcl(device) ? isIgcAvailable(device) : static_cast<bool>(fcl.entryPoint);
}

bool CompilerInterface::isIgcAvailable(const Device *device) {
    return nullptr != getIgc(device);
}

bool CompilerInterface::isFinalizerAvailable(const Device *device) {
    return nullptr != getFinalizer(device);
}

bool CompilerInterface::useIgcAsFcl(const Device *device) {
    if (0 != debugManager.flags.UseIgcAsFcl.get()) {
        if (1 == debugManager.flags.UseIgcAsFcl.get()) {
            return true;
        } else if (2 == debugManager.flags.UseIgcAsFcl.get()) {
            return false;
        }
    }

    if (nullptr == device) {
        return false;
    }

    return device->getCompilerProductHelper().useIgcAsFcl();
}

const CompilerInterface::CompilerLibraryEntry *CompilerInterface::getIgc(const Device *device) {
    if (nullptr == device) {
        if (defaultIgc.entryPoint == nullptr) {
            return nullptr;
        }
        return &defaultIgc;
    }
    return getIgc(device->getCompilerProductHelper().getCustomIgcLibraryName());
}

const CompilerInterface::CompilerLibraryEntry *CompilerInterface::getFinalizer(const Device *device) {
    if (nullptr == device) {
        return nullptr;
    }

    const char *finalizerLibName = device->getCompilerProductHelper().getFinalizerLibraryName();
    if (debugManager.flags.FinalizerLibraryName.get() != "unk") {
        finalizerLibName = debugManager.flags.FinalizerLibraryName.getRef().c_str();
    }

    return getFinalizer(finalizerLibName);
}

const CompilerInterface::CompilerLibraryEntry *CompilerInterface::getCustomCompilerLibrary(const char *libName) {
    std::lock_guard<decltype(customCompilerLibraryLoadMutex)> lock{customCompilerLibraryLoadMutex};
    auto it = customCompilerLibraries.find(libName);
    if (it != customCompilerLibraries.end()) {
        return it->second.get();
    }

    CompilerLibraryEntry newEntry = {};
    this->loadIgcBasedCompiler(newEntry, libName);
    if (newEntry.entryPoint == nullptr) {
        return nullptr;
    }

    customCompilerLibraries[libName].reset(new CompilerLibraryEntry(std::move(newEntry)));

    return customCompilerLibraries[libName].get();
}

void CompilerCacheHelper::packAndCacheBinary(CompilerCache &compilerCache, const std::string &kernelFileHash, const NEO::TargetDevice &targetDevice, const NEO::TranslationOutput &translationOutput) {
    NEO::SingleDeviceBinary singleDeviceBinary = {};
    singleDeviceBinary.targetDevice = targetDevice;
    singleDeviceBinary.deviceBinary = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(translationOutput.deviceBinary.mem.get()), translationOutput.deviceBinary.size);
    singleDeviceBinary.debugData = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(translationOutput.debugData.mem.get()), translationOutput.debugData.size);
    singleDeviceBinary.intermediateRepresentation = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(translationOutput.intermediateRepresentation.mem.get()), translationOutput.intermediateRepresentation.size);

    if (NEO::isAnyPackedDeviceBinaryFormat(singleDeviceBinary.deviceBinary)) {
        compilerCache.cacheBinary(kernelFileHash, translationOutput.deviceBinary.mem.get(), translationOutput.deviceBinary.size);
        return;
    }

    std::string packWarnings;
    std::string packErrors;
    auto packedBinary = packDeviceBinary<DeviceBinaryFormat::oclElf>(singleDeviceBinary, packErrors, packWarnings);

    if (false == packedBinary.empty()) {
        compilerCache.cacheBinary(kernelFileHash, reinterpret_cast<const char *>(packedBinary.data()), packedBinary.size());
    }
}

bool CompilerCacheHelper::loadCacheAndSetOutput(CompilerCache &compilerCache, const std::string &kernelFileHash, NEO::TranslationOutput &output, const NEO::Device &device) {
    size_t cacheBinarySize = 0u;
    auto cacheBinary = compilerCache.loadCachedBinary(kernelFileHash, cacheBinarySize);

    if (cacheBinary) {
        ArrayRef<const uint8_t> archive(reinterpret_cast<const uint8_t *>(cacheBinary.get()), cacheBinarySize);

        if (isDeviceBinaryFormat<DeviceBinaryFormat::oclElf>(archive)) {
            bool success = processPackedCacheBinary(archive, output, device);
            if (success) {
                return true;
            }
        } else {
            output.deviceBinary.mem = std::move(cacheBinary);
            output.deviceBinary.size = cacheBinarySize;
            return true;
        }
    }

    return false;
}

bool CompilerCacheHelper::processPackedCacheBinary(ArrayRef<const uint8_t> archive, TranslationOutput &output, const NEO::Device &device) {
    auto productAbbreviation = NEO::hardwarePrefix[device.getHardwareInfo().platform.eProductFamily];
    NEO::TargetDevice targetDevice = NEO::getTargetDevice(device.getRootDeviceEnvironment());
    std::string decodeErrors;
    std::string decodeWarnings;
    auto singleDeviceBinary = unpackSingleDeviceBinary(archive, NEO::ConstStringRef(productAbbreviation, strlen(productAbbreviation)), targetDevice,
                                                       decodeErrors, decodeWarnings);

    if (false == singleDeviceBinary.deviceBinary.empty()) {
        if (nullptr == output.deviceBinary.mem) {
            output.deviceBinary.mem = makeCopy<char>(reinterpret_cast<const char *>(singleDeviceBinary.deviceBinary.begin()), singleDeviceBinary.deviceBinary.size());
            output.deviceBinary.size = singleDeviceBinary.deviceBinary.size();
        }

        if (false == singleDeviceBinary.intermediateRepresentation.empty() &&
            nullptr == output.intermediateRepresentation.mem) {
            output.intermediateRepresentation.mem = makeCopy(reinterpret_cast<const char *>(singleDeviceBinary.intermediateRepresentation.begin()), singleDeviceBinary.intermediateRepresentation.size());
            output.intermediateRepresentation.size = singleDeviceBinary.intermediateRepresentation.size();
        }

        if (false == singleDeviceBinary.debugData.empty() &&
            nullptr == output.debugData.mem) {
            output.debugData.mem = makeCopy(reinterpret_cast<const char *>(singleDeviceBinary.debugData.begin()), singleDeviceBinary.debugData.size());
            output.debugData.size = singleDeviceBinary.debugData.size();
        }

        return true;
    }

    return false;
}

CachingMode CompilerCacheHelper::getCachingMode(CompilerCache *compilerCache, IGC::CodeType::CodeType_t srcCodeType, const ArrayRef<const char> source) {
    if (compilerCache == nullptr || !compilerCache->getConfig().enabled) {
        return CachingMode::none;
    }

    if (srcCodeType == IGC::CodeType::oclC &&
        validateIncludes(source, CompilerCacheHelper::whitelistedIncludes)) {
        return CachingMode::direct;
    }

    return CachingMode::preProcess;
}

bool CompilerCacheHelper::validateIncludes(const ArrayRef<const char> source, const WhitelistedIncludesVec &whitelistedIncludes) {
    const char *sourcePtr = source.begin();
    const char *sourceEnd = source.end();

    while (sourcePtr < sourceEnd) {
        const char *includePos = std::strstr(sourcePtr, "#include");
        if (includePos == nullptr) {
            break;
        }

        bool isKnownInclude = false;
        for (const auto &knownInclude : whitelistedIncludes) {
            if (std::strncmp(includePos, knownInclude.data(), knownInclude.size()) == 0) {
                isKnownInclude = true;
                break;
            }
        }

        if (!isKnownInclude) {
            return false;
        }

        sourcePtr = includePos + 1;
    }

    return true;
}

CompilerCacheHelper::WhitelistedIncludesVec CompilerCacheHelper::whitelistedIncludes{
    "#include <cm/cm.h>",
    "#include <cm/cmtl.h>"};

} // namespace NEO