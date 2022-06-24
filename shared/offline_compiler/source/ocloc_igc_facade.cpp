/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_igc_facade.h"

#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/offline_compiler/source/ocloc_error_code.h"
#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/os_inc_base.h"
#include "shared/source/os_interface/os_library.h"

#include "ocl_igc_interface/platform_helper.h"

#include <vector>

namespace NEO {

CIF::CIFMain *createMainNoSanitize(CIF::CreateCIFMainFunc_t createFunc);

OclocIgcFacade::OclocIgcFacade(OclocArgHelper *argHelper)
    : argHelper{argHelper} {
}

OclocIgcFacade::~OclocIgcFacade() = default;

int OclocIgcFacade::initialize(const HardwareInfo &hwInfo) {
    igcLib = loadIgcLibrary();
    if (!igcLib) {
        argHelper->printf("Error! Loading of IGC library has failed! Filename: %s\n", Os::igcDllName);
        return OclocErrorCode::OUT_OF_HOST_MEMORY;
    }

    const auto igcCreateMainFunction = loadCreateIgcMainFunction();
    if (!igcCreateMainFunction) {
        argHelper->printf("Error! Cannot load required functions from IGC library.\n");
        return OclocErrorCode::OUT_OF_HOST_MEMORY;
    }

    igcMain = createIgcMain(igcCreateMainFunction);
    if (!igcMain) {
        argHelper->printf("Error! Cannot create IGC main component!\n");
        return OclocErrorCode::OUT_OF_HOST_MEMORY;
    }

    const std::vector<CIF::InterfaceId_t> interfacesToIgnore = {IGC::OclGenBinaryBase::GetInterfaceId()};
    if (!isIgcInterfaceCompatible(interfacesToIgnore)) {
        const auto incompatibleInterface{getIncompatibleInterface(interfacesToIgnore)};
        argHelper->printf("Error! Incompatible interface in IGC: %s\n", incompatibleInterface.c_str());

        DEBUG_BREAK_IF(true);
        return OclocErrorCode::OUT_OF_HOST_MEMORY;
    }

    if (!isPatchtokenInterfaceSupported()) {
        argHelper->printf("Error! Patchtoken interface is missing.\n");
        return OclocErrorCode::OUT_OF_HOST_MEMORY;
    }

    igcDeviceCtx = createIgcDeviceContext();
    if (!igcDeviceCtx) {
        argHelper->printf("Error! Cannot create IGC device context!\n");
        return OclocErrorCode::OUT_OF_HOST_MEMORY;
    }

    igcDeviceCtx->SetProfilingTimerResolution(static_cast<float>(hwInfo.capabilityTable.defaultProfilingTimerResolution));

    const auto igcPlatform = getIgcPlatformHandle();
    const auto igcGtSystemInfo = getGTSystemInfoHandle();
    const auto igcFtrWa = getIgcFeaturesAndWorkaroundsHandle();

    if (!igcPlatform || !igcGtSystemInfo || !igcFtrWa) {
        argHelper->printf("Error! IGC device context has not been properly created!\n");
        return OclocErrorCode::OUT_OF_HOST_MEMORY;
    }

    const auto compilerHwInfoConfig = CompilerHwInfoConfig::get(hwInfo.platform.eProductFamily);

    auto copyHwInfo = hwInfo;
    if (compilerHwInfoConfig) {
        compilerHwInfoConfig->adjustHwInfoForIgc(copyHwInfo);
    }

    IGC::PlatformHelper::PopulateInterfaceWith(*igcPlatform.get(), copyHwInfo.platform);
    IGC::GtSysInfoHelper::PopulateInterfaceWith(*igcGtSystemInfo.get(), copyHwInfo.gtSystemInfo);

    populateWithFeatures(igcFtrWa.get(), hwInfo, compilerHwInfoConfig);

    initialized = true;
    return OclocErrorCode::SUCCESS;
}

std::unique_ptr<OsLibrary> OclocIgcFacade::loadIgcLibrary() const {
    return std::unique_ptr<OsLibrary>{OsLibrary::load(Os::igcDllName)};
}

CIF::CreateCIFMainFunc_t OclocIgcFacade::loadCreateIgcMainFunction() const {
    return reinterpret_cast<CIF::CreateCIFMainFunc_t>(igcLib->getProcAddress(CIF::CreateCIFMainFuncName));
}

CIF::RAII::UPtr_t<CIF::CIFMain> OclocIgcFacade::createIgcMain(CIF::CreateCIFMainFunc_t createMainFunction) const {
    return CIF::RAII::UPtr(createMainNoSanitize(createMainFunction));
}

bool OclocIgcFacade::isIgcInterfaceCompatible(const std::vector<CIF::InterfaceId_t> &interfacesToIgnore) const {
    return igcMain->IsCompatible<IGC::IgcOclDeviceCtx>(&interfacesToIgnore);
}

std::string OclocIgcFacade::getIncompatibleInterface(const std::vector<CIF::InterfaceId_t> &interfacesToIgnore) const {
    return CIF::InterfaceIdCoder::Dec(igcMain->FindIncompatible<IGC::IgcOclDeviceCtx>(&interfacesToIgnore));
}

bool OclocIgcFacade::isPatchtokenInterfaceSupported() const {
    CIF::Version_t verMin = 0, verMax = 0;
    return igcMain->FindSupportedVersions<IGC::IgcOclDeviceCtx>(IGC::OclGenBinaryBase::GetInterfaceId(), verMin, verMax);
}

CIF::RAII::UPtr_t<IGC::IgcOclDeviceCtxTagOCL> OclocIgcFacade::createIgcDeviceContext() const {
    return igcMain->CreateInterface<IGC::IgcOclDeviceCtxTagOCL>();
}

CIF::RAII::UPtr_t<IGC::PlatformTagOCL> OclocIgcFacade::getIgcPlatformHandle() const {
    return igcDeviceCtx->GetPlatformHandle();
}

CIF::RAII::UPtr_t<IGC::GTSystemInfoTagOCL> OclocIgcFacade::getGTSystemInfoHandle() const {
    return igcDeviceCtx->GetGTSystemInfoHandle();
}

CIF::RAII::UPtr_t<IGC::IgcFeaturesAndWorkaroundsTagOCL> OclocIgcFacade::getIgcFeaturesAndWorkaroundsHandle() const {
    return igcDeviceCtx->GetIgcFeaturesAndWorkaroundsHandle();
}

void OclocIgcFacade::populateWithFeatures(IGC::IgcFeaturesAndWorkaroundsTagOCL *handle, const HardwareInfo &hwInfo, const CompilerHwInfoConfig *compilerHwInfoConfig) const {
    handle->SetFtrDesktop(hwInfo.featureTable.flags.ftrDesktop);
    handle->SetFtrChannelSwizzlingXOREnabled(hwInfo.featureTable.flags.ftrChannelSwizzlingXOREnabled);
    handle->SetFtrIVBM0M1Platform(hwInfo.featureTable.flags.ftrIVBM0M1Platform);
    handle->SetFtrSGTPVSKUStrapPresent(hwInfo.featureTable.flags.ftrSGTPVSKUStrapPresent);
    handle->SetFtr5Slice(hwInfo.featureTable.flags.ftr5Slice);

    if (compilerHwInfoConfig) {
        handle->SetFtrGpGpuMidThreadLevelPreempt(compilerHwInfoConfig->isMidThreadPreemptionSupported(hwInfo));
    }

    handle->SetFtrIoMmuPageFaulting(hwInfo.featureTable.flags.ftrIoMmuPageFaulting);
    handle->SetFtrWddm2Svm(hwInfo.featureTable.flags.ftrWddm2Svm);
    handle->SetFtrPooledEuEnabled(hwInfo.featureTable.flags.ftrPooledEuEnabled);

    handle->SetFtrResourceStreamer(hwInfo.featureTable.flags.ftrResourceStreamer);
}

CIF::RAII::UPtr_t<CIF::Builtins::BufferLatest> OclocIgcFacade::createConstBuffer(const void *data, size_t size) {
    return CIF::Builtins::CreateConstBuffer(igcMain.get(), data, size);
}

CIF::RAII::UPtr_t<IGC::IgcOclTranslationCtxTagOCL> OclocIgcFacade::createTranslationContext(IGC::CodeType::CodeType_t inType, IGC::CodeType::CodeType_t outType) {
    return igcDeviceCtx->CreateTranslationCtx(inType, outType);
}

bool OclocIgcFacade::isInitialized() const {
    return initialized;
}

} // namespace NEO
