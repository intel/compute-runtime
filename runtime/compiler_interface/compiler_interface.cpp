/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cif/common/cif_main.h"
#include "cif/helpers/error.h"
#include "cif/import/library_api.h"
#include "ocl_igc_interface/code_type.h"
#include "ocl_igc_interface/fcl_ocl_device_ctx.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"
#include "ocl_igc_interface/platform_helper.h"
#include "runtime/compiler_interface/binary_cache.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/compiler_interface/compiler_interface.inl"
#include "runtime/helpers/hw_info.h"
#include "runtime/program/program.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/os_interface/os_inc_base.h"

#include <fstream>

namespace OCLRT {
bool CompilerInterface::useLlvmText = false;
std::mutex CompilerInterface::mtx;

enum CachingMode {
    None,
    Direct,
    PreProcess
};

CompilerInterface::CompilerInterface() = default;
CompilerInterface::~CompilerInterface() = default;
NO_SANITIZE
cl_int CompilerInterface::build(
    Program &program,
    const TranslationArgs &inputArgs,
    bool enableCaching) {
    if (false == isCompilerAvailable()) {
        return CL_COMPILER_NOT_AVAILABLE;
    }

    IGC::CodeType::CodeType_t highLevelCodeType = IGC::CodeType::undefined;
    IGC::CodeType::CodeType_t intermediateCodeType = IGC::CodeType::undefined;

    if (program.getProgramBinaryType() == CL_PROGRAM_BINARY_TYPE_INTERMEDIATE) {
        if (program.getIsSpirV()) {
            intermediateCodeType = IGC::CodeType::spirV;
        } else {
            intermediateCodeType = IGC::CodeType::llvmBc;
        }
    } else {
        highLevelCodeType = IGC::CodeType::oclC;
        if (useLlvmText == true) {
            intermediateCodeType = IGC::CodeType::llvmLl;
        }
    }

    CachingMode cachingMode = None;

    if (enableCaching) {
        if ((highLevelCodeType == IGC::CodeType::oclC) && (std::strstr(inputArgs.pInput, "#include") == nullptr)) {
            cachingMode = CachingMode::Direct;
        } else {
            cachingMode = CachingMode::PreProcess;
        }
    }

    uint32_t numDevices = static_cast<uint32_t>(program.getNumDevices());
    for (uint32_t i = 0; i < numDevices; i++) {
        const auto &device = program.getDevice(i);
        if (intermediateCodeType == IGC::CodeType::undefined) {
            UNRECOVERABLE_IF(highLevelCodeType != IGC::CodeType::oclC);
            intermediateCodeType = getPreferredIntermediateRepresentation(device);
        }

        bool binaryLoaded = false;
        std::string kernelFileHash;
        if (cachingMode == CachingMode::Direct) {
            kernelFileHash = cache->getCachedFileName(device.getHardwareInfo(),
                                                      ArrayRef<const char>(inputArgs.pInput, inputArgs.InputSize),
                                                      ArrayRef<const char>(inputArgs.pOptions, inputArgs.OptionsSize),
                                                      ArrayRef<const char>(inputArgs.pInternalOptions, inputArgs.InternalOptionsSize));
            if (cache->loadCachedBinary(kernelFileHash, program)) {
                continue;
            }
        }

        auto inSrc = CIF::Builtins::CreateConstBuffer(fclMain.get(), inputArgs.pInput, inputArgs.InputSize);
        auto fclOptions = CIF::Builtins::CreateConstBuffer(fclMain.get(), inputArgs.pOptions, inputArgs.OptionsSize);
        auto fclInternalOptions = CIF::Builtins::CreateConstBuffer(fclMain.get(), inputArgs.pInternalOptions, inputArgs.InternalOptionsSize);

        CIF::RAII::UPtr_t<CIF::Builtins::BufferSimple> intermediateRepresentation;

        if (highLevelCodeType != IGC::CodeType::undefined) {
            auto fclTranslationCtx = createFclTranslationCtx(device, highLevelCodeType, intermediateCodeType);
            auto fclOutput = translate(fclTranslationCtx.get(), inSrc.get(),
                                       fclOptions.get(), fclInternalOptions.get());

            if (fclOutput == nullptr) {
                return CL_OUT_OF_HOST_MEMORY;
            }

            if (fclOutput->Successful() == false) {
                program.updateBuildLog(&device, fclOutput->GetBuildLog()->GetMemory<char>(), fclOutput->GetBuildLog()->GetSizeRaw());
                return CL_BUILD_PROGRAM_FAILURE;
            }

            program.storeIrBinary(fclOutput->GetOutput()->GetMemory<char>(), fclOutput->GetOutput()->GetSizeRaw(), intermediateCodeType == IGC::CodeType::spirV);
            program.updateBuildLog(&device, fclOutput->GetBuildLog()->GetMemory<char>(), fclOutput->GetBuildLog()->GetSizeRaw());

            fclOutput->GetOutput()->Retain(); // will be used as input to compiler
            intermediateRepresentation.reset(fclOutput->GetOutput());
        } else {
            inSrc->Retain(); // will be used as input to compiler directly
            intermediateRepresentation.reset(inSrc.get());
        }

        if (cachingMode == CachingMode::PreProcess) {
            kernelFileHash = cache->getCachedFileName(device.getHardwareInfo(), ArrayRef<const char>(intermediateRepresentation->GetMemory<char>(), intermediateRepresentation->GetSize<char>()),
                                                      ArrayRef<const char>(fclOptions->GetMemory<char>(), fclOptions->GetSize<char>()),
                                                      ArrayRef<const char>(fclInternalOptions->GetMemory<char>(), fclInternalOptions->GetSize<char>()));
            binaryLoaded = cache->loadCachedBinary(kernelFileHash, program);
        }
        if (!binaryLoaded) {
            auto igcTranslationCtx = createIgcTranslationCtx(device, intermediateCodeType, IGC::CodeType::oclGenBin);

            auto igcOutput = translate(igcTranslationCtx.get(), intermediateRepresentation.get(),
                                       fclOptions.get(), fclInternalOptions.get(), inputArgs.GTPinInput);

            if (igcOutput == nullptr) {
                return CL_OUT_OF_HOST_MEMORY;
            }

            if (igcOutput->Successful() == false) {
                program.updateBuildLog(&device, igcOutput->GetBuildLog()->GetMemory<char>(), igcOutput->GetBuildLog()->GetSizeRaw());
                return CL_BUILD_PROGRAM_FAILURE;
            }

            if (enableCaching) {
                cache->cacheBinary(kernelFileHash, igcOutput->GetOutput()->GetMemory<char>(), static_cast<uint32_t>(igcOutput->GetOutput()->GetSizeRaw()));
            }

            program.storeGenBinary(igcOutput->GetOutput()->GetMemory<char>(), igcOutput->GetOutput()->GetSizeRaw());
            program.updateBuildLog(&device, igcOutput->GetBuildLog()->GetMemory<char>(), igcOutput->GetBuildLog()->GetSizeRaw());
            if (igcOutput->GetDebugData()->GetSizeRaw() != 0) {
                program.storeDebugData(igcOutput->GetDebugData()->GetMemory<char>(), igcOutput->GetDebugData()->GetSizeRaw());
            }
        }
    }

    return CL_SUCCESS;
}

cl_int CompilerInterface::compile(
    Program &program,
    const TranslationArgs &inputArgs) {
    if (false == isCompilerAvailable()) {
        return CL_COMPILER_NOT_AVAILABLE;
    }

    IGC::CodeType::CodeType_t inType = IGC::CodeType::undefined;
    IGC::CodeType::CodeType_t outType = IGC::CodeType::undefined;

    bool fromIntermediate = (program.getProgramBinaryType() == CL_PROGRAM_BINARY_TYPE_INTERMEDIATE);

    if (fromIntermediate == false) {
        inType = IGC::CodeType::elf;
        if (useLlvmText == true) {
            outType = IGC::CodeType::llvmLl;
        }
    }

    uint32_t numDevices = static_cast<uint32_t>(program.getNumDevices());
    for (uint32_t i = 0; i < numDevices; i++) {
        const auto &device = program.getDevice(i);
        if (outType == IGC::CodeType::undefined) {
            outType = getPreferredIntermediateRepresentation(device);
        }

        if (fromIntermediate == false) {
            auto fclSrc = CIF::Builtins::CreateConstBuffer(fclMain.get(), inputArgs.pInput, inputArgs.InputSize);
            auto fclOptions = CIF::Builtins::CreateConstBuffer(fclMain.get(), inputArgs.pOptions, inputArgs.OptionsSize);
            auto fclInternalOptions = CIF::Builtins::CreateConstBuffer(fclMain.get(), inputArgs.pInternalOptions, inputArgs.InternalOptionsSize);

            auto fclTranslationCtx = createFclTranslationCtx(device, inType, outType);

            auto fclOutput = translate(fclTranslationCtx.get(), fclSrc.get(),
                                       fclOptions.get(), fclInternalOptions.get());

            if (fclOutput == nullptr) {
                return CL_OUT_OF_HOST_MEMORY;
            }

            if (fclOutput->Successful() == false) {
                program.updateBuildLog(&device, fclOutput->GetBuildLog()->GetMemory<char>(), fclOutput->GetBuildLog()->GetSizeRaw());
                return CL_COMPILE_PROGRAM_FAILURE;
            }

            program.storeIrBinary(fclOutput->GetOutput()->GetMemory<char>(), fclOutput->GetOutput()->GetSizeRaw(), outType == IGC::CodeType::spirV);
            program.updateBuildLog(&device, fclOutput->GetBuildLog()->GetMemory<char>(), fclOutput->GetBuildLog()->GetSizeRaw());
        } else {
            char *pOutput;
            uint32_t OutputSize;
            program.getSource(pOutput, OutputSize);
            program.storeIrBinary(pOutput, OutputSize, program.getIsSpirV());
        }
    }

    return CL_SUCCESS;
}

cl_int CompilerInterface::link(
    Program &program,
    const TranslationArgs &inputArgs) {
    if (false == isCompilerAvailable()) {
        return CL_COMPILER_NOT_AVAILABLE;
    }

    uint32_t numDevices = static_cast<uint32_t>(program.getNumDevices());
    for (uint32_t i = 0; i < numDevices; i++) {
        const auto &device = program.getDevice(i);

        auto inSrc = CIF::Builtins::CreateConstBuffer(igcMain.get(), inputArgs.pInput, inputArgs.InputSize);
        auto igcOptions = CIF::Builtins::CreateConstBuffer(igcMain.get(), inputArgs.pOptions, inputArgs.OptionsSize);
        auto igcInternalOptions = CIF::Builtins::CreateConstBuffer(igcMain.get(), inputArgs.pInternalOptions, inputArgs.InternalOptionsSize);

        if (inSrc == nullptr) {
            return CL_OUT_OF_HOST_MEMORY;
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
                return CL_OUT_OF_HOST_MEMORY;
            }

            if (currOut->Successful() == false) {
                program.updateBuildLog(&device, currOut->GetBuildLog()->GetMemory<char>(), currOut->GetBuildLog()->GetSizeRaw());
                return CL_BUILD_PROGRAM_FAILURE;
            }

            currOut->GetOutput()->Retain(); // shared with currSrc
            currSrc.reset(currOut->GetOutput());
        }

        program.storeGenBinary(currOut->GetOutput()->GetMemory<char>(), currOut->GetOutput()->GetSizeRaw());
        program.updateBuildLog(&device, currOut->GetBuildLog()->GetMemory<char>(), currOut->GetBuildLog()->GetSizeRaw());

        if (currOut->GetDebugData()->GetSizeRaw() != 0) {
            program.storeDebugData(currOut->GetDebugData()->GetMemory<char>(), currOut->GetDebugData()->GetSizeRaw());
        }
    }

