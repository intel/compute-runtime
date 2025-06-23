/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_igc_facade.h"

#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/source/compiler_interface/igc_platform_helper.h"
#include "shared/source/compiler_interface/os_compiler_cache_helper.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
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
    if (initialized) {
        return OCLOC_SUCCESS;
    }

    auto compilerProductHelper = NEO::CompilerProductHelper::create(hwInfo.platform.eProductFamily);
    igcLib = loadIgcLibrary(compilerProductHelper ? compilerProductHelper->getCustomIgcLibraryName() : nullptr);
    if (!igcLib) {
        argHelper->printf("Error! Loading of IGC library has failed! Filename: %s\n", (compilerProductHelper && compilerProductHelper->getCustomIgcLibraryName()) ? compilerProductHelper->getCustomIgcLibraryName() : Os::igcDllName);
        return OCLOC_OUT_OF_HOST_MEMORY;
    }

    std::string igcPath = igcLib->getFullPath();
    igcLibSize = NEO::getFileSize(igcPath);
    igcLibMTime = NEO::getFileModificationTime(igcPath);

    const auto igcCreateMainFunction = loadCreateIgcMainFunction();
    if (!igcCreateMainFunction) {
        argHelper->printf("Error! Cannot load required functions from IGC library.\n");
        return OCLOC_OUT_OF_HOST_MEMORY;
    }

    igcMain = createIgcMain(igcCreateMainFunction);
    if (!igcMain) {
        argHelper->printf("Error! Cannot create IGC main component!\n");
        return OCLOC_OUT_OF_HOST_MEMORY;
    }

    const std::vector<CIF::InterfaceId_t> interfacesToIgnore = {IGC::OclGenBinaryBase::GetInterfaceId()};
    if (!isIgcInterfaceCompatible(interfacesToIgnore)) {
        const auto incompatibleInterface{getIncompatibleInterface(interfacesToIgnore)};
        argHelper->printf("Error! Incompatible interface in IGC: %s\n", incompatibleInterface.c_str());

        DEBUG_BREAK_IF(true);
        return OCLOC_OUT_OF_HOST_MEMORY;
    }

    if (!isPatchtokenInterfaceSupported()) {
        argHelper->printf("Error! Patchtoken interface is missing.\n");
        return OCLOC_OUT_OF_HOST_MEMORY;
    }

    {
        // revision is sha-1 hash
        igcRevision.resize(41);
        igcRevision[0] = '\0';
        auto igcDeviceCtx3 = createIgcDeviceContext3();
        if (igcDeviceCtx3) {
            const char *revision = igcDeviceCtx3->GetIGCRevision();
            strncpy_s(igcRevision.data(), 41, revision, 40);
        }
    }

    igcDeviceCtx = createIgcDeviceContext();
    if (!igcDeviceCtx) {
        argHelper->printf("Error! Cannot create IGC device context!\n");
        return OCLOC_OUT_OF_HOST_MEMORY;
    }

    igcDeviceCtx->SetProfilingTimerResolution(static_cast<float>(CommonConstants::defaultProfilingTimerResolution));

    const auto igcPlatform = getIgcPlatformHandle();
    const auto igcGtSystemInfo = getGTSystemInfoHandle();
    const auto igcFtrWa = getIgcFeaturesAndWorkaroundsHandle();

    if (!igcPlatform || !igcGtSystemInfo || !igcFtrWa) {
        argHelper->printf("Error! IGC device context has not been properly created!\n");
        return OCLOC_OUT_OF_HOST_MEMORY;
    }

    populateIgcPlatform(*igcPlatform, hwInfo);
    IGC::GtSysInfoHelper::PopulateInterfaceWith(*igcGtSystemInfo.get(), hwInfo.gtSystemInfo);

    populateWithFeatures(igcFtrWa.get(), hwInfo, compilerProductHelper.get());

    initialized = true;
    return OCLOC_SUCCESS;
}

std::unique_ptr<OsLibrary> OclocIgcFacade::loadIgcLibrary(const char *libName) const {
    auto effectiveLibName = libName ? libName : Os::igcDllName;
    return std::unique_ptr<OsLibrary>{OsLibrary::loadFunc({effectiveLibName})};
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

CIF::RAII::UPtr_t<IGC::IgcOclDeviceCtx<3>> OclocIgcFacade::createIgcDeviceContext3() const {
    return igcMain->CreateInterface<IGC::IgcOclDeviceCtx<3>>();
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

void OclocIgcFacade::populateWithFeatures(IGC::IgcFeaturesAndWorkaroundsTagOCL *handle, const HardwareInfo &hwInfo, const CompilerProductHelper *compilerProductHelper) const {
    if (compilerProductHelper) {
        handle->SetFtrGpGpuMidThreadLevelPreempt(compilerProductHelper->isMidThreadPreemptionSupported(hwInfo));
    }

    handle->SetFtrWddm2Svm(hwInfo.featureTable.flags.ftrWddm2Svm);
    handle->SetFtrPooledEuEnabled(hwInfo.featureTable.flags.ftrPooledEuEnabled);
}

const char *OclocIgcFacade::getIgcRevision() {
    return igcRevision.data();
}

size_t OclocIgcFacade::getIgcLibSize() {
    return igcLibSize;
}

time_t OclocIgcFacade::getIgcLibMTime() {
    return igcLibMTime;
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

OclocIgcAsFcl::OclocIgcAsFcl(OclocArgHelper *argHelper) : igc(std::make_unique<OclocIgcFacade>(argHelper)) {}
OclocIgcAsFcl::~OclocIgcAsFcl() = default;

int OclocIgcAsFcl::initialize(const HardwareInfo &hwInfo) {
    auto ret = igc->initialize(hwInfo);
    if (OCLOC_SUCCESS != ret) {
        return ret;
    }
    auto compilerProductHelper = NEO::CompilerProductHelper::create(hwInfo.platform.eProductFamily);
    if (!compilerProductHelper) {
        return OCLOC_INVALID_DEVICE;
    }
    this->preferredIntermediateRepresentation = compilerProductHelper->getPreferredIntermediateRepresentation();
    return OCLOC_SUCCESS;
}

bool OclocIgcAsFcl::isInitialized() const {
    return igc->isInitialized();
}
IGC::CodeType::CodeType_t OclocIgcAsFcl::getPreferredIntermediateRepresentation() const {
    return this->preferredIntermediateRepresentation;
}

CIF::RAII::UPtr_t<CIF::Builtins::BufferLatest> OclocIgcAsFcl::createConstBuffer(const void *data, size_t size) {
    return igc->createConstBuffer(data, size);
}

CIF::RAII::UPtr_t<IGC::OclTranslationOutputTagOCL> OclocIgcAsFcl::translate(IGC::CodeType::CodeType_t inType, IGC::CodeType::CodeType_t outType, CIF::Builtins::BufferLatest *error,
                                                                            CIF::Builtins::BufferSimple *src,
                                                                            CIF::Builtins::BufferSimple *options,
                                                                            CIF::Builtins::BufferSimple *internalOptions,
                                                                            CIF::Builtins::BufferSimple *tracingOptions,
                                                                            uint32_t tracingOptionsCount) {

    auto translationCtx = igc->createTranslationContext(inType, outType);

    if ((nullptr != error->GetMemory<char>()) || (nullptr == translationCtx)) {
        return nullptr;
    }

    return translationCtx->Translate(src, options, internalOptions, nullptr, 0);
}

} // namespace NEO
