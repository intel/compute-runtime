/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_fcl_facade.h"

#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/source/compiler_interface/igc_platform_helper.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/os_inc_base.h"
#include "shared/source/os_interface/os_library.h"

#include "ocl_igc_interface/igc_ocl_device_ctx.h"
#include "ocl_igc_interface/platform_helper.h"

namespace NEO {

CIF::CIFMain *createMainNoSanitize(CIF::CreateCIFMainFunc_t createFunc);

OclocFclFacade::OclocFclFacade(OclocArgHelper *argHelper)
    : argHelper{argHelper} {
}

OclocFclFacade::~OclocFclFacade() = default;

int OclocFclFacade::initialize(const HardwareInfo &hwInfo) {
    if (initialized) {
        return OCLOC_SUCCESS;
    }
    fclLib = loadFclLibrary();
    if (!fclLib) {
        argHelper->printf("Error! Loading of FCL library has failed! Filename: %s\n", Os::frontEndDllName);
        return OCLOC_OUT_OF_HOST_MEMORY;
    }

    const auto fclCreateMainFunction = loadCreateFclMainFunction();
    if (!fclCreateMainFunction) {
        argHelper->printf("Error! Cannot load required functions from FCL library.\n");
        return OCLOC_OUT_OF_HOST_MEMORY;
    }

    fclMain = createFclMain(fclCreateMainFunction);
    if (!fclMain) {
        argHelper->printf("Error! Cannot create FCL main component!\n");
        return OCLOC_OUT_OF_HOST_MEMORY;
    }

    if (!isFclInterfaceCompatible()) {
        const auto incompatibleInterface{getIncompatibleInterface()};
        argHelper->printf("Error! Incompatible interface in FCL: %s\n", incompatibleInterface.c_str());

        DEBUG_BREAK_IF(true);
        return OCLOC_OUT_OF_HOST_MEMORY;
    }

    fclDeviceCtx = createFclDeviceContext();
    if (!fclDeviceCtx) {
        argHelper->printf("Error! Cannot create FCL device context!\n");
        return OCLOC_OUT_OF_HOST_MEMORY;
    }

    fclDeviceCtx->SetOclApiVersion(hwInfo.capabilityTable.clVersionSupport * 10);

    if (shouldPopulateFclInterface()) {
        const auto platform = getPlatformHandle();
        if (!platform) {
            argHelper->printf("Error! FCL device context has not been properly created!\n");
            return OCLOC_OUT_OF_HOST_MEMORY;
        }

        populateFclInterface(*platform, hwInfo);
    }

    initialized = true;
    return OCLOC_SUCCESS;
}

std::unique_ptr<OsLibrary> OclocFclFacade::loadFclLibrary() const {
    return std::unique_ptr<OsLibrary>{OsLibrary::loadFunc({Os::frontEndDllName})};
}

CIF::CreateCIFMainFunc_t OclocFclFacade::loadCreateFclMainFunction() const {
    return reinterpret_cast<CIF::CreateCIFMainFunc_t>(fclLib->getProcAddress(CIF::CreateCIFMainFuncName));
}

CIF::RAII::UPtr_t<CIF::CIFMain> OclocFclFacade::createFclMain(CIF::CreateCIFMainFunc_t createMainFunction) const {
    return CIF::RAII::UPtr(createMainNoSanitize(createMainFunction));
}

bool OclocFclFacade::isFclInterfaceCompatible() const {
    return fclMain->IsCompatible<IGC::FclOclDeviceCtx>();
}

std::string OclocFclFacade::getIncompatibleInterface() const {
    return CIF::InterfaceIdCoder::Dec(fclMain->FindIncompatible<IGC::FclOclDeviceCtx>());
}

CIF::RAII::UPtr_t<IGC::FclOclDeviceCtxTagOCL> OclocFclFacade::createFclDeviceContext() const {
    return fclMain->CreateInterface<IGC::FclOclDeviceCtxTagOCL>();
}

bool OclocFclFacade::shouldPopulateFclInterface() const {
    return fclDeviceCtx->GetUnderlyingVersion() > 4U;
}

CIF::RAII::UPtr_t<IGC::PlatformTagOCL> OclocFclFacade::getPlatformHandle() const {
    return fclDeviceCtx->GetPlatformHandle();
}

void OclocFclFacade::populateFclInterface(IGC::PlatformTagOCL &handle, const HardwareInfo &hwInfo) {
    populateIgcPlatform(handle, hwInfo);
}

IGC::CodeType::CodeType_t OclocFclFacade::getPreferredIntermediateRepresentation() const {
    return fclDeviceCtx->GetPreferredIntermediateRepresentation();
}

CIF::RAII::UPtr_t<CIF::Builtins::BufferLatest> OclocFclFacade::createConstBuffer(const void *data, size_t size) {
    return CIF::Builtins::CreateConstBuffer(fclMain.get(), data, size);
}

CIF::RAII::UPtr_t<IGC::OclTranslationOutputTagOCL> OclocFclFacade::translate(IGC::CodeType::CodeType_t inType, IGC::CodeType::CodeType_t outType, CIF::Builtins::BufferLatest *error,
                                                                             CIF::Builtins::BufferSimple *src,
                                                                             CIF::Builtins::BufferSimple *options,
                                                                             CIF::Builtins::BufferSimple *internalOptions,
                                                                             CIF::Builtins::BufferSimple *tracingOptions,
                                                                             uint32_t tracingOptionsCount) {

    auto fclTranslationCtx = this->createTranslationContext(inType, outType, error);

    if ((nullptr != error->GetMemory<char>()) || (nullptr == fclTranslationCtx)) {
        return nullptr;
    }

    return fclTranslationCtx->Translate(src, options, internalOptions, nullptr, 0);
}

CIF::RAII::UPtr_t<IGC::FclOclTranslationCtxTagOCL> OclocFclFacade::createTranslationContext(IGC::CodeType::CodeType_t inType, IGC::CodeType::CodeType_t outType, CIF::Builtins::BufferLatest *error) {
    return fclDeviceCtx->CreateTranslationCtx(inType, outType, error);
}

bool OclocFclFacade::isInitialized() const {
    return initialized;
}

} // namespace NEO