    return CL_SUCCESS;
}

cl_int CompilerInterface::createLibrary(
    Program &program,
    const TranslationArgs &inputArgs) {
    if (false == isCompilerAvailable()) {
        return CL_COMPILER_NOT_AVAILABLE;
    }

    uint32_t numDevices = static_cast<uint32_t>(program.getNumDevices());
    for (uint32_t i = 0; i < numDevices; i++) {
        const auto &device = program.getDevice(i);

        auto igcSrc = CIF::Builtins::CreateConstBuffer(igcMain.get(), inputArgs.pInput, inputArgs.InputSize);
        auto igcOptions = CIF::Builtins::CreateConstBuffer(igcMain.get(), inputArgs.pOptions, inputArgs.OptionsSize);
        auto igcInternalOptions = CIF::Builtins::CreateConstBuffer(igcMain.get(), inputArgs.pInternalOptions, inputArgs.InternalOptionsSize);

        auto intermediateRepresentation = IGC::CodeType::llvmBc;
        auto igcTranslationCtx = createIgcTranslationCtx(device, IGC::CodeType::elf, intermediateRepresentation);

        auto igcOutput = translate(igcTranslationCtx.get(), igcSrc.get(),
                                   igcOptions.get(), igcInternalOptions.get());

        if (igcOutput == nullptr) {
            return CL_OUT_OF_HOST_MEMORY;
        }

        if (igcOutput->Successful() == false) {
            program.updateBuildLog(&device, igcOutput->GetBuildLog()->GetMemory<char>(), igcOutput->GetBuildLog()->GetSizeRaw());
            return CL_BUILD_PROGRAM_FAILURE;
        }

        program.storeIrBinary(igcOutput->GetOutput()->GetMemory<char>(), igcOutput->GetOutput()->GetSizeRaw(), intermediateRepresentation == IGC::CodeType::spirV);
        program.updateBuildLog(&device, igcOutput->GetBuildLog()->GetMemory<char>(), igcOutput->GetBuildLog()->GetSizeRaw());
    }

    return CL_SUCCESS;
}

cl_int CompilerInterface::getSipKernelBinary(SipKernelType kernel, const Device &device, std::vector<char> &retBinary) {
    if (false == isCompilerAvailable()) {
        return CL_COMPILER_NOT_AVAILABLE;
    }

    const char *sipSrc = getSipLlSrc(device);
    std::string sipInternalOptions = getSipKernelCompilerInternalOptions(kernel);

    auto igcSrc = CIF::Builtins::CreateConstBuffer(igcMain.get(), sipSrc, strlen(sipSrc) + 1);
    auto igcOptions = CIF::Builtins::CreateConstBuffer(igcMain.get(), nullptr, 0);
    auto igcInternalOptions = CIF::Builtins::CreateConstBuffer(igcMain.get(), sipInternalOptions.c_str(), sipInternalOptions.size() + 1);

    auto igcTranslationCtx = createIgcTranslationCtx(device, IGC::CodeType::llvmLl, IGC::CodeType::oclGenBin);

    auto igcOutput = translate(igcTranslationCtx.get(), igcSrc.get(),
                               igcOptions.get(), igcInternalOptions.get());

    if (igcOutput == nullptr) {
        return CL_OUT_OF_HOST_MEMORY;
    }

    if (igcOutput->Successful() == false) {
        return CL_BUILD_PROGRAM_FAILURE;
    }

    retBinary.assign(igcOutput->GetOutput()->GetMemory<char>(), igcOutput->GetOutput()->GetMemory<char>() + igcOutput->GetOutput()->GetSizeRaw());
    return CL_SUCCESS;
}

bool CompilerInterface::initialize() {
    bool compilersModulesSuccessfulyLoaded = true;
    compilersModulesSuccessfulyLoaded &= OCLRT::loadCompiler<IGC::FclOclDeviceCtx>(Os::frontEndDllName, fclLib, fclMain);
    compilersModulesSuccessfulyLoaded &= OCLRT::loadCompiler<IGC::IgcOclDeviceCtx>(Os::igcDllName, igcLib, igcMain);

    cache.reset(new BinaryCache());

    return compilersModulesSuccessfulyLoaded;
}

BinaryCache *CompilerInterface::replaceBinaryCache(BinaryCache *newCache) {
    auto res = cache.release();
    this->cache.reset(newCache);

    return res;
}

IGC::FclOclDeviceCtxTagOCL *CompilerInterface::getFclDeviceCtx(const Device &device) {
    auto it = fclDeviceContexts.find(&device);
    if (it != fclDeviceContexts.end()) {
        return it->second.get();
    }

    {
        auto ulock = this->lock();
        it = fclDeviceContexts.find(&device);
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
        fclBaseTranslationCtx = fclDeviceContexts[&device]->CreateTranslationCtx(inType, outType);
    }

    return deviceCtx->CreateTranslationCtx(inType, outType);
}

CIF::RAII::UPtr_t<IGC::IgcOclTranslationCtxTagOCL> CompilerInterface::createIgcTranslationCtx(const Device &device, IGC::CodeType::CodeType_t inType, IGC::CodeType::CodeType_t outType) {
    auto it = igcDeviceContexts.find(&device);
    if (it != igcDeviceContexts.end()) {
        return it->second->CreateTranslationCtx(inType, outType);
    }

    {
        auto ulock = this->lock();
        it = igcDeviceContexts.find(&device);
        if (it != igcDeviceContexts.end()) {
            return it->second->CreateTranslationCtx(inType, outType);
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
        if (false == OCLRT::areNotNullptr(igcPlatform.get(), igcGtSystemInfo.get(), igcFeWa.get())) {
            DEBUG_BREAK_IF(true); // could not acquire handles to device descriptors
            return nullptr;
        }
        const HardwareInfo *hwInfo = &device.getHardwareInfo();
        auto productFamily = DebugManager.flags.ForceCompilerUsePlatform.get();
        if (productFamily != "unk") {
            getHwInfoForPlatformString(productFamily.c_str(), hwInfo);
        }
        IGC::PlatformHelper::PopulateInterfaceWith(*igcPlatform, *hwInfo->pPlatform);
        IGC::GtSysInfoHelper::PopulateInterfaceWith(*igcGtSystemInfo, *hwInfo->pSysInfo);

        igcFeWa.get()->SetFtrDesktop(device.getHardwareInfo().pSkuTable->ftrDesktop);
        igcFeWa.get()->SetFtrChannelSwizzlingXOREnabled(device.getHardwareInfo().pSkuTable->ftrChannelSwizzlingXOREnabled);

        igcFeWa.get()->SetFtrGtBigDie(device.getHardwareInfo().pSkuTable->ftrGtBigDie);
        igcFeWa.get()->SetFtrGtMediumDie(device.getHardwareInfo().pSkuTable->ftrGtMediumDie);
        igcFeWa.get()->SetFtrGtSmallDie(device.getHardwareInfo().pSkuTable->ftrGtSmallDie);

        igcFeWa.get()->SetFtrGT1(device.getHardwareInfo().pSkuTable->ftrGT1);
        igcFeWa.get()->SetFtrGT1_5(device.getHardwareInfo().pSkuTable->ftrGT1_5);
        igcFeWa.get()->SetFtrGT2(device.getHardwareInfo().pSkuTable->ftrGT2);
        igcFeWa.get()->SetFtrGT3(device.getHardwareInfo().pSkuTable->ftrGT3);
        igcFeWa.get()->SetFtrGT4(device.getHardwareInfo().pSkuTable->ftrGT4);

        igcFeWa.get()->SetFtrIVBM0M1Platform(device.getHardwareInfo().pSkuTable->ftrIVBM0M1Platform);
        igcFeWa.get()->SetFtrGTL(device.getHardwareInfo().pSkuTable->ftrGT1);
        igcFeWa.get()->SetFtrGTM(device.getHardwareInfo().pSkuTable->ftrGT2);
        igcFeWa.get()->SetFtrGTH(device.getHardwareInfo().pSkuTable->ftrGT3);

        igcFeWa.get()->SetFtrSGTPVSKUStrapPresent(device.getHardwareInfo().pSkuTable->ftrSGTPVSKUStrapPresent);
        igcFeWa.get()->SetFtrGTA(device.getHardwareInfo().pSkuTable->ftrGTA);
        igcFeWa.get()->SetFtrGTC(device.getHardwareInfo().pSkuTable->ftrGTC);
        igcFeWa.get()->SetFtrGTX(device.getHardwareInfo().pSkuTable->ftrGTX);
        igcFeWa.get()->SetFtr5Slice(device.getHardwareInfo().pSkuTable->ftr5Slice);

        igcFeWa.get()->SetFtrGpGpuMidThreadLevelPreempt(device.getHardwareInfo().pSkuTable->ftrGpGpuMidThreadLevelPreempt);
        igcFeWa.get()->SetFtrIoMmuPageFaulting(device.getHardwareInfo().pSkuTable->ftrIoMmuPageFaulting);
        igcFeWa.get()->SetFtrWddm2Svm(device.getHardwareInfo().pSkuTable->ftrWddm2Svm);
        igcFeWa.get()->SetFtrPooledEuEnabled(device.getHardwareInfo().pSkuTable->ftrPooledEuEnabled);

        igcFeWa.get()->SetFtrResourceStreamer(device.getHardwareInfo().pSkuTable->ftrResourceStreamer);

        igcDeviceContexts[&device] = std::move(newDeviceCtx);
        return igcDeviceContexts[&device]->CreateTranslationCtx(inType, outType);
    }
}

} // namespace OCLRT
